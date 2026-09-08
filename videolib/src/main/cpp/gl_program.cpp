//
// See gl_program.h. GLES 3.0, RGBA-only, no FFmpeg/YUV/OES.
//

#include "gl_program.h"

#include <android/log.h>
#include <cmath>
#include <cstdlib>
#include <vector>

#define LOG_TAG "videolib.gl"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace {

// Full-surface quad in clip space with matching texture coords. Texture V is
// flipped (1 - t) so a top-left-origin RGBA buffer appears upright on screen.
// Interleaved: x, y, u, v.
const GLfloat kQuadVertices[] = {
    //  x      y     u     v
    -1.0f, -1.0f, 0.0f, 1.0f, // bottom-left
     1.0f, -1.0f, 1.0f, 1.0f, // bottom-right
     1.0f,  1.0f, 1.0f, 0.0f, // top-right
    -1.0f,  1.0f, 0.0f, 0.0f, // top-left
};
const GLushort kQuadIndices[] = {0, 1, 2, 0, 2, 3};

const char *kVertexShader =
    "#version 300 es\n"
    "layout(location = 0) in vec2 a_position;\n"
    "layout(location = 1) in vec2 a_texCoord;\n"
    "out vec2 v_texCoord;\n"
    "void main() {\n"
    "    v_texCoord = a_texCoord;\n"
    "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
    "}\n";

const char *kFragmentShader =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec2 v_texCoord;\n"
    "uniform sampler2D u_texture;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    fragColor = texture(u_texture, v_texCoord);\n"
    "}\n";

GLuint compileShader(GLenum type, const char *src) {
    GLuint shader = glCreateShader(type);
    if (shader == 0) {
        LOGE("glCreateShader failed for type 0x%04x", type);
        return 0;
    }
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        GLint logLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        if (logLen > 0) {
            std::vector<char> log(static_cast<size_t>(logLen));
            glGetShaderInfoLog(shader, logLen, nullptr, log.data());
            LOGE("shader compile failed (%s): %s",
                 type == GL_VERTEX_SHADER ? "vertex" : "fragment", log.data());
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint linkProgram(const char *vsSrc, const char *fsSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    if (vs == 0) {
        return 0;
    }
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (fs == 0) {
        glDeleteShader(vs);
        return 0;
    }
    GLuint program = glCreateProgram();
    if (program == 0) {
        LOGE("glCreateProgram failed");
        glDeleteShader(vs);
        glDeleteShader(fs);
        return 0;
    }
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    // Shaders are no longer needed once linked (or on failure).
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        GLint logLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
        if (logLen > 0) {
            std::vector<char> log(static_cast<size_t>(logLen));
            glGetProgramInfoLog(program, logLen, nullptr, log.data());
            LOGE("program link failed: %s", log.data());
        }
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

} // namespace

bool GlProgram::init() {
    // Idempotent: a re-init releases the prior program's objects first.
    if (program_ != 0) {
        release();
    }

    program_ = linkProgram(kVertexShader, kFragmentShader);
    if (program_ == 0) {
        return false;
    }

    aPositionLoc_ = glGetAttribLocation(program_, "a_position");
    aTexCoordLoc_ = glGetAttribLocation(program_, "a_texCoord");
    uTextureLoc_ = glGetUniformLocation(program_, "u_texture");

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

    LOGI("GlProgram initialized (program=%u)", program_);
    return true;
}

void GlProgram::ensureTextureSize(int width, int height) {
    glBindTexture(GL_TEXTURE_2D, texture_);
    if (width != texWidth_ || height != texHeight_) {
        // Allocate immutable-size storage lazily on first frame / size change.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
        texWidth_ = width;
        texHeight_ = height;
    }
}

void GlProgram::drawQuad() {
    glUseProgram(program_);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glUniform1i(uTextureLoc_, 0);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);

    const GLsizei stride = 4 * sizeof(GLfloat);
    glEnableVertexAttribArray(static_cast<GLuint>(aPositionLoc_));
    glVertexAttribPointer(static_cast<GLuint>(aPositionLoc_), 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void *>(0));
    glEnableVertexAttribArray(static_cast<GLuint>(aTexCoordLoc_));
    glVertexAttribPointer(static_cast<GLuint>(aTexCoordLoc_), 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void *>(2 * sizeof(GLfloat)));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);

    glDisableVertexAttribArray(static_cast<GLuint>(aPositionLoc_));
    glDisableVertexAttribArray(static_cast<GLuint>(aTexCoordLoc_));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

void GlProgram::drawFrame(const uint8_t *pixels, int width, int height) {
    if (program_ == 0 || width <= 0 || height <= 0) {
        return;
    }
    if (pixels != nullptr) {
        ensureTextureSize(width, height);
        glBindTexture(GL_TEXTURE_2D, texture_);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    drawQuad();
}

void GlProgram::drawTestPattern() {
    if (program_ == 0) {
        return;
    }
    // Small CPU-generated RGBA gradient/checker — self-contained, no host data.
    const int w = 256;
    const int h = 256;
    std::vector<uint8_t> buf(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t i = (static_cast<size_t>(y) * w + x) * 4;
            bool checker = ((x >> 5) + (y >> 5)) & 1;
            buf[i + 0] = static_cast<uint8_t>(x);              // R gradient
            buf[i + 1] = static_cast<uint8_t>(y);              // G gradient
            buf[i + 2] = checker ? 0xFF : 0x40;                // B checker
            buf[i + 3] = 0xFF;                                 // A opaque
        }
    }
    drawFrame(buf.data(), w, h);
}

void GlProgram::release() {
    if (texture_ != 0) {
        glDeleteTextures(1, &texture_);
        texture_ = 0;
    }
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (ebo_ != 0) {
        glDeleteBuffers(1, &ebo_);
        ebo_ = 0;
    }
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
    texWidth_ = 0;
    texHeight_ = 0;
}
