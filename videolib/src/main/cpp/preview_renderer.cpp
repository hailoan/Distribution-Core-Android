//
// See preview_renderer.h.
//

#include "preview_renderer.h"

#include <android/log.h>

#define LOG_TAG "videolib.egl"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

PreviewRenderer::~PreviewRenderer() {
    // Ensure EGL is torn down and the window released before the executor
    // (destroyed after this body) drains and joins its worker.
    releaseSurface();
}

bool PreviewRenderer::initEglLocked() {
    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display_ == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay returned EGL_NO_DISPLAY");
        return false;
    }
    if (eglInitialize(display_, nullptr, nullptr) != EGL_TRUE) {
        LOGE("eglInitialize failed: 0x%04x", eglGetError());
        return false;
    }

    const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE};
    EGLConfig config = nullptr;
    EGLint numConfigs = 0;
    if (eglChooseConfig(display_, configAttribs, &config, 1, &numConfigs) != EGL_TRUE ||
        numConfigs < 1) {
        LOGE("eglChooseConfig failed: 0x%04x", eglGetError());
        return false;
    }

    surface_ = eglCreateWindowSurface(display_, config, window_, nullptr);
    if (surface_ == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface failed: 0x%04x", eglGetError());
        return false;
    }

    const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    context_ = eglCreateContext(display_, config, EGL_NO_CONTEXT, contextAttribs);
    if (context_ == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext failed: 0x%04x", eglGetError());
        return false;
    }

    if (eglMakeCurrent(display_, surface_, surface_, context_) != EGL_TRUE) {
        LOGE("eglMakeCurrent failed: 0x%04x", eglGetError());
        return false;
    }

    eglQuerySurface(display_, surface_, EGL_WIDTH, &width_);
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &height_);

    if (!glProgram_.init()) {
        LOGE("GL program init failed");
        return false;
    }

    glViewport(0, 0, width_, height_);
    LOGI("EGL initialized (%dx%d)", width_, height_);
    return true;
}

bool PreviewRenderer::surfaceAvailable(ANativeWindow *window) {
    if (window == nullptr) {
        LOGE("surfaceAvailable called with null window");
        return false;
    }
    // Re-init on a fresh surface: tear down any prior EGL first (idempotent).
    if (state_ != State::Idle && state_ != State::Released) {
        releaseSurface();
    }
    window_ = window; // take ownership of the one reference

    bool ok = false;
    executor_.runSync([this, &ok] { ok = initEglLocked(); });

    if (!ok) {
        state_ = State::Failed;
        // Release partial EGL/window so a retry starts clean.
        releaseSurface();
        state_ = State::Failed;
        return false;
    }
    state_ = State::Ready;
    return true;
}

void PreviewRenderer::pushFrame(const uint8_t *pixels, int width, int height) {
    if (state_ != State::Ready && state_ != State::Rendering) {
        return;
    }
    if (pixels == nullptr || width <= 0 || height <= 0) {
        return; // invalid frame: ignore, keep last good state (AC-1)
    }
    executor_.runSync([this, pixels, width, height] {
        if (eglMakeCurrent(display_, surface_, surface_, context_) != EGL_TRUE) {
            LOGE("pushFrame eglMakeCurrent failed: 0x%04x", eglGetError());
            return;
        }
        glViewport(0, 0, width_, height_);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glProgram_.drawFrame(pixels, width, height);
        eglSwapBuffers(display_, surface_);
    });
    state_ = State::Rendering;
}

void PreviewRenderer::requestPattern() {
    if (state_ != State::Ready && state_ != State::Rendering) {
        return;
    }
    executor_.runSync([this] {
        if (eglMakeCurrent(display_, surface_, surface_, context_) != EGL_TRUE) {
            LOGE("requestPattern eglMakeCurrent failed: 0x%04x", eglGetError());
            return;
        }
        glViewport(0, 0, width_, height_);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glProgram_.drawTestPattern();
        eglSwapBuffers(display_, surface_);
    });
    state_ = State::Rendering;
}

void PreviewRenderer::teardownEglLocked() {
    if (display_ != EGL_NO_DISPLAY) {
        // Release GL objects while the context is still current.
        if (context_ != EGL_NO_CONTEXT) {
            eglMakeCurrent(display_, surface_, surface_, context_);
            glProgram_.release();
        }
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(display_, context_);
        }
        if (surface_ != EGL_NO_SURFACE) {
            eglDestroySurface(display_, surface_);
        }
        eglTerminate(display_);
    }
    display_ = EGL_NO_DISPLAY;
    surface_ = EGL_NO_SURFACE;
    context_ = EGL_NO_CONTEXT;
    width_ = 0;
    height_ = 0;
}

void PreviewRenderer::releaseSurface() {
    // Idempotent: nothing to do if never initialized / already released.
    if (state_ == State::Released &&
        display_ == EGL_NO_DISPLAY && window_ == nullptr) {
        return;
    }
    executor_.runSync([this] { teardownEglLocked(); });

    if (window_ != nullptr) {
        ANativeWindow_release(window_); // the single release of the one ref
        window_ = nullptr;
    }
    state_ = State::Released;
}
