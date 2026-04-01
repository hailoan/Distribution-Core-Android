#include <jni.h>
#include "gl/egl_renderer.h"
#include "camera//camera_controller.h"
//
// Created by loannth20 on 1/4/26.
//
EGLRenderer *renderer = new EGLRenderer();
CameraController *g_camera = new CameraController();

std::vector<const uint8_t *> convertListOfByteBuffers(JNIEnv *env, jobject listObj, std::vector<jlong> &sizes) {
    std::vector<const uint8_t *> result;

    jclass listClass = env->GetObjectClass(listObj);
    jmethodID sizeMethod = env->GetMethodID(listClass, "size", "()I");
    jmethodID getMethod = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");

    jint listSize = env->CallIntMethod(listObj, sizeMethod);

    for (jint i = 0; i < listSize; ++i) {
        jobject byteBuffer = env->CallObjectMethod(listObj, getMethod, i);
        if (byteBuffer != nullptr) {
            void *bufferPtr = env->GetDirectBufferAddress(byteBuffer);
            jlong capacity = env->GetDirectBufferCapacity(byteBuffer);

            result.push_back(static_cast<const uint8_t *>(bufferPtr));
            sizes.push_back(capacity);

            env->DeleteLocalRef(byteBuffer);
        }
    }

    env->DeleteLocalRef(listClass);
    return result;
}

std::vector<int> convertListToIntVector(JNIEnv *env, jobject list) {
    std::vector<int> result;

    jclass listClass = env->GetObjectClass(list);
    jmethodID sizeMethod = env->GetMethodID(listClass, "size", "()I");
    jmethodID getMethod = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");

    jint size = env->CallIntMethod(list, sizeMethod);

    for (jint i = 0; i < size; i++) {
        jobject integerObj = env->CallObjectMethod(list, getMethod, i);
        jclass integerClass = env->FindClass("java/lang/Integer");
        jmethodID intValueMethod = env->GetMethodID(integerClass, "intValue", "()I");
        jint value = env->CallIntMethod(integerObj, intValueMethod);

        result.push_back(static_cast<int>(value));

        env->DeleteLocalRef(integerObj);
        env->DeleteLocalRef(integerClass);
    }
    env->DeleteLocalRef(listClass);
    return result;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_chiistudio_camerandk_jni_NativeRenderer_nativeInit(JNIEnv *env, jobject thiz,
                                                            jobject surface, jstring vertex,
                                                            jstring fragment) {
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    const char *v = env->GetStringUTFChars(vertex, nullptr);
    const char *f = env->GetStringUTFChars(fragment, nullptr);

    if (window) {
        startInitGL(renderer, window, v, f);
        g_camera->renderer = renderer;

        g_camera->open();

        g_camera->setMode(CaptureMode::PREVIEW);

        g_camera->setResolution(2);        // 1080p
        g_camera->setResolution(1920, 800); // custom
        g_camera->setPreviewQuality(3);    // ultra


        env->ReleaseStringUTFChars(vertex,   v);
        env->ReleaseStringUTFChars(fragment, f);
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_com_chiistudio_camerandk_jni_NativeRenderer_nativeRender(JNIEnv *env, jobject thiz) {

}
extern "C"
JNIEXPORT void JNICALL
Java_com_chiistudio_camerandk_jni_NativeRenderer_nativeCleanup(JNIEnv *env, jobject thiz) {

}
extern "C"
JNIEXPORT void JNICALL
Java_com_chiistudio_camerandk_jni_NativeRenderer_changeFilter(JNIEnv *env, jobject thiz,
                                                              jstring res_vertex,
                                                              jstring res_shader,
                                                              jobject data_filter, jobject widths,
                                                              jobject heights, jfloat opacity) {
    std::vector<jlong> bufferSizes;
    std::vector<const uint8_t *> buffers = convertListOfByteBuffers(env, data_filter, bufferSizes);
    updateFilter(
            renderer,
            env->GetStringUTFChars(res_vertex, nullptr),
            env->GetStringUTFChars(res_shader, nullptr),
            buffers,
            convertListToIntVector(env, widths),
            convertListToIntVector(env, heights)
    );

}
extern "C"
JNIEXPORT void JNICALL
Java_com_chiistudio_camerandk_jni_NativeRenderer_startCamera(JNIEnv *env, jobject thiz) {
    g_camera->start();
}