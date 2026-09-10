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

    void drawFrame(const uint8_t *pixels, int width, int height);

    void drawTestPattern();

    void release();

    bool isReady() const { return generation_.program != 0; }

private:
    struct UniformLocations {
        GLint texture = -1, filterOpacity = -1, texelSize = -1;
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
};

#endif // VIDEOLIB_GL_PROGRAM_H
