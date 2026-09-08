#include <jni.h>
#include <string>

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
