#include <jni.h>
#include <android/log.h>
#include <string>
#include <memory>

#include <android/native_window_jni.h>

#include "video_playback.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
}

#define LOG_TAG "videolib.jni"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

JavaVM *gJvm = nullptr;

class PlaybackJniBridge {
public:
    PlaybackJniBridge(JNIEnv *env, jobject target) : vm_(gJvm) {
        if (env == nullptr || target == nullptr || vm_ == nullptr) {
            return;
        }
        target_ = env->NewGlobalRef(target);
        jclass targetClass = env->GetObjectClass(target);
        if (target_ != nullptr && targetClass != nullptr) {
            completedMethod_ = env->GetMethodID(
                    targetClass, "onNativePlaybackCompleted", "(J)V");
            errorMethod_ = env->GetMethodID(
                    targetClass, "onNativePlaybackError", "(JI)V");
        }
        if (targetClass != nullptr) {
            env->DeleteLocalRef(targetClass);
        }
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            completedMethod_ = nullptr;
            errorMethod_ = nullptr;
        }
    }

    ~PlaybackJniBridge() {
        if (target_ == nullptr || vm_ == nullptr) {
            return;
        }
        bool attached = false;
        JNIEnv *env = environment(&attached);
        if (env != nullptr) {
            env->DeleteGlobalRef(target_);
        }
        if (attached) {
            vm_->DetachCurrentThread();
        }
    }

    bool isValid() const {
        return target_ != nullptr && completedMethod_ != nullptr && errorMethod_ != nullptr;
    }

    void notify(uint64_t attemptId, std::optional<PlaybackErrorCode> error) const {
        bool attached = false;
        JNIEnv *env = environment(&attached);
        if (env == nullptr || !isValid()) {
            if (attached) {
                vm_->DetachCurrentThread();
            }
            return;
        }
        if (error.has_value()) {
            env->CallVoidMethod(
                    target_, errorMethod_, static_cast<jlong>(attemptId),
                    static_cast<jint>(*error));
        } else {
            env->CallVoidMethod(target_, completedMethod_, static_cast<jlong>(attemptId));
        }
        if (env->ExceptionCheck()) {
            LOGE("VideoPreview native callback raised an exception");
            env->ExceptionClear();
        }
        if (attached) {
            vm_->DetachCurrentThread();
        }
    }

private:
    JNIEnv *environment(bool *attached) const {
        *attached = false;
        JNIEnv *env = nullptr;
        const jint status = vm_->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
        if (status == JNI_OK) {
            return env;
        }
        if (status != JNI_EDETACHED ||
            vm_->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            return nullptr;
        }
        *attached = true;
        return env;
    }

    JavaVM *vm_ = nullptr;
    jobject target_ = nullptr;
    jmethodID completedMethod_ = nullptr;
    jmethodID errorMethod_ = nullptr;
};

static inline VideoPlayback *asPlayback(jlong handle) {
    return reinterpret_cast<VideoPlayback *>(handle);
}

} // namespace

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void * /* reserved */) {
    gJvm = vm;
    return JNI_VERSION_1_6;
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
// nativeHandle is an opaque pointer to a heap VideoPlayback owned by the
// Kotlin instance. VideoPlayback owns the renderer and decode worker.

extern "C" JNIEXPORT jlong JNICALL
Java_com_cii_videolib_VideoPreview_nativeCreate(JNIEnv* env, jobject thiz) {
    try {
        auto bridge = std::make_shared<PlaybackJniBridge>(env, thiz);
        if (!bridge->isValid()) {
            return 0;
        }
        auto *playback = new VideoPlayback(
                [bridge](uint64_t attemptId, std::optional<PlaybackErrorCode> error) {
                    bridge->notify(attemptId, error);
                });
        return reinterpret_cast<jlong>(playback);
    } catch (...) {
        return 0;
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_cii_videolib_VideoPreview_nativeSurfaceAvailable(
        JNIEnv* env, jobject /* this */, jlong handle, jobject surface) {
    VideoPlayback* playback = asPlayback(handle);
    if (playback == nullptr || surface == nullptr) {
        return JNI_FALSE;
    }
    // ANativeWindow_fromSurface adds a reference; PreviewRenderer owns it and
    // releases it exactly once in releaseSurface.
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (window == nullptr) {
        return JNI_FALSE;
    }
    return playback->surfaceAvailable(window) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_cii_videolib_VideoPreview_nativePlay(
        JNIEnv* env, jobject /* this */, jlong handle, jstring path) {
    VideoPlayback* playback = asPlayback(handle);
    if (playback == nullptr || path == nullptr) {
        return 0;
    }
    const char *pathChars = env->GetStringUTFChars(path, nullptr);
    if (pathChars == nullptr) {
        return 0;
    }
    std::string pathCopy;
    try {
        pathCopy.assign(pathChars);
    } catch (...) {
        env->ReleaseStringUTFChars(path, pathChars);
        return 0;
    }
    env->ReleaseStringUTFChars(path, pathChars);
    return static_cast<jlong>(playback->play(pathCopy));
}

extern "C" JNIEXPORT void JNICALL
Java_com_cii_videolib_VideoPreview_nativeStop(
        JNIEnv* env, jobject /* this */, jlong handle) {
    VideoPlayback* playback = asPlayback(handle);
    if (playback != nullptr) {
        playback->stop();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_cii_videolib_VideoPreview_nativePushFrame(
        JNIEnv* env, jobject /* this */, jlong handle, jobject frame, jint width, jint height) {
    VideoPlayback* playback = asPlayback(handle);
    if (playback == nullptr || frame == nullptr) {
        return;
    }
    // Zero-copy: requires a direct ByteBuffer (validated on the Kotlin side).
    // The buffer must outlive this synchronous call; the renderer copies the
    // pixels into a GL texture before returning.
    const auto* pixels = static_cast<const uint8_t*>(env->GetDirectBufferAddress(frame));
    if (pixels == nullptr) {
        return;
    }
    playback->pushFrame(pixels, width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_cii_videolib_VideoPreview_nativeRequestPattern(
        JNIEnv* env, jobject /* this */, jlong handle) {
    VideoPlayback* playback = asPlayback(handle);
    if (playback != nullptr) {
        playback->requestPattern();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_cii_videolib_VideoPreview_nativeReleaseSurface(
        JNIEnv* env, jobject /* this */, jlong handle) {
    VideoPlayback* playback = asPlayback(handle);
    if (playback != nullptr) {
        playback->releaseSurface();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_cii_videolib_VideoPreview_nativeDestroy(
        JNIEnv* env, jobject /* this */, jlong handle) {
    VideoPlayback* playback = asPlayback(handle);
    delete playback; // destructor stops decode, then releases EGL and callbacks
}
