#include <jni.h>
#include <android/log.h>
#include <string>
#include <memory>
#include <limits>
#include <vector>

#include <android/native_window_jni.h>

#include "video_playback.h"
#include "video_export.h"

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
                timelineCompletedMethod_ = env->GetMethodID(
                        targetClass, "onNativeTimelineCompleted", "(J)V");
                timelineErrorMethod_ = env->GetMethodID(
                        targetClass, "onNativeTimelineError", "(JII)V");
            }
            if (targetClass != nullptr) {
                env->DeleteLocalRef(targetClass);
            }
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                completedMethod_ = nullptr;
                errorMethod_ = nullptr;
                timelineCompletedMethod_ = nullptr;
                timelineErrorMethod_ = nullptr;
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
            return target_ != nullptr && completedMethod_ != nullptr &&
                   errorMethod_ != nullptr && timelineCompletedMethod_ != nullptr &&
                   timelineErrorMethod_ != nullptr;
        }

        // Routes the single terminal event to the callback family that matches
        // the attempt kind. Single-clip attempts use the pre-existing
        // onNativePlayback* callbacks (segmentIndex is ignored); timeline
        // attempts use onNativeTimeline*, carrying the failing segment index.
        void notify(uint64_t attemptId, PlaybackKind kind,
                    std::optional<PlaybackErrorCode> error, int segmentIndex) const {
            bool attached = false;
            JNIEnv *env = environment(&attached);
            if (env == nullptr || !isValid()) {
                if (attached) {
                    vm_->DetachCurrentThread();
                }
                return;
            }
            if (kind == PlaybackKind::Timeline) {
                if (error.has_value()) {
                    env->CallVoidMethod(
                            target_, timelineErrorMethod_, static_cast<jlong>(attemptId),
                            static_cast<jint>(*error), static_cast<jint>(segmentIndex));
                } else {
                    env->CallVoidMethod(
                            target_, timelineCompletedMethod_, static_cast<jlong>(attemptId));
                }
            } else if (error.has_value()) {
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
        jmethodID timelineCompletedMethod_ = nullptr;
        jmethodID timelineErrorMethod_ = nullptr;
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

    // Marshals the parallel per-segment arrays into a TimelineSegment vector.
    // Returns true on success; on any structural error clears any pending JNI
    // exception and returns false so the caller rejects the whole request
    // before starting an attempt (fail-fast, no partial timeline). Each
    // segment's appearance is read through the same readAppearance path used by
    // single-clip apply, so filter/adjustment validation stays identical.
    bool readTimelineSegments(
            JNIEnv *env,
            jobjectArray paths,
            jlongArray startsMs,
            jlongArray endsMs,
            jdoubleArray speeds,
            jobjectArray adjustments,
            jintArray filterVersions,
            jobjectArray filterSources,
            jfloatArray filterOpacities,
            jobjectArray textureWidths,
            jobjectArray textureHeights,
            jobjectArray textureBytes,
            std::vector<TimelineSegment> *segments) {
        if (paths == nullptr || startsMs == nullptr || endsMs == nullptr ||
            speeds == nullptr || adjustments == nullptr || filterVersions == nullptr ||
            filterSources == nullptr || filterOpacities == nullptr ||
            textureWidths == nullptr || textureHeights == nullptr ||
            textureBytes == nullptr) {
            return false;
        }
        const jsize count = env->GetArrayLength(paths);
        if (count <= 0 ||
            env->GetArrayLength(startsMs) != count ||
            env->GetArrayLength(endsMs) != count ||
            env->GetArrayLength(speeds) != count ||
            env->GetArrayLength(adjustments) != count ||
            env->GetArrayLength(filterVersions) != count ||
            env->GetArrayLength(filterSources) != count ||
            env->GetArrayLength(filterOpacities) != count ||
            env->GetArrayLength(textureWidths) != count ||
            env->GetArrayLength(textureHeights) != count ||
            env->GetArrayLength(textureBytes) != count) {
            return false;
        }

        std::vector<jlong> starts;
        std::vector<jlong> ends;
        std::vector<jdouble> segmentSpeeds;
        std::vector<jint> versions;
        std::vector<jfloat> opacities;
        try {
            starts.resize(static_cast<size_t>(count));
            ends.resize(static_cast<size_t>(count));
            segmentSpeeds.resize(static_cast<size_t>(count));
            versions.resize(static_cast<size_t>(count));
            opacities.resize(static_cast<size_t>(count));
            segments->reserve(static_cast<size_t>(count));
        } catch (...) {
            return false;
        }
        env->GetLongArrayRegion(startsMs, 0, count, starts.data());
        env->GetLongArrayRegion(endsMs, 0, count, ends.data());
        env->GetDoubleArrayRegion(speeds, 0, count, segmentSpeeds.data());
        env->GetIntArrayRegion(filterVersions, 0, count, versions.data());
        env->GetFloatArrayRegion(filterOpacities, 0, count, opacities.data());
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            return false;
        }

        for (jsize i = 0; i < count; ++i) {
            auto pathString = static_cast<jstring>(env->GetObjectArrayElement(paths, i));
            auto adjustmentValues =
                    static_cast<jfloatArray>(env->GetObjectArrayElement(adjustments, i));
            auto filterSource =
                    static_cast<jstring>(env->GetObjectArrayElement(filterSources, i));
            auto widths = static_cast<jintArray>(env->GetObjectArrayElement(textureWidths, i));
            auto heights = static_cast<jintArray>(env->GetObjectArrayElement(textureHeights, i));
            auto bytes = static_cast<jobjectArray>(env->GetObjectArrayElement(textureBytes, i));

            bool ok = pathString != nullptr && adjustmentValues != nullptr;
            TimelineSegment segment;
            if (ok) {
                const char *pathChars = env->GetStringUTFChars(pathString, nullptr);
                if (pathChars == nullptr) {
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    ok = false;
                } else {
                    try {
                        segment.path.assign(pathChars);
                    } catch (...) {
                        ok = false;
                    }
                    env->ReleaseStringUTFChars(pathString, pathChars);
                }
            }
            if (ok) {
                AppearanceApplyResult conversion = readAppearance(
                        env, adjustmentValues, versions[static_cast<size_t>(i)],
                        filterSource, opacities[static_cast<size_t>(i)],
                        widths, heights, bytes, &segment.appearance);
                ok = conversion.accepted();
            }
            if (ok) {
                segment.startMs = static_cast<int64_t>(starts[static_cast<size_t>(i)]);
                segment.endMs = static_cast<int64_t>(ends[static_cast<size_t>(i)]);
                segment.speed = static_cast<double>(segmentSpeeds[static_cast<size_t>(i)]);
                try {
                    segments->push_back(std::move(segment));
                } catch (...) {
                    ok = false;
                }
            }

            if (pathString != nullptr) env->DeleteLocalRef(pathString);
            if (adjustmentValues != nullptr) env->DeleteLocalRef(adjustmentValues);
            if (filterSource != nullptr) env->DeleteLocalRef(filterSource);
            if (widths != nullptr) env->DeleteLocalRef(widths);
            if (heights != nullptr) env->DeleteLocalRef(heights);
            if (bytes != nullptr) env->DeleteLocalRef(bytes);
            if (!ok) {
                if (env->ExceptionCheck()) env->ExceptionClear();
                return false;
            }
        }
        return true;
    }

} // namespace

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void * /* reserved */) {
    gJvm = vm;
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_cii_videolib_NativeLib_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

// Returns the linked FFmpeg version, proving the static archives are actually
// linked into libvideolib.so. Referencing av_version_info() pulls in libavutil
// and avformat_version() pulls in libavformat, so the linker cannot strip them.
extern "C" JNIEXPORT jstring JNICALL
Java_com_cii_videolib_NativeLib_nativeFFmpegVersion(
        JNIEnv *env,
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
Java_com_cii_videolib_VideoPreview_nativeCreate(JNIEnv *env, jobject thiz) {
    try {
        auto bridge = std::make_shared<PlaybackJniBridge>(env, thiz);
        if (!bridge->isValid()) {
            return 0;
        }
        auto *playback = new VideoPlayback(
                [bridge](uint64_t attemptId, PlaybackKind kind,
                         std::optional<PlaybackErrorCode> error, int segmentIndex) {
                    bridge->notify(attemptId, kind, error, segmentIndex);
                });
        return reinterpret_cast<jlong>(playback);
    } catch (...) {
        return 0;
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_cii_videolib_VideoPreview_nativeSurfaceAvailable(
        JNIEnv *env, jobject /* this */, jlong handle, jobject surface) {
    VideoPlayback *playback = asPlayback(handle);
    if (playback == nullptr || surface == nullptr) {
        return JNI_FALSE;
    }
    // ANativeWindow_fromSurface adds a reference; PreviewRenderer owns it and
    // releases it exactly once in releaseSurface.
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    if (window == nullptr) {
        return JNI_FALSE;
    }
    return playback->surfaceAvailable(window) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_cii_videolib_VideoPreview_nativePlay(
        JNIEnv *env, jobject /* this */, jlong handle, jstring path) {
    VideoPlayback *playback = asPlayback(handle);
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

// Ordered-timeline entry point. Segment i is described by the i-th element of
// every parallel array; the per-segment appearance arrays mirror
// nativeApplyAppearance's parameters. Returns a positive attempt id when the
// timeline was accepted, otherwise 0. The Kotlin external fun signature must
// stay in exact sync with this parameter list (hand-mangled JNI, no
// RegisterNatives).
extern "C" JNIEXPORT jlong JNICALL
Java_com_cii_videolib_VideoPreview_nativePlayTimeline(
        JNIEnv *env,
        jobject /* this */,
        jlong handle,
        jobjectArray paths,
        jlongArray startsMs,
        jlongArray endsMs,
        jdoubleArray speeds,
        jobjectArray adjustments,
        jintArray filterVersions,
        jobjectArray filterSources,
        jfloatArray filterOpacities,
        jobjectArray textureWidths,
        jobjectArray textureHeights,
        jobjectArray textureBytes) {
    VideoPlayback *playback = asPlayback(handle);
    if (playback == nullptr) {
        return 0;
    }
    std::vector<TimelineSegment> segments;
    if (!readTimelineSegments(
            env, paths, startsMs, endsMs, speeds, adjustments, filterVersions,
            filterSources, filterOpacities, textureWidths, textureHeights,
            textureBytes, &segments)) {
        return 0;
    }
    return static_cast<jlong>(playback->playTimeline(std::move(segments)));
}

extern "C" JNIEXPORT void JNICALL
Java_com_cii_videolib_VideoPreview_nativeStop(
        JNIEnv *env, jobject /* this */, jlong handle) {
    VideoPlayback *playback = asPlayback(handle);
    if (playback != nullptr) {
        playback->stop();
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_cii_videolib_VideoPreview_nativePause(
        JNIEnv *env, jobject /* this */, jlong handle) {
    VideoPlayback *playback = asPlayback(handle);
    return playback != nullptr && playback->pause() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_cii_videolib_VideoPreview_nativeResume(
        JNIEnv *env, jobject /* this */, jlong handle) {
    VideoPlayback *playback = asPlayback(handle);
    return playback != nullptr && playback->resume() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_cii_videolib_VideoPreview_nativeSetLooping(
        JNIEnv *env, jobject /* this */, jlong handle, jboolean enabled) {
    VideoPlayback *playback = asPlayback(handle);
    return playback != nullptr && playback->setLooping(enabled == JNI_TRUE)
           ? JNI_TRUE
           : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_cii_videolib_VideoPreview_nativeSetPlaybackSpeed(
        JNIEnv *env, jobject /* this */, jlong handle, jdouble speed) {
    VideoPlayback *playback = asPlayback(handle);
    return playback != nullptr && playback->setPlaybackSpeed(static_cast<double>(speed))
           ? JNI_TRUE
           : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_cii_videolib_VideoPreview_nativeSeekTo(
        JNIEnv *env, jobject /* this */, jlong handle, jlong positionMs) {
    VideoPlayback *playback = asPlayback(handle);
    return playback != nullptr && playback->seekTo(static_cast<int64_t>(positionMs))
           ? JNI_TRUE
           : JNI_FALSE;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_cii_videolib_VideoPreview_nativeApplyAppearance(
        JNIEnv *env,
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
        JNIEnv *env, jobject /* this */, jlong handle, jobject frame, jint width, jint height) {
    VideoPlayback *playback = asPlayback(handle);
    if (playback == nullptr || frame == nullptr) {
        return;
    }
    // Zero-copy: requires a direct ByteBuffer (validated on the Kotlin side).
    // The buffer must outlive this synchronous call; the renderer copies the
    // pixels into a GL texture before returning.
    const auto *pixels = static_cast<const uint8_t *>(env->GetDirectBufferAddress(frame));
    if (pixels == nullptr) {
        return;
    }
    playback->pushFrame(pixels, width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_cii_videolib_VideoPreview_nativeRequestPattern(
        JNIEnv *env, jobject /* this */, jlong handle) {
    VideoPlayback *playback = asPlayback(handle);
    if (playback != nullptr) {
        playback->requestPattern();
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_cii_videolib_VideoPreview_nativeRepresent(
        JNIEnv *env, jobject /* this */, jlong handle) {
    VideoPlayback *playback = asPlayback(handle);
    return playback != nullptr && playback->representFrame() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_cii_videolib_VideoPreview_nativeReleaseSurface(
        JNIEnv *env, jobject /* this */, jlong handle) {
    VideoPlayback *playback = asPlayback(handle);
    if (playback != nullptr) {
        playback->releaseSurface();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_cii_videolib_VideoPreview_nativeDestroy(
        JNIEnv *env, jobject /* this */, jlong handle) {
    VideoPlayback *playback = asPlayback(handle);
    delete playback; // destructor stops decode, then releases EGL and callbacks
}

// --- Export (VideoExporter) --------------------------------------------------
// Each export maps 1:1 to a com.cii.videolib.VideoExporter `external fun`; the
// hand-mangled name must stay in exact sync with the Kotlin class/method names.
// nativeHandle is an opaque pointer to a heap VideoExport owned by the Kotlin
// VideoExporter instance.

namespace {

    class ExportJniBridge {
    public:
        ExportJniBridge(JNIEnv *env, jobject target) : vm_(gJvm) {
            if (env == nullptr || target == nullptr || vm_ == nullptr) {
                return;
            }
            target_ = env->NewGlobalRef(target);
            jclass targetClass = env->GetObjectClass(target);
            if (target_ != nullptr && targetClass != nullptr) {
                completedMethod_ = env->GetMethodID(
                        targetClass, "onNativeExportCompleted", "(J)V");
                errorMethod_ = env->GetMethodID(
                        targetClass, "onNativeExportError", "(JII)V");
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

        ~ExportJniBridge() {
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
            return target_ != nullptr && completedMethod_ != nullptr &&
                   errorMethod_ != nullptr;
        }

        void notify(uint64_t attemptId, std::optional<ExportErrorCode> error,
                    int segmentIndex) const {
            bool attached = false;
            JNIEnv *env = environment(&attached);
            if (env == nullptr || !isValid()) {
                if (attached) vm_->DetachCurrentThread();
                return;
            }
            if (error.has_value()) {
                env->CallVoidMethod(target_, errorMethod_, static_cast<jlong>(attemptId),
                                    static_cast<jint>(*error), static_cast<jint>(segmentIndex));
            } else {
                env->CallVoidMethod(target_, completedMethod_, static_cast<jlong>(attemptId));
            }
            if (env->ExceptionCheck()) {
                LOGE("VideoExporter native callback raised an exception");
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

    static inline VideoExport *asExport(jlong handle) {
        return reinterpret_cast<VideoExport *>(handle);
    }

    // Marshal the parallel per-segment arrays (mirroring nativePlayTimeline) into
    // an ExportRequest. Each segment's appearance is read through the same
    // readAppearance path used by preview, so validation stays identical.
    // Returns true on success; on any structural error clears pending JNI
    // exceptions and returns false so the caller rejects the whole request.
    bool readExportRequest(
            JNIEnv *env,
            jstring outputPath,
            jboolean includeAudio,
            jobjectArray paths,
            jlongArray startsMs,
            jlongArray endsMs,
            jdoubleArray speeds,
            jobjectArray adjustments,
            jintArray filterVersions,
            jobjectArray filterSources,
            jfloatArray filterOpacities,
            jobjectArray textureWidths,
            jobjectArray textureHeights,
            jobjectArray textureBytes,
            ExportRequest *request) {
        if (outputPath == nullptr || paths == nullptr || startsMs == nullptr ||
            endsMs == nullptr || speeds == nullptr || adjustments == nullptr ||
            filterVersions == nullptr || filterSources == nullptr ||
            filterOpacities == nullptr || textureWidths == nullptr ||
            textureHeights == nullptr || textureBytes == nullptr) {
            return false;
        }
        const char *outChars = env->GetStringUTFChars(outputPath, nullptr);
        if (outChars == nullptr) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            return false;
        }
        try {
            request->outputPath.assign(outChars);
        } catch (...) {
            env->ReleaseStringUTFChars(outputPath, outChars);
            return false;
        }
        env->ReleaseStringUTFChars(outputPath, outChars);
        request->includeAudio = includeAudio == JNI_TRUE;

        const jsize count = env->GetArrayLength(paths);
        if (count <= 0 ||
            env->GetArrayLength(startsMs) != count ||
            env->GetArrayLength(endsMs) != count ||
            env->GetArrayLength(speeds) != count ||
            env->GetArrayLength(adjustments) != count ||
            env->GetArrayLength(filterVersions) != count ||
            env->GetArrayLength(filterSources) != count ||
            env->GetArrayLength(filterOpacities) != count ||
            env->GetArrayLength(textureWidths) != count ||
            env->GetArrayLength(textureHeights) != count ||
            env->GetArrayLength(textureBytes) != count) {
            return false;
        }

        std::vector<jlong> starts;
        std::vector<jlong> ends;
        std::vector<jdouble> segmentSpeeds;
        std::vector<jint> versions;
        std::vector<jfloat> opacities;
        try {
            starts.resize(static_cast<size_t>(count));
            ends.resize(static_cast<size_t>(count));
            segmentSpeeds.resize(static_cast<size_t>(count));
            versions.resize(static_cast<size_t>(count));
            opacities.resize(static_cast<size_t>(count));
            request->segments.reserve(static_cast<size_t>(count));
        } catch (...) {
            return false;
        }
        env->GetLongArrayRegion(startsMs, 0, count, starts.data());
        env->GetLongArrayRegion(endsMs, 0, count, ends.data());
        env->GetDoubleArrayRegion(speeds, 0, count, segmentSpeeds.data());
        env->GetIntArrayRegion(filterVersions, 0, count, versions.data());
        env->GetFloatArrayRegion(filterOpacities, 0, count, opacities.data());
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            return false;
        }

        for (jsize i = 0; i < count; ++i) {
            auto pathString = static_cast<jstring>(env->GetObjectArrayElement(paths, i));
            auto adjustmentValues =
                    static_cast<jfloatArray>(env->GetObjectArrayElement(adjustments, i));
            auto filterSource =
                    static_cast<jstring>(env->GetObjectArrayElement(filterSources, i));
            auto widths = static_cast<jintArray>(env->GetObjectArrayElement(textureWidths, i));
            auto heights = static_cast<jintArray>(env->GetObjectArrayElement(textureHeights, i));
            auto bytes = static_cast<jobjectArray>(env->GetObjectArrayElement(textureBytes, i));

            bool ok = pathString != nullptr && adjustmentValues != nullptr;
            ExportSegment segment;
            if (ok) {
                const char *pathChars = env->GetStringUTFChars(pathString, nullptr);
                if (pathChars == nullptr) {
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    ok = false;
                } else {
                    try {
                        segment.path.assign(pathChars);
                    } catch (...) {
                        ok = false;
                    }
                    env->ReleaseStringUTFChars(pathString, pathChars);
                }
            }
            if (ok) {
                AppearanceApplyResult conversion = readAppearance(
                        env, adjustmentValues, versions[static_cast<size_t>(i)],
                        filterSource, opacities[static_cast<size_t>(i)],
                        widths, heights, bytes, &segment.appearance);
                ok = conversion.accepted();
            }
            if (ok) {
                segment.startMs = static_cast<int64_t>(starts[static_cast<size_t>(i)]);
                segment.endMs = static_cast<int64_t>(ends[static_cast<size_t>(i)]);
                segment.speed = static_cast<double>(segmentSpeeds[static_cast<size_t>(i)]);
                try {
                    request->segments.push_back(std::move(segment));
                } catch (...) {
                    ok = false;
                }
            }

            if (pathString != nullptr) env->DeleteLocalRef(pathString);
            if (adjustmentValues != nullptr) env->DeleteLocalRef(adjustmentValues);
            if (filterSource != nullptr) env->DeleteLocalRef(filterSource);
            if (widths != nullptr) env->DeleteLocalRef(widths);
            if (heights != nullptr) env->DeleteLocalRef(heights);
            if (bytes != nullptr) env->DeleteLocalRef(bytes);
            if (!ok) {
                if (env->ExceptionCheck()) env->ExceptionClear();
                return false;
            }
        }
        return true;
    }

} // namespace

extern "C" JNIEXPORT jlong JNICALL
Java_com_cii_videolib_VideoExporter_nativeCreate(JNIEnv *env, jobject thiz) {
    try {
        auto bridge = std::make_shared<ExportJniBridge>(env, thiz);
        if (!bridge->isValid()) {
            return 0;
        }
        auto *exporter = new VideoExport(
                [bridge](uint64_t attemptId, std::optional<ExportErrorCode> error,
                         int segmentIndex) {
                    bridge->notify(attemptId, error, segmentIndex);
                });
        return reinterpret_cast<jlong>(exporter);
    } catch (...) {
        return 0;
    }
}

// Timeline-shaped export entry (a single-clip export is a one-element timeline).
// The Kotlin external fun signature must stay in exact sync with this parameter
// list (hand-mangled JNI, no RegisterNatives).
extern "C" JNIEXPORT jlong JNICALL
Java_com_cii_videolib_VideoExporter_nativeStartExport(
        JNIEnv *env,
        jobject /* this */,
        jlong handle,
        jstring outputPath,
        jboolean includeAudio,
        jobjectArray paths,
        jlongArray startsMs,
        jlongArray endsMs,
        jdoubleArray speeds,
        jobjectArray adjustments,
        jintArray filterVersions,
        jobjectArray filterSources,
        jfloatArray filterOpacities,
        jobjectArray textureWidths,
        jobjectArray textureHeights,
        jobjectArray textureBytes) {
    VideoExport *exporter = asExport(handle);
    if (exporter == nullptr) {
        return 0;
    }
    ExportRequest request;
    if (!readExportRequest(env, outputPath, includeAudio, paths, startsMs, endsMs,
                           speeds, adjustments, filterVersions, filterSources,
                           filterOpacities, textureWidths, textureHeights,
                           textureBytes, &request)) {
        return 0;
    }
    return static_cast<jlong>(exporter->start(std::move(request)));
}

extern "C" JNIEXPORT void JNICALL
Java_com_cii_videolib_VideoExporter_nativeCancelExport(
        JNIEnv *env, jobject /* this */, jlong handle) {
    VideoExport *exporter = asExport(handle);
    if (exporter != nullptr) {
        exporter->cancel();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_cii_videolib_VideoExporter_nativeDestroy(
        JNIEnv *env, jobject /* this */, jlong handle) {
    VideoExport *exporter = asExport(handle);
    delete exporter; // destructor cancels the attempt and releases resources
}
