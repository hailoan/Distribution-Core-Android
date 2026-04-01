//
// Created by Loannth5 on 2/4/25.
//

#ifndef MY_APPLICATION_EGL_RENDERER_H
#define MY_APPLICATION_EGL_RENDERER_H

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <unistd.h>
#include "queue"
#include "mutex"
#include "condition_variable"
#include "thread"
#include "video_gl.h"

using namespace std;

typedef struct EGLRenderer {
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    EGLConfig config;
    ANativeWindow *nativeWindow;
    EGLint w, h, format;
    SingleThreadExecutor *thread = new SingleThreadExecutor("egl_renderer");
    VideoGl *gl = new VideoGl();
    AVFrame *currentFrame;
};


void onReady(EGLRenderer *egl);

void initVideoGL(EGLRenderer *egl, const char *vertex, const char *fragment);

bool initEGL(EGLRenderer *egl, const char *vertex, const char *fragment);

void updateFilter(EGLRenderer *egl, const char *vertex, const char *fragment,
                  std::vector<const uint8_t *> filters,
                  std::vector<jint> widths, std::vector<jint> heights);

void requestRenderer(EGLRenderer *egl);

void
startInitGL(EGLRenderer *egl, ANativeWindow *window, const char *vertex, const char *fragment);

void renderFrame(EGLRenderer *egl, AVFrame *frame);

void cleanup(EGLRenderer *egl);

#endif //MY_APPLICATION_EGL_RENDERER_H
