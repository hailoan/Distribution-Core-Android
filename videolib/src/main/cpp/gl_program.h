// GLES 3.0 RGBA appearance program. Every method is render-thread-only and
// requires the owning PreviewRenderer's EGL context to be current.
#ifndef VIDEOLIB_GL_PROGRAM_H
#define VIDEOLIB_GL_PROGRAM_H

#include <GLES3/gl3.h>
#include <cstdint>
#include <string>
#include <vector>
#include "appearance.h"

class GlProgram {
public:
    bool init(const AppearanceSnapshot &appearance, AppearanceApplyResult *result);

    AppearanceApplyResult applyAppearance(const AppearanceSnapshot &appearance);

    // Sets the effect clock, in seconds from the start of the kept interval. This
    // is what an effect snippet reads as u_time, so it is the caller's frame
    // timestamp rather than wall time: the same frame always renders identically,
    // which keeps a paused redraw stable and an export equal to its preview.
    void setEffectTime(float seconds) { effectTime_ = seconds; }

    void drawFrame(const uint8_t *pixels, int width, int height);

    void drawTestPattern();

    void release();

    bool isReady() const { return generation_.program != 0; }

    // True once a frame has been uploaded into the base texture, so drawFrame
    // can redraw it without new pixel data.
    bool hasRetainedFrame() const { return texWidth_ > 0 && texHeight_ > 0; }

    // Extent of the retained base frame. Zero until the first upload.
    int retainedFrameWidth() const { return texWidth_; }

    int retainedFrameHeight() const { return texHeight_; }

private:
    struct UniformLocations {
        GLint texture = -1, filterOpacity = -1, texelSize = -1;
        GLint effectOpacity = -1, time = -1;
        GLint brightness = -1, contrast = -1, saturation = -1, exposure = -1;
        GLint darks = -1, levels = -1, vignette = -1, vibrance = -1;
        GLint temperature = -1, hue = -1, highlights = -1, shadows = -1;
        GLint lights = -1, clarity = -1;
        std::vector<GLint> filterTextures;
    };
    struct Generation {
        GLuint program = 0;
        std::vector<GLuint> filterTextures;
        UniformLocations uniforms;
        std::optional<FilterDescriptor> filter;
        std::optional<EffectDescriptor> effect;
    };

    AppearanceApplyResult
    buildGeneration(const AppearanceSnapshot &appearance, Generation *candidate);

    void releaseGeneration(Generation *generation);

    void bindAppearanceUniforms();

    void ensureTextureSize(int width, int height);

    void drawQuad();

    Generation generation_;
    AdjustmentSnapshot adjustments_;
    GLuint texture_ = 0, vbo_ = 0, ebo_ = 0;
    int texWidth_ = 0, texHeight_ = 0;
    float effectTime_ = 0.0f;
};

#endif // VIDEOLIB_GL_PROGRAM_H
