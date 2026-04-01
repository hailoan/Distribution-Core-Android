//
// Created by Admin on 6/7/2021.
//

#ifndef GRAPHIC_GL_UNIT_H
#define GRAPHIC_GL_UNIT_H


#include <android/log.h>
#include <math.h>

#include<GLES3/gl3.h>
#include<GLES3/gl3ext.h>
#include<GLES3/gl3platform.h>

#define DEBUG 1

#define LOG_TAG "GLES3JNI"
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#if DEBUG
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#else
#define ALOGV(...)
#endif

extern bool checkGlError(const char* funcName);
extern GLuint createShader(GLenum shaderType, const char* src);
extern GLuint createProgram(const char* vtxSrc, const char* fragSrc);

#endif //GRAPHIC_GL_UNIT_H
