#include <jni.h>
#include <android/log.h>
#include <string>
#include <memory>
#include <limits>
#include <vector>

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

jobject newAppearanceResult(JNIEnv *env, const AppearanceApplyResult &result) {
    jclass resultClass = env->FindClass("com/cii/videolib/NativeAppearanceResult");
    if (resultClass == nullptr) return nullptr;
    jmethodID constructor = env->GetMethodID(
            resultClass, "<init>", "(ILjava/lang/String;)V");
    if (constructor == nullptr) {
        env->DeleteLocalRef(resultClass);
        return nullptr;
    }
    jstring diagnostic = result.diagnostic.empty()
            ? nullptr
            : env->NewStringUTF(result.diagnostic.c_str());
    jobject object = env->NewObject(
            resultClass, constructor, static_cast<jint>(result.error), diagnostic);
    if (diagnostic != nullptr) env->DeleteLocalRef(diagnostic);
    env->DeleteLocalRef(resultClass);
    return object;
}

AppearanceApplyResult readAppearance(
        JNIEnv *env,
        jfloatArray adjustmentValues,
        jint filterVersion,
        jstring filterSource,
        jfloat filterOpacity,
        jintArray textureWidths,
        jintArray textureHeights,
        jobjectArray textureBytes,
        AppearanceSnapshot *appearance) {
    if (adjustmentValues == nullptr || env->GetArrayLength(adjustmentValues) != 16) {
        return AppearanceApplyResult::failure(AppearanceError::InvalidValue);
    }
    jfloat values[16] = {};
    env->GetFloatArrayRegion(adjustmentValues, 0, 16, values);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return AppearanceApplyResult::failure(AppearanceError::InvalidValue);
    }
    appearance->adjustments = {
        values[0], values[1], values[2], values[3], values[4], values[5],
        values[6], values[7], values[8], values[9], values[10], values[11],
        values[12], values[13], values[14], values[15],
    };
    if (filterVersion == 0) {
        if (filterSource != nullptr ||
            (textureWidths != nullptr && env->GetArrayLength(textureWidths) != 0) ||
            (textureHeights != nullptr && env->GetArrayLength(textureHeights) != 0) ||
            (textureBytes != nullptr && env->GetArrayLength(textureBytes) != 0)) {
            return AppearanceApplyResult::failure(AppearanceError::InvalidFilterTexture);
        }
        appearance->filter.reset();
        return AppearanceApplyResult::success();
    }
    if (filterSource == nullptr || textureWidths == nullptr ||
        textureHeights == nullptr || textureBytes == nullptr) {
        return AppearanceApplyResult::failure(AppearanceError::InvalidFilterSource);
    }
    const jsize count = env->GetArrayLength(textureWidths);
    if (env->GetArrayLength(textureHeights) != count ||
        env->GetArrayLength(textureBytes) != count) {
        return AppearanceApplyResult::failure(AppearanceError::InvalidFilterTexture);
    }

    const char *sourceChars = env->GetStringUTFChars(filterSource, nullptr);
    if (sourceChars == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return AppearanceApplyResult::failure(AppearanceError::ResourceAllocation);
    }
    FilterDescriptor filter;
    try {
        filter.version = filterVersion;
        filter.source.assign(sourceChars);
        filter.opacity = filterOpacity;
        filter.textures.reserve(static_cast<size_t>(count));
    } catch (...) {
        env->ReleaseStringUTFChars(filterSource, sourceChars);
        return AppearanceApplyResult::failure(AppearanceError::ResourceAllocation);
    }
    env->ReleaseStringUTFChars(filterSource, sourceChars);

    std::vector<jint> widths;
    std::vector<jint> heights;
    try {
        widths.resize(static_cast<size_t>(count));
        heights.resize(static_cast<size_t>(count));
    } catch (...) {
        return AppearanceApplyResult::failure(AppearanceError::ResourceAllocation);
    }
    env->GetIntArrayRegion(textureWidths, 0, count, widths.data());
    env->GetIntArrayRegion(textureHeights, 0, count, heights.data());
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return AppearanceApplyResult::failure(AppearanceError::InvalidFilterTexture);
    }
    for (jsize i = 0; i < count; ++i) {
        auto bytes = static_cast<jbyteArray>(env->GetObjectArrayElement(textureBytes, i));
        if (bytes == nullptr || widths[static_cast<size_t>(i)] <= 0 ||
            heights[static_cast<size_t>(i)] <= 0) {
            if (bytes != nullptr) env->DeleteLocalRef(bytes);
            return AppearanceApplyResult::failure(AppearanceError::InvalidFilterTexture);
        }
        const uint64_t expected = static_cast<uint64_t>(widths[static_cast<size_t>(i)]) *
                                  static_cast<uint64_t>(heights[static_cast<size_t>(i)]) * 4U;
        if (expected > static_cast<uint64_t>(std::numeric_limits<jsize>::max()) ||
            env->GetArrayLength(bytes) != static_cast<jsize>(expected)) {
            env->DeleteLocalRef(bytes);
            return AppearanceApplyResult::failure(AppearanceError::InvalidFilterTexture);
        }
        FilterTexture texture;
        try {
            texture.width = widths[static_cast<size_t>(i)];
            texture.height = heights[static_cast<size_t>(i)];
            texture.rgba8888.resize(static_cast<size_t>(expected));
        } catch (...) {
            env->DeleteLocalRef(bytes);
            return AppearanceApplyResult::failure(AppearanceError::ResourceAllocation);
        }
        env->GetByteArrayRegion(
                bytes, 0, static_cast<jsize>(expected),
                reinterpret_cast<jbyte *>(texture.rgba8888.data()));
        env->DeleteLocalRef(bytes);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            return AppearanceApplyResult::failure(AppearanceError::InvalidFilterTexture);
        }
        try {
            filter.textures.push_back(std::move(texture));
        } catch (...) {
            return AppearanceApplyResult::failure(AppearanceError::ResourceAllocation);
        }
    }
    appearance->filter = std::move(filter);
    return AppearanceApplyResult::success();
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

extern "C" JNIEXPORT jboolean JNICALL
Java_com_cii_videolib_VideoPreview_nativePause(
        JNIEnv* env, jobject /* this */, jlong handle) {
    VideoPlayback* playback = asPlayback(handle);
    return playback != nullptr && playback->pause() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_cii_videolib_VideoPreview_nativeResume(
        JNIEnv* env, jobject /* this */, jlong handle) {
    VideoPlayback* playback = asPlayback(handle);
    return playback != nullptr && playback->resume() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_cii_videolib_VideoPreview_nativeSetLooping(
        JNIEnv* env, jobject /* this */, jlong handle, jboolean enabled) {
    VideoPlayback* playback = asPlayback(handle);
    return playback != nullptr && playback->setLooping(enabled == JNI_TRUE)
           ? JNI_TRUE
           : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_cii_videolib_VideoPreview_nativeSetPlaybackSpeed(
        JNIEnv* env, jobject /* this */, jlong handle, jdouble speed) {
    VideoPlayback* playback = asPlayback(handle);
    return playback != nullptr && playback->setPlaybackSpeed(static_cast<double>(speed))
           ? JNI_TRUE
           : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_cii_videolib_VideoPreview_nativeSeekTo(
        JNIEnv* env, jobject /* this */, jlong handle, jlong positionMs) {
    VideoPlayback* playback = asPlayback(handle);
    return playback != nullptr && playback->seekTo(static_cast<int64_t>(positionMs))
           ? JNI_TRUE
           : JNI_FALSE;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_cii_videolib_VideoPreview_nativeApplyAppearance(
        JNIEnv* env,
        jobject /* this */,
        jlong handle,
        jfloatArray adjustments,
        jint filterVersion,
        jstring filterSource,
        jfloat filterOpacity,
        jintArray textureWidths,
        jintArray textureHeights,
        jobjectArray textureBytes) {
    VideoPlayback *playback = asPlayback(handle);
    if (playback == nullptr) {
        return newAppearanceResult(
                env, AppearanceApplyResult::failure(AppearanceError::Released));
    }
    AppearanceSnapshot appearance;
    AppearanceApplyResult conversion = readAppearance(
            env, adjustments, filterVersion, filterSource, filterOpacity,
            textureWidths, textureHeights, textureBytes, &appearance);
    if (!conversion.accepted()) return newAppearanceResult(env, conversion);
    return newAppearanceResult(env, playback->applyAppearance(appearance));
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
