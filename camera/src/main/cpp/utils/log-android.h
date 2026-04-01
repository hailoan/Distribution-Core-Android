//
// Created by Loannth5 on 18/3/25.
//

#ifndef MY_APPLICATION_LOG_ANDROID_H
#define MY_APPLICATION_LOG_ANDROID_H
#include <android/log.h>

#define LOG_TAG "ChiiStudio"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOG_INFO(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#endif //MY_APPLICATION_LOG_ANDROID_H
