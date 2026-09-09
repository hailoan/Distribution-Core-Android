//
// EGL/GLES 3.0 preview renderer for videolib.
//
// Owns the EGL display/surface/context, the GL draw program, and the renderer
// lifecycle state. All GL/EGL work is marshalled onto a single RenderThreadExecutor
// so the context is created on and current only on that thread (opengles-guideline,
// ndk-cpp-guideline). Frame content is host-supplied RGBA8888 or a built-in test
// pattern — no video/camera/FFmpeg dependency (SOLUTION-DESIGN D-2, AC-6).
//
// State model (SOLUTION-DESIGN §2):
//   Idle -> Ready -> Rendering -> Released/Failed, with idempotent teardown.
//

#ifndef VIDEOLIB_PREVIEW_RENDERER_H
#define VIDEOLIB_PREVIEW_RENDERER_H

#include <EGL/egl.h>
#include <android/native_window.h>
#include <cstdint>
#include <memory>

#include "gl_program.h"
#include "render_thread_executor.h"

class PreviewRenderer {
public:
    enum class State { Idle, Ready, Rendering, Released, Failed };

    PreviewRenderer() = default;
    ~PreviewRenderer();

    PreviewRenderer(const PreviewRenderer &) = delete;
    PreviewRenderer &operator=(const PreviewRenderer &) = delete;

    // Takes ownership of `window` (one ANativeWindow reference, released once in
    // releaseSurface). Initializes EGL + GL program on the render thread.
    // Returns false if the window is null or EGL/GL init fails (-> Failed).
    bool surfaceAvailable(ANativeWindow *window);

    // Uploads and draws a host RGBA8888 frame. Returns false unless the frame
    // was presented successfully while Ready/Rendering.
    // `pixels` must remain valid until the call returns (synchronous on render
    // thread); the caller owns the buffer.
    bool pushFrame(const uint8_t *pixels, int width, int height);

    // Draws the built-in test pattern. Ignored unless Ready/Rendering.
    void requestPattern();

    // Ordered EGL teardown + single ANativeWindow release. Idempotent.
    void releaseSurface();

    State state() const { return state_; }
    bool isSurfaceReady() const {
        return state_ == State::Ready || state_ == State::Rendering;
    }

private:
    bool initEglLocked();       // render-thread body of surfaceAvailable
    void teardownEglLocked();   // render-thread body of releaseSurface

    RenderThreadExecutor executor_{"videolib_render"};
    GlProgram glProgram_;

    ANativeWindow *window_ = nullptr;
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;
    EGLint width_ = 0;
    EGLint height_ = 0;

    State state_ = State::Idle;
};

#endif // VIDEOLIB_PREVIEW_RENDERER_H
