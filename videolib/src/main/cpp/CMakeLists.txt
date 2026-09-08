# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.

cmake_minimum_required(VERSION 3.22.1)

project("videolib")

# Keep libvideolib.so 16 KB-page aligned — the FFmpeg static archives under
# ffmpeg/<abi> were cross-compiled with max-page-size=16384 (ffmpeg-build).
set(CMAKE_ANDROID_PAGE_SIZE 16384)

# --- Prebuilt FFmpeg 7.1 static archives (per ABI) ---------------------------
# Cross-compiled out-of-band by ffmpeg-build/ and committed under
# ffmpeg/<abi>/{include,lib}. Only arm64-v8a and armeabi-v7a are provided; the
# module's abiFilters (build.gradle.kts) is restricted to match.
set(FFMPEG_ROOT ${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI})
set(FFMPEG_INCLUDE_DIR ${FFMPEG_ROOT}/include)
set(FFMPEG_LIB_DIR ${FFMPEG_ROOT}/lib)
include_directories(${FFMPEG_INCLUDE_DIR})

# Import each archive as a STATIC IMPORTED target.
foreach(fflib avdevice avfilter avformat avcodec swscale swresample avutil)
    add_library(${fflib} STATIC IMPORTED)
    set_target_properties(${fflib} PROPERTIES
            IMPORTED_LOCATION ${FFMPEG_LIB_DIR}/lib${fflib}.a)
endforeach()

# Creates the module's shared library. Gradle packages it into the AAR/APK and
# it is loaded via System.loadLibrary("videolib").
add_library(${CMAKE_PROJECT_NAME} SHARED
        # List C/C++ source files with relative paths to this CMakeLists.txt.
        videolib.cpp
        # OpenGL ES preview pipeline (EGL renderer + GLES draw path). Header-only
        # render_thread_executor.h / gl_program.h are pulled in via includes.
        preview_renderer.cpp
        gl_program.cpp)

# 16 KB max-page-size on every ABI. arm64-v8a additionally requires -Bsymbolic
# to resolve FFmpeg's aarch64 TX-table PC-relative relocations
# (libavutil/aarch64/tx_float_neon.S); without it the static link fails with
# "R_AARCH64_ADR_PREL_PG_HI21 cannot be used against symbol 'ff_tx_tab_*_float'".
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "-Wl,-z,max-page-size=16384")
if(ANDROID_ABI STREQUAL "arm64-v8a")
    target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "-Wl,-Bsymbolic")
endif()

# Link the FFmpeg archives in dependency order (consumers before providers,
# avutil last), wrapped in a link group so the linker rescans and resolves
# FFmpeg's cross-references. z/m satisfy FFmpeg's zlib/math dependencies.
target_link_libraries(${CMAKE_PROJECT_NAME}
        -Wl,--start-group
        avdevice
        avfilter
        avformat
        avcodec
        swscale
        swresample
        avutil
        -Wl,--end-group
        z
        m
        # Android system libraries
        android
        log
        # OpenGL ES preview: EGL context + GLES 3.0 rendering onto the Surface.
        EGL
        GLESv3)
