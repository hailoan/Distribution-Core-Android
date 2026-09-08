---
name: Native Android Boundary Guideline
description: Protect Kotlin/JNI/C++, Camera2 NDK, EGL/GLES, MediaCodec, FFmpeg, ABI, and native lifecycle contracts when camera or native build surfaces are touched.
---

## Native Android Boundary Guideline

Load only when a change touches `camera`, JNI/native declarations, CMake, GLSL, bundled native
libraries, ABI filters, or a consumer of those contracts. This is the umbrella guard; when the
change lands specifically on the FFmpeg (`AVFrame`/swscale/packaging) surface also load
`ffmpeg-guideline.md`, on the EGL/GLES/GLSL/FBO surface also load `opengles-guideline.md`, and on
the JNI/C++/toolchain surface also load `ndk-cpp-guideline.md`.

Trace each affected operation end to end: Kotlin caller/callback → JNI export/signature → native
owner → camera/EGL/codec worker and back. Verify exact names and parameter/callback types on both
sides. Record thread affinity, JavaVM attach/detach, local/global reference ownership, buffer/frame
lifetime, stop-before-release order, and which thread/context creates and destroys GL resources.

For this repository, preserve these established invariants unless the approved design changes
them explicitly:

- native libraries are loaded by `JNILibraryLoader` before `NativeRenderer` use;
- Camera2 request rebuilds apply quality first and persistent AF/AE/EV control state afterward;
- mode/lens switching is rejected while recording;
- recorded output comes from the post-filter EGL FBO/PBO path, not a second raw-frame path;
- FFmpeg headers/shared objects are vendored inputs, with packaged ARM ABI filters and 16 KB page
  compatibility kept aligned;
- EGL/GL resource work occurs while the correct context is current.

Treat surface teardown, cleanup, callbacks during shutdown, codec/muxer finalization, output paths,
frame timestamps, pixel/YUV conversion, and backpressure as failure paths, not happy-path details.
Do not edit vendored FFmpeg content for an ordinary feature/fix.

Verification must distinguish what a host JVM test, native/Gradle compile, APK/AAR inspection, and
supported-device/ABI run can actually prove. Never claim camera, GL, codec, or library-load runtime
behavior from compilation alone.
