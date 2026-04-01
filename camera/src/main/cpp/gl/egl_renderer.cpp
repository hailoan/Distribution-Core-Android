//
// Created by Loannth5 on 2/4/25.
//

#include "egl_renderer.h"

void onReady(EGLRenderer *egl) {
    egl->thread->runInThread([egl] {
//        rendererFrame(egl);
    });
}

void initVideoGL(EGLRenderer *egl, const char *vertex, const char *fragment) {
    egl->gl->textureIdY = loadTexture2D();
    egl->gl->textureIdU = loadTexture2D();
    egl->gl->textureIdV = loadTexture2D();
    initProgram(egl->gl, vertex, fragment);
}

bool initEGL(EGLRenderer *egl, const char *vertex, const char *fragment) {
    if (!egl->nativeWindow) {
        LOGI("Native window is NULL, cannot create EGL surface.");
        return false;
    }
    egl->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (egl->display == EGL_NO_DISPLAY) {
        LOGI("Failed to get EGL display");
        return false;
    }
    if (!eglInitialize(egl->display, nullptr, nullptr)) {
        LOGI("Failed to initialize EGL");
        return false;
    }

    EGLint configAttribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_DEPTH_SIZE, 16,
            EGL_NONE
    };
    EGLint numConfigs;
    if (!eglChooseConfig(egl->display, configAttribs, &egl->config, 1, &numConfigs)) {
        LOGI("Failed to choose EGL config");
        return false;
    }
    if (egl->config == nullptr) {
        LOGI("Unable to initialize EGLConfig");
        return -1;
    }
    eglGetConfigAttrib(egl->display, egl->config, EGL_NATIVE_VISUAL_ID, &egl->format);

    egl->surface = eglCreateWindowSurface(egl->display, egl->config, egl->nativeWindow, nullptr);
    if (egl->surface == EGL_NO_SURFACE) {
        LOGI("Failed to create EGL surface");
        return false;
    }

    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    egl->context = eglCreateContext(egl->display, egl->config, EGL_NO_CONTEXT, contextAttribs);
    if (egl->context == EGL_NO_CONTEXT) {
        LOGI("Failed to create EGL context");
        return false;
    }

    if (!eglMakeCurrent(egl->display, egl->surface, egl->surface, egl->context)) {
        LOGI("Failed to make EGL context current");
        return false;
    }

    eglQuerySurface(egl->display, egl->surface, EGL_WIDTH, &egl->w);
    eglQuerySurface(egl->display, egl->surface, EGL_HEIGHT, &egl->h);
    LOGI("EGL initialized successfully");

    initVideoGL(egl, vertex, fragment);
    return true;
}

void
updateFilter(EGLRenderer *egl, const char *vertex, const char *fragment,
             std::vector<const uint8_t *> filters,
             std::vector<jint> widths, std::vector<jint> heights) {
    egl->thread->runInThread([egl, vertex, fragment, filters, widths, heights] {
        egl->gl->filterSize = filters.size();

        for (int i = 0; i < filters.size(); i++) {
            egl->gl->textureFilterIds[i] = loadTextureFromPixel(filters[i], widths[i],
                                                                heights[i]);
        }
        initProgram(egl->gl, vertex, fragment);
        renderFrame(egl, egl->currentFrame);
    });
}

void requestRenderer(EGLRenderer *egl) {
    egl->thread->runInThread([egl] {
        if (egl->currentFrame) {
            renderFrame(egl, egl->currentFrame);
        }
    });
}

void
startInitGL(EGLRenderer *egl, ANativeWindow *window, const char *vertex,
                 const char *fragment) {
    egl->nativeWindow = window;
    egl->thread->runInThread([egl, vertex, fragment] {
        initEGL(egl, vertex, fragment);
    });
}

void renderFrame(EGLRenderer *egl, AVFrame *frame) {
    if (egl == nullptr || frame == nullptr) return;
    egl->currentFrame = frame;
    egl->thread->runInThread([egl, frame] {
        if (eglMakeCurrent(egl->display, egl->surface, egl->surface, egl->context)) {
            glViewport(0, 0, egl->w, egl->h);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            bindUniformYUV(egl->gl, frame->width, frame->height, frame);
            setAttributes(egl->gl);
            eglSwapBuffers(egl->display, egl->surface);
        } else {
            LOGI("eglMakeCurrent fail %d", eglGetError());
        }
    });
}

void cleanup(EGLRenderer *egl) {
    if (egl->display != EGL_NO_DISPLAY) {
        LOGI("EGL cleaned up");
        eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (egl->context != EGL_NO_CONTEXT) {
            eglDestroyContext(egl->display, egl->context);
        }
        if (egl->surface != EGL_NO_SURFACE) {
            eglDestroySurface(egl->display, egl->surface);
        }
        eglTerminate(egl->display);
    }
    // Reset to no context
    egl->display = EGL_NO_DISPLAY;
    egl->surface = EGL_NO_SURFACE;
    egl->context = EGL_NO_CONTEXT;
    ANativeWindow_release(egl->nativeWindow);
    egl->nativeWindow = nullptr;
}