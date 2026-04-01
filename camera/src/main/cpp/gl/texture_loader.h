//
// Created by loannth5 on 7/7/21.
//

#ifndef IMAGEGL_TEXTURE_LOADER_H
#define IMAGEGL_TEXTURE_LOADER_H

#include "gl_unit.h"
#include "android/asset_manager.h"
#include "android/asset_manager_jni.h"

GLuint loadTexture2D();

GLuint loadTextureFromPixel(const uint8_t* pixel, int width, int height);

#endif //IMAGEGL_TEXTURE_LOADER_H
