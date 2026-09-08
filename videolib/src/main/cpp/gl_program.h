//
// GLES 3.0 draw path for the videolib preview: a passthrough RGBA program that
// samples one 2D texture onto a full-surface quad, plus a CPU-generated test
// pattern used when no host frame is available.
//
// Deliberately source-agnostic: RGBA8888 only, no YUV, no AVFrame, no FFmpeg,
// and no external-OES/SurfaceTexture sampler (see opengles-guideline). All
// functions here must be called on the render thread with the EGL context
// current (owner: PreviewRenderer via RenderThreadExecutor).
//

#ifndef VIDEOLIB_GL_PROGRAM_H
#define VIDEOLIB_GL_PROGRAM_H

#include <GLES3/gl3.h>
#include <cstdint>

// Owns one GLES program, one RGBA texture, and the unit-quad geometry.
class GlProgram {
public:
    GlProgram() = default;
    ~GlProgram() = default;

    // Compiles/links the program and creates the texture + geometry. Must run
    // with the context current. Returns false on shader/link failure (the
    // renderer then stays in a safe non-rendering state). Idempotent-safe: a
    // second successful init releases the prior program.
    bool init();

    // Uploads an RGBA8888 buffer (w*h*4 bytes, tightly packed) as the current
    // texture and draws it to the bound surface. Caller validates dimensions and
    // buffer lifetime. No-op guidance: pass pixels==nullptr to redraw last state.
    void drawFrame(const uint8_t *pixels, int width, int height);

    // Draws a self-contained test pattern (no host data). Proves the pipeline.
    void drawTestPattern();

    // Releases GL objects. Must run with the context current.
    void release();

    bool isReady() const { return program_ != 0; }

private:
    void ensureTextureSize(int width, int height);
    void drawQuad();

    GLuint program_ = 0;
    GLuint texture_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    GLint aPositionLoc_ = -1;
    GLint aTexCoordLoc_ = -1;
    GLint uTextureLoc_ = -1;
    int texWidth_ = 0;
    int texHeight_ = 0;
};

#endif // VIDEOLIB_GL_PROGRAM_H
