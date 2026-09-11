#include "offscreen_renderer.h"

#include <android/log.h>
#include <GLES3/gl3.h>

#define LOG_TAG "videolib.export.gl"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

OffscreenRenderer::~OffscreenRenderer() {
    executor_.runSync([this] { teardownLocked(); });
}

bool OffscreenRenderer::initEglLocked(const AppearanceSnapshot &appearance) {
    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display_ == EGL_NO_DISPLAY || eglInitialize(display_, nullptr, nullptr) != EGL_TRUE) {
        LOGE("eglInitialize failed: 0x%04x", eglGetError());
        return false;
    }

    const EGLint configAttribs[] = {
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
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

    const EGLint pbufferAttribs[] = {
            EGL_WIDTH, width_,
            EGL_HEIGHT, height_,
            EGL_NONE};
    surface_ = eglCreatePbufferSurface(display_, config, pbufferAttribs);
    if (surface_ == EGL_NO_SURFACE) {
        LOGE("eglCreatePbufferSurface failed: 0x%04x", eglGetError());
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

    AppearanceApplyResult result;
    if (!glProgram_.init(appearance, &result)) {
        LOGE("export GL program init failed");
        return false;
    }
    glViewport(0, 0, width_, height_);
    return true;
}

bool OffscreenRenderer::init(int width, int height, const AppearanceSnapshot &appearance) {
    if (width <= 0 || height <= 0) {
        return false;
    }
    width_ = width;
    height_ = height;
    bool ok = false;
    executor_.runSync([this, &appearance, &ok] { ok = initEglLocked(appearance); });
    if (!ok) {
        executor_.runSync([this] { teardownLocked(); });
        return false;
    }
    ready_ = true;
    return true;
}

bool OffscreenRenderer::applyAppearance(const AppearanceSnapshot &appearance) {
    if (!ready_) return false;
    bool ok = false;
    executor_.runSync([this, &appearance, &ok] {
        if (eglMakeCurrent(display_, surface_, surface_, context_) != EGL_TRUE) {
            return;
        }
        while (glGetError() != GL_NO_ERROR) {}
        ok = glProgram_.applyAppearance(appearance).accepted();
    });
    return ok;
}

bool OffscreenRenderer::renderToRgba(const uint8_t *pixels, int width, int height,
                                     std::vector<uint8_t> *outRgba) {
    if (!ready_ || pixels == nullptr || width <= 0 || height <= 0) {
        return false;
    }
    bool ok = false;
    executor_.runSync([this, pixels, width, height, outRgba, &ok] {
        if (eglMakeCurrent(display_, surface_, surface_, context_) != EGL_TRUE) {
            LOGE("render eglMakeCurrent failed: 0x%04x", eglGetError());
            return;
        }
        while (glGetError() != GL_NO_ERROR) {}
        glViewport(0, 0, width_, height_);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        // Reuse the exact preview effect pass (adjustments + filter). The source
        // frame may differ in size from the pbuffer only if the decoder changed
        // dimensions mid-stream; export sizes the pbuffer to the first frame, so
        // width/height match width_/height_ in the normal case.
        glProgram_.drawFrame(pixels, width, height);
        if (glGetError() != GL_NO_ERROR) {
            LOGE("render drawFrame GL error");
            return;
        }
        outRgba->resize(static_cast<size_t>(width_) * height_ * 4);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, outRgba->data());
        if (glGetError() != GL_NO_ERROR) {
            LOGE("glReadPixels failed");
            return;
        }
        ok = true;
    });
    return ok;
}

void OffscreenRenderer::teardownLocked() {
    if (display_ != EGL_NO_DISPLAY) {
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
    ready_ = false;
}
