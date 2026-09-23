#ifndef VIDEOLIB_OFFSCREEN_RENDERER_H
#define VIDEOLIB_OFFSCREEN_RENDERER_H

#include <EGL/egl.h>
#include <cstdint>
#include <vector>

#include "appearance.h"
#include "gl_program.h"
#include "render_thread_executor.h"

// Headless EGL/GLES 3.0 renderer for export. Unlike PreviewRenderer it binds no
// ANativeWindow: it creates an EGL pbuffer surface of the export dimensions,
// reuses the exact GlProgram appearance+filter pipeline as preview (effect
// parity), draws each host RGBA frame, and reads the filtered result back as
// RGBA. All GL/EGL work is marshalled onto one RenderThreadExecutor with the
// context current only on that thread (opengles-guideline).
class OffscreenRenderer {
public:
    OffscreenRenderer() = default;

    ~OffscreenRenderer();

    OffscreenRenderer(const OffscreenRenderer &) = delete;

    OffscreenRenderer &operator=(const OffscreenRenderer &) = delete;

    // Initialize EGL pbuffer (width x height, both must be > 0) and the GL
    // program with the initial appearance. Returns false on EGL/GL failure.
    bool init(int width, int height, const AppearanceSnapshot &appearance);

    // Replace the active appearance (adjustments + filter) for later frames.
    bool applyAppearance(const AppearanceSnapshot &appearance);

    // Draw one RGBA8888 frame through the effect program and read the filtered
    // result back into `outRgba` (resized to width*height*4). Returns false on
    // GL failure. `pixels` must remain valid for the call.
    //
    // ORIENTATION: the readback follows the GL convention — row 0 of `outRgba`
    // is the BOTTOM of the image, i.e. vertically flipped relative to the
    // top-left-origin decoded frame. The consumer (H264Encoder) undoes this flip
    // during RGBA->NV12 conversion, mirroring the camera record path.
    // `timeSeconds` is the frame's presentation time within the segment and becomes
    // the effect's u_time, so an exported effect animates exactly as it previewed.
    bool renderToRgba(const uint8_t *pixels, int width, int height, float timeSeconds,
                      std::vector<uint8_t> *outRgba);

    int width() const { return width_; }

    int height() const { return height_; }

private:
    bool initEglLocked(const AppearanceSnapshot &appearance);

    void teardownLocked();

    RenderThreadExecutor executor_{"videolib_export_gl"};
    GlProgram glProgram_;

    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;
    int width_ = 0;
    int height_ = 0;
    bool ready_ = false;
};

#endif // VIDEOLIB_OFFSCREEN_RENDERER_H
