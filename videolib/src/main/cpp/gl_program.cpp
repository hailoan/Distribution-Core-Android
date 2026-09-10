#include "gl_program.h"

#include <android/log.h>
#include <algorithm>
#include <new>
#include <regex>
#include <sstream>
#include <utility>

#define LOG_TAG "videolib.gl"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace {
    const GLfloat kQuadVertices[] = {
            -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f,
    };
    const GLushort kQuadIndices[] = {0, 1, 2, 0, 2, 3};

    const char *kVertexShader = R"glsl(#version 300 es
layout(location=0) in vec2 a_position;
layout(location=1) in vec2 a_texCoord;
out vec2 v_texCoord;
void main(){ v_texCoord=a_texCoord; gl_Position=vec4(a_position,0.0,1.0); }
)glsl";

    const char *kFragmentPrefix = R"glsl(#version 300 es
precision highp float;
in vec2 v_texCoord;
uniform sampler2D u_texture;
uniform float u_filterOpacity;
uniform vec2 u_texelSize;
uniform float u_brightness, u_contrast, u_saturation, u_exposure, u_darks;
uniform float u_vignette, u_vibrance, u_temperature, u_hue, u_highlights;
uniform float u_shadows, u_lights, u_clarity;
uniform vec3 u_levels;
out vec4 fragColor;
const vec3 W=vec3(0.2125,0.7154,0.0721);
vec4 brightnessAdjust(vec4 c,float v){c.rgb+=v;return c;}
vec4 contrastAdjust(vec4 c,float v){c.rgb=c.rgb*v+0.5-v*0.5;return c;}
vec4 saturationAdjust(vec4 c,float v){return vec4(mix(vec3(dot(c.rgb,W)),c.rgb,v),c.a);}
vec4 exposureAdjust(vec4 c,float v){c.rgb*=pow(2.0,v);return c;}
vec4 darkAdjust(vec4 c,float v){return vec4(pow(max(c.rgb,vec3(0.0)),vec3(v)),c.a);}
vec3 finalLevels(vec3 c,vec3 v){return pow(clamp((c-v.x)/(v.z-v.x),0.0,1.0),vec3(1.0/v.y));}
vec4 vignetteAdjust(vec4 c,float v,vec2 uv){float e=smoothstep(0.0,max(v,0.0001),distance(uv,vec2(0.5)));return vec4(mix(c.rgb,vec3(0.0),e),c.a);}
vec4 vibranceAdjust(vec4 c,float v){vec4 k=vec4(0.299,0.587,0.114,0.0);float l=dot(c,k);vec4 m=clamp(c-vec4(l),0.0,1.0);return mix(vec4(l,l,l,c.a),c,1.0+v*(1.0-dot(k,m)));}
vec4 temperatureAdjust(vec4 c,float v){c.rgb*=vec3(1.0+v*0.22,1.0,1.0-v*0.22);return c;}
vec4 hueAdjust(vec4 c,float v){
 const vec4 ky=vec4(0.299,0.587,0.114,0.0),ki=vec4(0.596,-0.275,-0.321,0.0),kq=vec4(0.212,-0.523,0.311,0.0);
 float y=dot(c,ky),i=dot(c,ki),q=dot(c,kq),h=atan(q,i)+v,ch=length(vec2(i,q));i=ch*cos(h);q=ch*sin(h);
 c.r=dot(vec4(y,i,q,0.0),vec4(1.0,0.956,0.621,0.0));c.g=dot(vec4(y,i,q,0.0),vec4(1.0,-0.272,-0.647,0.0));c.b=dot(vec4(y,i,q,0.0),vec4(1.0,-1.107,1.704,0.0));return c;}
vec4 highlightAdjust(vec4 c,float v){float l=(max(c.r,max(c.g,c.b))+min(c.r,min(c.g,c.b)))*0.5;if(l<=0.00001||l<0.5)return c;float x=abs(v*2.0);float n=l+0.1357698*sign(v)*exp(-0.5*(pow((x-1.0060456)/0.467434,2.0)+pow((l-0.8029415)/0.1127807,2.0)));return vec4(c.rgb*n/l,c.a);}
vec4 shadowAdjust(vec4 c,float v){if(v==0.0)return c;float l=dot(c.rgb,vec3(0.3));if(l<=0.00001)return c;float d=max(v+1.0,0.001);float s=clamp(pow(max(l,0.0),1.0/d)-0.76*pow(max(l,0.0),2.0/d)-l,0.0,1.0);return vec4((l+s)*c.rgb/l,c.a);}
vec4 lightAdjust(vec4 c,float v){c.rgb=clamp(c.rgb/max(v,0.0001),0.0,1.0);return c;}
vec4 clarityAdjust(vec4 c,float v,vec2 uv){vec3 b=(texture(u_texture,uv+vec2(u_texelSize.x,0.0)).rgb+texture(u_texture,uv-vec2(u_texelSize.x,0.0)).rgb+texture(u_texture,uv+vec2(0.0,u_texelSize.y)).rgb+texture(u_texture,uv-vec2(0.0,u_texelSize.y)).rgb)*0.25;float i=v<0.0?v*0.5:v*2.0;c.rgb=mix(c.rgb,clamp(c.rgb+(c.rgb-b),0.0,1.0),i);return c;}
)glsl";
    const char *kSharedFilterComponents = R"glsl(
const float FILTER_HUE_RANGE = 1.0 / 12.0;
float current_hue;

vec3 hueToRgb(float hue) {
    hue = fract(hue);
    return clamp(
        vec3(
            abs(hue * 6.0 - 3.0) - 1.0,
            2.0 - abs(hue * 6.0 - 2.0),
            2.0 - abs(hue * 6.0 - 4.0)
        ),
        0.0,
        1.0
    );
}

vec3 HSLtoRGB(vec3 hsl) {
    if (hsl.y == 0.0) return vec3(hsl.z);
    float maximum = hsl.z < 0.5
        ? hsl.z * (1.0 + hsl.y)
        : hsl.z + hsl.y - hsl.y * hsl.z;
    float minimum = 2.0 * hsl.z - maximum;
    return minimum + hueToRgb(hsl.x) * (maximum - minimum);
}

vec3 RGBtoHSL(vec3 color) {
    float minimum = min(min(color.r, color.g), color.b);
    float maximum = max(max(color.r, color.g), color.b);
    float delta = maximum - minimum;
    vec3 hsl = vec3(0.0, 0.0, (maximum + minimum) / 2.0);
    if (delta != 0.0) {
        hsl.y = hsl.z < 0.5
            ? delta / (maximum + minimum)
            : delta / (2.0 - maximum - minimum);
        float deltaRed = (((maximum - color.r) / 6.0) + (delta / 2.0)) / delta;
        float deltaGreen = (((maximum - color.g) / 6.0) + (delta / 2.0)) / delta;
        float deltaBlue = (((maximum - color.b) / 6.0) + (delta / 2.0)) / delta;
        if (color.r == maximum) {
            hsl.x = deltaBlue - deltaGreen;
        } else if (color.g == maximum) {
            hsl.x = (1.0 / 3.0) + deltaRed - deltaBlue;
        } else {
            hsl.x = (2.0 / 3.0) + deltaGreen - deltaRed;
        }
        hsl.x = fract(hsl.x);
    }
    return clamp(hsl, 0.0, 1.0);
}

vec3 vibranceAdjust(vec3 color, float vibrance) {
    float luminance = color.r * 0.299 + color.g * 0.587 + color.b * 0.114;
    float minimum = min(min(color.r, color.g), color.b);
    float maximum = max(max(color.r, color.g), color.b);
    float saturation =
        (1.0 - (maximum - minimum)) * (1.0 - maximum) * luminance * 5.0;
    vec3 lightness = vec3((minimum + maximum) / 2.0);
    return mix(color, mix(color, lightness, -vibrance), saturation);
}

float cubicPulse(float center, float width, float value) {
    return smoothstep(center - width, center, value) -
        smoothstep(center, center + width, value);
}

vec3 mixColorRed(vec3 hsl, float hue, float saturation, float lightness) {
    float firstPulse = cubicPulse(
        0.0,
        1.0,
        current_hue / (0.8 * FILTER_HUE_RANGE)
    );
    float secondPulse = cubicPulse(
        0.0,
        1.0,
        (current_hue - 11.5 * FILTER_HUE_RANGE) / (0.5 * FILTER_HUE_RANGE)
    );
    float pulse = max(firstPulse, secondPulse);
    vec3 adjusted = vec3(
        hsl.x + (hue - 1.0) * FILTER_HUE_RANGE,
        min(1.5, hsl.y * abs(saturation)),
        hsl.z + (lightness - 1.0) * FILTER_HUE_RANGE
    );
    return mix(hsl, adjusted, pulse);
}

vec3 mixColorOrange(vec3 hsl, float hue, float saturation, float lightness) {
    float pulse = cubicPulse(
        0.0,
        1.0,
        (current_hue - FILTER_HUE_RANGE * 0.65) / FILTER_HUE_RANGE
    );
    vec3 adjusted = vec3(
        hsl.x + (hue - 1.0) * FILTER_HUE_RANGE,
        min(1.5, hsl.y * abs(saturation)),
        hsl.z + (lightness - 1.0) * FILTER_HUE_RANGE
    );
    return mix(hsl, adjusted, pulse);
}

vec3 mixColorYellow(vec3 hsl, float hue, float saturation, float lightness) {
    float pulse = cubicPulse(
        0.0,
        1.0,
        (current_hue - FILTER_HUE_RANGE * 1.5) / FILTER_HUE_RANGE
    );
    vec3 adjusted = vec3(
        hsl.x + (hue - 1.0) * FILTER_HUE_RANGE,
        min(1.5, hsl.y * abs(saturation)),
        hsl.z + (lightness - 1.0) * FILTER_HUE_RANGE
    );
    return mix(hsl, adjusted, pulse);
}

vec3 mixColorGreen(vec3 hsl, float hue, float saturation, float lightness) {
    float pulse = cubicPulse(
        0.0,
        1.0,
        (current_hue - FILTER_HUE_RANGE * 2.5) / (FILTER_HUE_RANGE * 2.5)
    );
    vec3 adjusted = vec3(
        hsl.x + (hue - 1.0) * FILTER_HUE_RANGE,
        min(1.5, hsl.y * abs(saturation)),
        hsl.z + (lightness - 1.0) * FILTER_HUE_RANGE
    );
    return mix(hsl, adjusted, pulse);
}

vec3 mixColorCyan(vec3 hsl, float hue, float saturation, float lightness) {
    float pulse = cubicPulse(
        0.0,
        1.0,
        (current_hue - FILTER_HUE_RANGE * 5.0) / (FILTER_HUE_RANGE * 2.0)
    );
    vec3 adjusted = vec3(
        hsl.x + (hue - 1.0) * FILTER_HUE_RANGE,
        min(1.5, hsl.y * abs(saturation)),
        hsl.z + (lightness - 1.0) * FILTER_HUE_RANGE
    );
    return mix(hsl, adjusted, pulse);
}

vec3 mixColorBlue(vec3 hsl, float hue, float saturation, float lightness) {
    float pulse = cubicPulse(
        0.0,
        1.0,
        (current_hue - FILTER_HUE_RANGE * 7.0) / (FILTER_HUE_RANGE * 2.0)
    );
    vec3 adjusted = vec3(
        hsl.x + (hue - 1.0) * FILTER_HUE_RANGE,
        min(1.5, hsl.y * abs(saturation)),
        hsl.z + (lightness - 1.0) * FILTER_HUE_RANGE
    );
    return mix(hsl, adjusted, pulse);
}
)glsl";
    const char *kPassThroughFilter = "vec4 addFilter(vec4 inputColor,vec2 uv){return inputColor;}\n";
    const char *kFragmentSuffix = R"glsl(
void main(){
 vec4 inputColor=texture(u_texture,v_texCoord);vec4 filtered=addFilter(inputColor,v_texCoord);vec4 color=mix(inputColor,filtered,u_filterOpacity);
 if(u_brightness!=0.0)color=brightnessAdjust(color,u_brightness);if(u_contrast!=1.0)color=contrastAdjust(color,u_contrast);if(u_saturation!=1.0)color=saturationAdjust(color,u_saturation);if(u_exposure!=0.0)color=exposureAdjust(color,u_exposure);if(u_darks!=1.0)color=darkAdjust(color,u_darks);
 if(any(notEqual(u_levels,vec3(0.0,1.0,1.0))))color.rgb=finalLevels(color.rgb,u_levels);if(u_vignette!=0.0)color=vignetteAdjust(color,u_vignette,v_texCoord);if(u_vibrance!=0.0)color=vibranceAdjust(color,u_vibrance);if(u_temperature!=0.0)color=temperatureAdjust(color,u_temperature);if(u_hue!=0.0)color=hueAdjust(color,u_hue);
 if(u_highlights!=-2.0)color=highlightAdjust(color,u_highlights);if(u_shadows!=0.0)color=shadowAdjust(color,u_shadows);if(u_lights!=1.0)color=lightAdjust(color,u_lights);if(u_clarity!=0.0)color=clarityAdjust(color,u_clarity,v_texCoord);fragColor=color;
}
)glsl";

    GLuint compileShader(GLenum type, const char *source, std::string *diagnostic) {
        GLuint shader = glCreateShader(type);
        if (shader == 0) {
            *diagnostic = "glCreateShader failed";
            return 0;
        }
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        GLint ok = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (ok == GL_TRUE)return shader;
        *diagnostic = type == GL_FRAGMENT_SHADER ? "filter shader compilation failed"
                                                 : "vertex shader compilation failed";
        glDeleteShader(shader);
        return 0;
    }

    GLuint linkProgram(const std::string &fragment, AppearanceApplyResult *result) {
        std::string diagnostic;
        GLuint vertex = compileShader(GL_VERTEX_SHADER, kVertexShader, &diagnostic);
        if (vertex == 0) {
            *result = AppearanceApplyResult::failure(AppearanceError::ShaderCompilation,
                                                     diagnostic);
            return 0;
        }
        GLuint pixel = compileShader(GL_FRAGMENT_SHADER, fragment.c_str(), &diagnostic);
        if (pixel == 0) {
            glDeleteShader(vertex);
            *result = AppearanceApplyResult::failure(AppearanceError::ShaderCompilation,
                                                     diagnostic);
            return 0;
        }
        GLuint program = glCreateProgram();
        if (program == 0) {
            glDeleteShader(vertex);
            glDeleteShader(pixel);
            *result = AppearanceApplyResult::failure(AppearanceError::ResourceAllocation,
                                                     "glCreateProgram failed");
            return 0;
        }
        glAttachShader(program, vertex);
        glAttachShader(program, pixel);
        glLinkProgram(program);
        glDeleteShader(vertex);
        glDeleteShader(pixel);
        GLint ok = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (ok == GL_TRUE)return program;
        diagnostic = "appearance program link failed";
        glDeleteProgram(program);
        *result = AppearanceApplyResult::failure(AppearanceError::ProgramLink, diagnostic);
        return 0;
    }

    GLint uniform(GLuint program, const char *name) { return glGetUniformLocation(program, name); }

    bool consumerOwnsSharedFilterComponents(const std::string &source) {
        static const std::regex sharedDeclaration(
                R"glsl(\bconst\s+float\s+FILTER_HUE_RANGE\b|\bfloat\s+current_hue\b|\b(?:void|bool|int|uint|float|double|vec[234]|[biud]vec[234]|mat[234](?:x[234])?)\s+(?:hueToRgb|HSLtoRGB|RGBtoHSL|vibranceAdjust|cubicPulse|mixColorRed|mixColorOrange|mixColorYellow|mixColorGreen|mixColorCyan|mixColorBlue)\s*\()glsl");
        return std::regex_search(source, sharedDeclaration);
    }
} // namespace

bool GlProgram::init(const AppearanceSnapshot &appearance, AppearanceApplyResult *result) {
    release();
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);
    glGenBuffers(1, &ebo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kQuadIndices), kQuadIndices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (vbo_ == 0 || ebo_ == 0 || texture_ == 0) {
        *result = AppearanceApplyResult::failure(AppearanceError::ResourceAllocation,
                                                 "base GL allocation failed");
        release();
        return false;
    }
    Generation candidate;
    *result = buildGeneration(appearance, &candidate);
    if (!result->accepted()) {
        release();
        return false;
    }
    generation_ = std::move(candidate);
    adjustments_ = appearance.adjustments;
    LOGI("GlProgram initialized");
    return true;
}

AppearanceApplyResult
GlProgram::buildGeneration(const AppearanceSnapshot &appearance, Generation *candidate) {
    try {
        std::ostringstream source;
        source << kFragmentPrefix;
        for (size_t i = 0; appearance.filter && i < appearance.filter->textures.size(); ++i)
            source << "uniform sampler2D u_filterTexture" << i << ";\n";
        if (appearance.filter &&
            !consumerOwnsSharedFilterComponents(appearance.filter->source)) {
            source << kSharedFilterComponents;
        }
        source << (appearance.filter ? appearance.filter->source : kPassThroughFilter);
        source << kFragmentSuffix;
        AppearanceApplyResult result;
        candidate->program = linkProgram(source.str(), &result);
        if (candidate->program == 0)return result;
        candidate->filter = appearance.filter;
        GLint maxSize = 0, maxUnits = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxUnits);
        size_t count = appearance.filter ? appearance.filter->textures.size() : 0;
        if (count + 1U > static_cast<size_t>(std::max(maxUnits, 0))) {
            releaseGeneration(candidate);
            return AppearanceApplyResult::failure(AppearanceError::DeviceCapability,
                                                  "too many fragment textures");
        }
        glUseProgram(candidate->program);
        if (appearance.filter) {
            candidate->filterTextures.resize(count, 0);
            candidate->uniforms.filterTextures.reserve(count);
            glGenTextures(static_cast<GLsizei>(count), candidate->filterTextures.data());
            for (size_t i = 0; i < count; ++i) {
                const auto &texture = appearance.filter->textures[i];
                if (texture.width > maxSize || texture.height > maxSize ||
                    candidate->filterTextures[i] == 0) {
                    glUseProgram(0);
                    releaseGeneration(candidate);
                    return AppearanceApplyResult::failure(
                            texture.width > maxSize || texture.height > maxSize
                            ? AppearanceError::DeviceCapability
                            : AppearanceError::ResourceAllocation, "auxiliary texture unsupported");
                }
                glActiveTexture(static_cast<GLenum>(GL_TEXTURE1 + i));
                glBindTexture(GL_TEXTURE_2D, candidate->filterTextures[i]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture.width, texture.height, 0, GL_RGBA,
                             GL_UNSIGNED_BYTE, texture.rgba8888.data());
                std::string name = "u_filterTexture" + std::to_string(i);
                GLint location = uniform(candidate->program, name.c_str());
                candidate->uniforms.filterTextures.push_back(location);
                if (location >= 0)glUniform1i(location, static_cast<GLint>(i + 1));
            }
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
        if (glGetError() != GL_NO_ERROR) {
            releaseGeneration(candidate);
            return AppearanceApplyResult::failure(AppearanceError::ResourceAllocation,
                                                  "auxiliary texture upload failed");
        }
        auto &u = candidate->uniforms;
        u.texture = uniform(candidate->program, "u_texture");
        u.filterOpacity = uniform(candidate->program, "u_filterOpacity");
        u.texelSize = uniform(candidate->program, "u_texelSize");
        u.brightness = uniform(candidate->program, "u_brightness");
        u.contrast = uniform(candidate->program, "u_contrast");
        u.saturation = uniform(candidate->program, "u_saturation");
        u.exposure = uniform(candidate->program, "u_exposure");
        u.darks = uniform(candidate->program, "u_darks");
        u.levels = uniform(candidate->program, "u_levels");
        u.vignette = uniform(candidate->program, "u_vignette");
        u.vibrance = uniform(candidate->program, "u_vibrance");
        u.temperature = uniform(candidate->program, "u_temperature");
        u.hue = uniform(candidate->program, "u_hue");
        u.highlights = uniform(candidate->program, "u_highlights");
        u.shadows = uniform(candidate->program, "u_shadows");
        u.lights = uniform(candidate->program, "u_lights");
        u.clarity = uniform(candidate->program, "u_clarity");
        return AppearanceApplyResult::success();
    } catch (const std::bad_alloc &) {
        releaseGeneration(candidate);
        return AppearanceApplyResult::failure(AppearanceError::ResourceAllocation,
                                              "appearance allocation failed");
    }
}

AppearanceApplyResult GlProgram::applyAppearance(const AppearanceSnapshot &appearance) {
    if (!isReady())
        return AppearanceApplyResult::failure(AppearanceError::RenderFailure,
                                              "GL program unavailable");
    if (generation_.filter == appearance.filter) {
        adjustments_ = appearance.adjustments;
        return AppearanceApplyResult::success();
    }
    Generation candidate;
    auto result = buildGeneration(appearance, &candidate);
    if (!result.accepted())return result;
    Generation previous = std::move(generation_);
    generation_ = std::move(candidate);
    adjustments_ = appearance.adjustments;
    releaseGeneration(&previous);
    return AppearanceApplyResult::success();
}

void GlProgram::ensureTextureSize(int width, int height) {
    glBindTexture(GL_TEXTURE_2D, texture_);
    if (width != texWidth_ || height != texHeight_) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);
        texWidth_ = width;
        texHeight_ = height;
    }
}

void GlProgram::bindAppearanceUniforms() {
    const auto &u = generation_.uniforms;
    const auto &a = adjustments_;
    glUniform1f(u.filterOpacity, generation_.filter ? generation_.filter->opacity : 0.0f);
    glUniform2f(u.texelSize, texWidth_ > 0 ? 1.0f / texWidth_ : 0.0f,
                texHeight_ > 0 ? 1.0f / texHeight_ : 0.0f);
    glUniform1f(u.brightness, a.brightness);
    glUniform1f(u.contrast, a.contrast);
    glUniform1f(u.saturation, a.saturation);
    glUniform1f(u.exposure, a.exposure);
    glUniform1f(u.darks, a.darks);
    glUniform3f(u.levels, a.levelMinimum, a.levelGamma, a.levelMaximum);
    glUniform1f(u.vignette, a.vignette);
    glUniform1f(u.vibrance, a.vibrance);
    glUniform1f(u.temperature, a.temperature);
    glUniform1f(u.hue, a.hue);
    glUniform1f(u.highlights, a.highlights);
    glUniform1f(u.shadows, a.shadows);
    glUniform1f(u.lights, a.lights);
    glUniform1f(u.clarity, a.clarity);
}

void GlProgram::drawQuad() {
    glUseProgram(generation_.program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glUniform1i(generation_.uniforms.texture, 0);
    for (size_t i = 0; i < generation_.filterTextures.size(); ++i) {
        glActiveTexture(static_cast<GLenum>(GL_TEXTURE1 + i));
        glBindTexture(GL_TEXTURE_2D, generation_.filterTextures[i]);
    }
    bindAppearanceUniforms();
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    constexpr GLsizei stride = 4 * sizeof(GLfloat);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void *>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void *>(2 * sizeof(GLfloat)));
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

void GlProgram::drawFrame(const uint8_t *pixels, int width, int height) {
    if (!isReady() || width <= 0 || height <= 0)return;
    if (pixels) {
        ensureTextureSize(width, height);
        glBindTexture(GL_TEXTURE_2D, texture_);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    drawQuad();
}

void GlProgram::drawTestPattern() {
    constexpr int width = 256, height = 256;
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            size_t i = (static_cast<size_t>(y) * width + x) * 4;
            pixels[i] = static_cast<uint8_t>(x);
            pixels[i + 1] = static_cast<uint8_t>(y);
            pixels[i + 2] = ((x >> 5) + (y >> 5)) & 1 ? 0xff : 0x40;
            pixels[i + 3] = 0xff;
        }
    drawFrame(pixels.data(), width, height);
}

void GlProgram::releaseGeneration(Generation *generation) {
    if (!generation->filterTextures.empty())
        glDeleteTextures(static_cast<GLsizei>(generation->filterTextures.size()),
                         generation->filterTextures.data());
    if (generation->program)glDeleteProgram(generation->program);
    *generation = Generation{};
}

void GlProgram::release() {
    releaseGeneration(&generation_);
    if (texture_)glDeleteTextures(1, &texture_);
    if (vbo_)glDeleteBuffers(1, &vbo_);
    if (ebo_)glDeleteBuffers(1, &ebo_);
    texture_ = vbo_ = ebo_ = 0;
    texWidth_ = texHeight_ = 0;
}
