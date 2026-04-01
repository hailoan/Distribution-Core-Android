# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)
set(CMAKE_ANDROID_API 24)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)
set(CMAKE_ANDROID_PAGE_SIZE 16384) # critical for 16KB alignment
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

set(FFMPEG_ROOT ${CMAKE_SOURCE_DIR}/ffmpegv2/${CMAKE_ANDROID_ARCH_ABI})
set(FFMPEG_INCLUDE_DIR ${FFMPEG_ROOT}/include)
set(FFMPEG_LIB_DIR ${FFMPEG_ROOT})
include_directories(${FFMPEG_INCLUDE_DIR})
link_directories(${FFMPEG_LIB_DIR})

#set(FFMPEG_LIB_DIR ${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI}/lib)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
set(CMAKE_BUILD_TYPE Debug)
include_directories(${CMAKE_SOURCE_DIR}/include)

# Declares the project name. The project name can be accessed via ${ PROJECT_NAME},
# Since this is the top level CMakeLists.txt, the project name is also accessible
# with ${CMAKE_PROJECT_NAME} (both CMake variables are in-sync within the top level
# build script scope).

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.
#
# In this top level CMakeLists.txt, ${CMAKE_PROJECT_NAME} is used to define
# the target library name; in the sub-module's CMakeLists.txt, ${PROJECT_NAME}
# is preferred for the same purpose.
#
# In order to load a library into your app from Java/Kotlin, you must call
# System.loadLibrary() and pass the name of the library defined here;
# for GameActivity/NativeActivity derived applications, the same library name must be
# used in the AndroidManifest.xml file.

add_library( avcodec-57
        SHARED
        IMPORTED)
set_target_properties( avcodec-57
        PROPERTIES IMPORTED_LOCATION
        ${FFMPEG_LIB_DIR}/libavcodec-57.so)

add_library( swresample-2
        SHARED
        IMPORTED)
set_target_properties( swresample-2
        PROPERTIES IMPORTED_LOCATION
        ${FFMPEG_LIB_DIR}/libswresample-2.so)

add_library( avdevice-57
        SHARED
        IMPORTED)
set_target_properties( avdevice-57
        PROPERTIES IMPORTED_LOCATION
        ${FFMPEG_LIB_DIR}/libavdevice-57.so)

add_library( avfilter-6
        SHARED
        IMPORTED)
set_target_properties( avfilter-6
        PROPERTIES IMPORTED_LOCATION
        ${FFMPEG_LIB_DIR}/libavfilter-6.so)

add_library( avformat-57
        SHARED
        IMPORTED)
set_target_properties( avformat-57
        PROPERTIES IMPORTED_LOCATION
        ${FFMPEG_LIB_DIR}/libavformat-57.so)

#add_library( avresample
#        SHARED
#        IMPORTED)
#set_target_properties( avresample
#        PROPERTIES IMPORTED_LOCATION
#        ${FFMPEG_LIB_DIR}/libavresample-2.so)

add_library( avutil-55
        SHARED
        IMPORTED)
set_target_properties( avutil-55
        PROPERTIES IMPORTED_LOCATION
        ${FFMPEG_LIB_DIR}/libavutil-55.so)

add_library( postproc-54
        SHARED
        IMPORTED)
set_target_properties( postproc-54
        PROPERTIES IMPORTED_LOCATION
        ${FFMPEG_LIB_DIR}/libpostproc-54.so)
add_library( swscale-4
        SHARED
        IMPORTED)
set_target_properties( swscale-4
        PROPERTIES IMPORTED_LOCATION
        ${FFMPEG_LIB_DIR}/libswscale-4.so)

include_directories(${CMAKE_SOURCE_DIR}/include)

project("camera" LANGUAGES C CXX)
add_library(${CMAKE_PROJECT_NAME} SHARED
        # List C/C++ source files with relative paths to this CMakeLists.txt.
        camera_native.cpp
        camera/camera_controller.cpp
        gl/gl_unit.cpp
        gl/texture_loader.cpp
        gl/egl_renderer.cpp
        gl/video_gl.cpp
)



# Specifies libraries CMake should link to your target library. You
# can link libraries from various origins, such as libraries defined in this
# build script, prebuilt third-party libraries, or Android system libraries.

target_link_libraries(
        ${CMAKE_PROJECT_NAME}
        camera2ndk      # ACameraManager, ACameraDevice, ACaptureSession
        mediandk        # AImageReader, AImage
        android         # ANativeWindow
        log
        avcodec-57
        swresample-2
        avdevice-57
        avfilter-6
        avformat-57
        avutil-55
        postproc-54
        swscale-4
        EGL
        GLESv3
        -ljnigraphics
)