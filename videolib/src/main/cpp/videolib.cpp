#include <jni.h>
#include <string>

#include <android/native_window_jni.h>

#include "preview_renderer.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_cii_videolib_NativeLib_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

// Returns the linked FFmpeg version, proving the static archives are actually
// linked into libvideolib.so. Referencing av_version_info() pulls in libavutil
// and avformat_version() pulls in libavformat, so the linker cannot strip them.
extern "C" JNIEXPORT jstring JNICALL
Java_com_cii_videolib_NativeLib_nativeFFmpegVersion(
        JNIEnv* env,
        jobject /* this */) {
    unsigned fmt = avformat_version();
    std::string info = std::string(av_version_info())
            + " (avformat "
            + std::to_string(AV_VERSION_MAJOR(fmt)) + "."
            + std::to_string(AV_VERSION_MINOR(fmt)) + "."
            + std::to_string(AV_VERSION_MICRO(fmt)) + ")";
    return env->NewStringUTF(info.c_str());
}

// --- OpenGL ES preview (VideoPreview) ----------------------------------------
// Each export maps 1:1 to a com.cii.videolib.VideoPreview `external fun`; the
// mangled name must stay in exact sync with the Kotlin class/method names.
// nativeHandle is an opaque pointer to a heap PreviewRenderer owned by the
// Kotlin instance (created in nativeCreate, freed in nativeDestroy).

static inline PreviewRenderer *asRenderer(jlong handle) {
    return reinterpret_cast<PreviewRenderer *>(handle);
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_cii_videolib_VideoPreview_nativeCreate(JNIEnv* env, jobject /* this */) {
    return reinterpret_cast<jlong>(new PreviewRenderer());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_cii_videolib_VideoPreview_nativeSurfaceAvailable(
        JNIEnv* env, jobject /* this */, jlong handle, jobject surface) {
    PreviewRenderer* renderer = asRenderer(handle);
    if (renderer == nullptr || surface == nullptr) {
        return JNI_FALSE;
    }
    // ANativeWindow_fromSurface adds a reference; PreviewRenderer owns it and
    // releases it exactly once in releaseSurface.
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (window == nullptr) {
        return JNI_FALSE;
    }
    return renderer->surfaceAvailable(window) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_cii_videolib_VideoPreview_nativePushFrame(
        JNIEnv* env, jobject /* this */, jlong handle, jobject frame, jint width, jint height) {
    PreviewRenderer* renderer = asRenderer(handle);
    if (renderer == nullptr || frame == nullptr) {
        return;
    }
    // Zero-copy: requires a direct ByteBuffer (validated on the Kotlin side).
    // The buffer must outlive this synchronous call; the renderer copies the
    // pixels into a GL texture before returning.
    const auto* pixels = static_cast<const uint8_t*>(env->GetDirectBufferAddress(frame));
    if (pixels == nullptr) {
        return;
    }
    renderer->pushFrame(pixels, width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_cii_videolib_VideoPreview_nativeRequestPattern(
        JNIEnv* env, jobject /* this */, jlong handle) {
    PreviewRenderer* renderer = asRenderer(handle);
    if (renderer != nullptr) {
        renderer->requestPattern();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_cii_videolib_VideoPreview_nativeReleaseSurface(
        JNIEnv* env, jobject /* this */, jlong handle) {
    PreviewRenderer* renderer = asRenderer(handle);
    if (renderer != nullptr) {
        renderer->releaseSurface();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_cii_videolib_VideoPreview_nativeDestroy(
        JNIEnv* env, jobject /* this */, jlong handle) {
    PreviewRenderer* renderer = asRenderer(handle);
    delete renderer; // destructor releases surface + drains the render thread
}
