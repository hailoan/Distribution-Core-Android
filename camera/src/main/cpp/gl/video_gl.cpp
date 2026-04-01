//
// Created by Loannth5 on 28/6/25.
//

#include "video_gl.h"

void initProgram(VideoGl *gl, const char *vertex, const char *fragment) {
    if (gl->program != -1) {
        glDeleteProgram(gl->program);
    }
    gl->program = createProgram(vertex, fragment);

    /**
     * position
     */
    gl->aPos = glGetAttribLocation(gl->program, gl->aPosition);
    gl->aTex = glGetAttribLocation(gl->program, gl->aTexCoord);

    /**
     * uniform
     */
    gl->uTextureY = glGetUniformLocation(gl->program, gl->cUTextureY);
    gl->uTextureU = glGetUniformLocation(gl->program, gl->cUTextureU);
    gl->uTextureV = glGetUniformLocation(gl->program, gl->cUTextureV);
    gl->uTextureCurve = glGetUniformLocation(gl->program, gl->cUTextureCurve);
    gl->uTextureOverlay = glGetUniformLocation(gl->program, gl->cUTextureOverlay);
    if (gl->filterSize > 0) {
        gl->uTextureFilters = (GLint *) malloc(sizeof(GLint) * gl->filterSize);
        for (int i = 0; i < gl->filterSize; ++i) {
            gl->uTextureFilters[i] = glGetUniformLocation(gl->program, gl->uMapFilter[i]);
        }
    }
    gl->uRotation = glGetUniformLocation(gl->program, gl->cURotation);
    gl->uScale = glGetUniformLocation(gl->program, gl->cUScale);
    gl->uExposure = glGetUniformLocation(gl->program, gl->cUExposure);
    gl->uBrightness = glGetUniformLocation(gl->program, gl->cUBrightness);
    gl->uContrast = glGetUniformLocation(gl->program, gl->cUContrast);
    gl->uHighlight = glGetUniformLocation(gl->program, gl->cUHighlight);
    gl->uLight = glGetUniformLocation(gl->program, gl->cULight);
    gl->uDark = glGetUniformLocation(gl->program, gl->cUDark);
    gl->uShadow = glGetUniformLocation(gl->program, gl->cUShadow);
    gl->uShadow = glGetUniformLocation(gl->program, gl->cUShadow);
    gl->uSaturation = glGetUniformLocation(gl->program, gl->cUSaturation);
    gl->uVibrant = glGetUniformLocation(gl->program, gl->cUVibrant);
    gl->uTemperature = glGetUniformLocation(gl->program, gl->cUTemp);
    gl->uHue = glGetUniformLocation(gl->program, gl->cUHue);
    gl->uClarity = glGetUniformLocation(gl->program, gl->cUClarity);
    gl->uVignette = glGetUniformLocation(gl->program, gl->cUVignette);
    gl->uBalance = glGetUniformLocation(gl->program, gl->cUBalanceShadow);
    gl->uLevel = glGetUniformLocation(gl->program, gl->cULevel);
    gl->uMixRed = glGetUniformLocation(gl->program, gl->cURedShift);
    gl->uOrange = glGetUniformLocation(gl->program, gl->cUOrangeShift);
    gl->uYellow = glGetUniformLocation(gl->program, gl->cUYellowShift);
    gl->uCyan = glGetUniformLocation(gl->program, gl->cUCyanShift);
    gl->uBlue = glGetUniformLocation(gl->program, gl->cUBlueShift);
    gl->uPink = glGetUniformLocation(gl->program, gl->cUPinkShift);
    gl->uGreen = glGetUniformLocation(gl->program, gl->cUGreenShift);
    gl->uPurple = glGetUniformLocation(gl->program, gl->cUPurpleShift);
    gl->uTime = glGetUniformLocation(gl->program, gl->cUTime);
//        free((char *) fragment);
//        free((char *) vertex);

}

void bindUniformYUV(VideoGl *gl, int w, int h, AVFrame *data) {
    int textureIndex = 0;
    int glTexture = GL_TEXTURE0;

    glUseProgram(gl->program);


    if (data != nullptr) {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, data->linesize[0]);
        // Y
        glActiveTexture(glTexture++);
        glBindTexture(GL_TEXTURE_2D, gl->textureIdY);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
                     w, h, 0,
                     GL_RED, GL_UNSIGNED_BYTE,
                     data->data[0]);
        glUniform1i(gl->uTextureY, textureIndex++);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); // reset

        // U
        glPixelStorei(GL_UNPACK_ROW_LENGTH, data->linesize[1]);
        glActiveTexture(glTexture++);
        glBindTexture(GL_TEXTURE_2D, gl->textureIdU);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
                     w / 2, h / 2, 0,
                     GL_RED, GL_UNSIGNED_BYTE,
                     data->data[1]);
        glUniform1i(gl->uTextureU, textureIndex++);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);


        // V
        glPixelStorei(GL_UNPACK_ROW_LENGTH, data->linesize[2]);
        glActiveTexture(glTexture++);
        glBindTexture(GL_TEXTURE_2D, gl->textureIdV);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
                     w / 2, h / 2, 0,
                     GL_RED, GL_UNSIGNED_BYTE,
                     data->data[2]);
        glUniform1i(gl->uTextureV, textureIndex++);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);


    }

    if (gl->filterSize > 0) {
        for (int index = 0; index < gl->filterSize; index++) {
            glActiveTexture(glTexture++);
            glBindTexture(GL_TEXTURE_2D, gl->textureFilterIds[index]);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);  // in case row alignment is tight
            glUniform1i(gl->uTextureFilters[index], textureIndex++);
        }
    }

    glUniform1f(gl->uRotation, gl->rotation);
    glUniform2f(gl->uScale, gl->scaleX, gl->scaleY);
    glUniform1f(gl->uTime, gl->time);
    glUniform1f(gl->uBrightness, gl->brightness);
    glUniform1f(gl->uContrast, gl->contrast);
    glUniform1f(gl->uExposure, gl->exposure);
    glUniform1f(gl->uSaturation, gl->saturation);
    glUniform1f(gl->uDark, gl->dark);
    glUniform1f(gl->uHighlight, gl->highlight);
    glUniform1f(gl->uShadow, gl->shadow);
    glUniform1f(gl->uLight, gl->light);
    glUniform1f(gl->uClarity, gl->clarity);
    glUniform1f(gl->uVignette, gl->vignette);
    glUniform1f(gl->uVibrant, gl->vibrance);
    glUniform1f(gl->uTemperature, gl->temperature);
    glUniform1f(gl->uHue, gl->hue);
    glUniform1f(gl->uDark, gl->dark);
    glUniform3f(gl->uLevel, gl->level.x, gl->level.y, gl->level.z);
    glUniform3f(gl->uBalance, gl->balanceShadow.x, gl->balanceShadow.y, gl->balanceShadow.z);
}

void setAttributes(VideoGl *gl) {
    glVertexAttribPointer(gl->aPos, (GLint) gl->vertexSize, GL_FLOAT,
                          GL_FALSE, (GLsizei) gl->vertexStride, gl->arrVertex);
    glEnableVertexAttribArray(gl->aPos);

    glVertexAttribPointer(gl->aTex, (GLsizei) gl->vertexSize, GL_FLOAT,
                          GL_FALSE, (GLint) gl->vertexStride, gl->arrPosition);

    glEnableVertexAttribArray(gl->aTex);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, gl->indices);

    glFinish();

    unUseGL(gl);
}

void unUseGL(VideoGl *gl) {
    glDisableVertexAttribArray(gl->aTex);
    glDisableVertexAttribArray(gl->aPos);
    glUseProgram(0);
}

void onRelease(VideoGl *gl) {
    glDeleteProgram(gl->program);
    glDisableVertexAttribArray(gl->aTex);
}

