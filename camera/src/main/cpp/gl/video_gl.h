//
// Created by Loannth5 on 28/6/25.
//

#ifndef VIDEOEDITOR_VIDEO_GL_H
#define VIDEOEDITOR_VIDEO_GL_H


#include <GLES3/gl3.h>
#include "../utils/Vec3.h"

extern "C" {
#include "libavutil/frame.h"
#include "libavutil/pixdesc.h"
}

#include "gl_unit.h"
#include "texture_loader.h"
#include "../utils/log-android.h"
#include "iostream"
#include "../utils/single_thread_executor.h"

typedef struct VideoGl {
    GLfloat arrVertex[8] = {
            -1.0f, 1.0f,
            -1.0f, -1.0f,
            1.0f, -1.0f,
            1.0f, 1.0f
    };

    GLfloat arrPosition[8] = {
            1.0f, 0.0f,
            0.0f, 0.0f,
            0.0f, 1.0f,
            1.0f, 1.0f
    };

    const GLfloat vertexSize = 2;

    const GLfloat vertexStride = vertexSize * 4;

    const GLushort indices[6] = {0, 1, 2, 0, 2, 3};

    const GLchar *aPosition = "a_position";
    const GLchar *aTexCoord = "a_texCoord";
    const GLchar *cUBrightness = "u_brightness";
    const GLchar *cUContrast = "u_contrast";
    const GLchar *cUSaturation = "u_saturation";
    const GLchar *cUExposure = "u_exposure";
    const GLchar *cUDark = "u_dark";
    const GLchar *cUHighlight = "u_highlight";
    const GLchar *cUShadow = "u_shadow";
    const GLchar *cUVignette = "u_vignette";
    const GLchar *cULevel = "u_level";
    const GLchar *cUScale = "u_scale";
    const GLchar *cUHue = "u_hue";
    const GLchar *cUTemp = "u_temp";
    const GLchar *cUClarity = "u_clarity";
    const GLchar *cUTextureCurve = "u_texture_curve";
    const GLchar *cUTextureOverlay = "u_texture_overlay";
    const GLchar *cUOpacityOverlay = "u_opacity_overlay";
    const GLchar *cUTextureY = "texY";
    const GLchar *cUTextureU = "texU";
    const GLchar *cUTextureV = "texV";
    const GLchar *cULight = "u_light";
    const GLchar *cURedShift = "u_red_shift";
    const GLchar *cUOrangeShift = "u_orange_shift";
    const GLchar *cUYellowShift = "u_yellow_shift";
    const GLchar *cUGreenShift = "u_green_shift";
    const GLchar *cUCyanShift = "u_cyan_shift";
    const GLchar *cUBlueShift = "u_blue_shift";
    const GLchar *cUPurpleShift = "u_purple_shift";
    const GLchar *cUPinkShift = "u_pink_shift";
    const GLchar *cUVibrant = "u_vibrance";
    const GLchar *cUBalanceMidtones = "u_balance_midtones";
    const GLchar *cUBalanceShadow = "u_balance_shadow";
    const GLchar *cUBalanceHighlight = "u_balance_highlight";
    const GLchar *cURotation = "u_rotation";
    const GLchar *cUTime = "u_time";

    std::unordered_map<int, const char *> uMapFilter = {
            {0, "u_texture_filter0"},
            {1, "u_texture_filter1"},
            {2, "u_texture_filter2"}
    };

    /*
     * video
     */
    int width;
    int height;
    GLuint textureIdY;
    GLuint textureIdU;
    GLuint textureIdV;
    GLuint textureIdOverlay = 0;
    GLuint textureFilterIds[3] = {static_cast<GLuint>(-1), static_cast<GLuint>(-1),
                                  static_cast<GLuint>(-1)};
    GLuint program;
    float rotation = -90.0;
    float scaleX = 1.0, scaleY = 1.0;
    float time = 0.0;

    /**
     * ranger -0.5...0.5
     * default 0
     */
    float brightness = 0.0;
/**
     * ranger 0...2f
     * default 1
     */
    float contrast = 1.0;

    /**
     * ranger -1...1
     * default 0
     */
    float exposure = 0.0;

    /**
     * ranger -0...2
     * default 1
     */
    float saturation = 1.0;

    /**
     * ranger 0.5...1.5
     * default 1
     */
    float dark = 1.0;
    float vignette = 0.0;

    /**
     * ranger -1...1
     * default 0
     */
    float vibrance = 0.0;

    /**
     * ranger light 0...2 dark
     * default 1
     */
    float light = 1.0;

    float highlight = -2.0;

    /**
     * ranger -1...1
     * default 0
     */
    float shadow = 0.0;

    /**
     * ranger -0.5...0.5
     * default 0
     */
    float temperature = 0.0;

    /**
     * ranger -1...1
     * default 0
     */
    float hue = 0.0;
    float clarity = 0.0;
    float opacityOverlay = 1.0;

    /**
     * x -1...1 default 0
     * y 0.5...1.5 default 1
     * z 0.5...1.5 default 1
     */
    Vec3 level = Vec3(0.0, 1.0, 1.0);

    Vec3 balanceShadow = Vec3(1.0, 1.0, 1.0);

//    /**
//     * value max 1.0 min 0.0
//     */
//    float mixColor: EnumMap < AdjustColorType, Vec3> = EnumMap(AdjustColorType::class.java)

    /**
     * position and uniform
     */

    GLuint aPos;
    GLuint aTex;

    GLint uTextureY;
    GLint uTextureU;
    GLint uTextureV;
    GLint uTextureCurve;
    GLint uTextureOverlay;
    GLint *uTextureFilters = nullptr;
    GLint uRotation;
    GLint uScale;
    GLint uExposure;
    GLint uBrightness;
    GLint uContrast;
    GLint uHighlight;
    GLint uLight;
    GLint uDark;
    GLint uShadow;
    GLint uSaturation;
    GLint uVibrant;
    GLint uTemperature;
    GLint uHue;
    GLint uClarity;
    GLint uVignette;
    GLint uBalance;
    GLint uMixRed;
    GLint uOrange;
    GLint uYellow;
    GLint uGreen;
    GLint uCyan;
    GLint uBlue;
    GLint uPurple;
    GLint uPink;
    GLint uTime;
    GLint uLevel;
    size_t filterSize = 0;
};

void unUseGL(VideoGl *gl);

void setAttributes(VideoGl *gl);

void bindUniformYUV(VideoGl *gl, int w, int h, AVFrame *data);

void initProgram(VideoGl *gl, const char *vertex, const char *fragment);

void onRelease(VideoGl *gl);

#endif //VIDEOEDITOR_VIDEO_GL_H
