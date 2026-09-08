---
name: FFmpeg Guideline
description: Protect AVFrame ownership, swscale usage, vendored FFmpeg version/soname pins, library load order, and ABI packaging when camera native code touches FFmpeg. Read-only guidance.
---

## FFmpeg Guideline

Load only when a change touches FFmpeg surfaces in `camera`: `AVFrame`/`sws_scale` call sites, the
vendored `camera/src/main/cpp/ffmpegv2` tree, the FFmpeg `System.loadLibrary` order, or the ABI
packaging of the FFmpeg shared objects. This is the detail behind `native-boundary-guideline`;
keep that umbrella guard in effect too.

Know what FFmpeg is and is NOT here. Only two libraries are actually called from first-party code:

- `libavutil` — `AVFrame` allocation/refcount as the frame container passed through the pipeline
  (`av_frame_alloc`/`av_frame_clone`/`av_frame_ref`/`av_frame_free`, `av_frame_get_buffer`).
- `libswscale` — pixel-format conversion (`sws_getContext`/`sws_scale`/`sws_freeContext`), RGBA→
  YUV420P for recording and camera YUV handling.

The H.264 encode and MP4 mux are Android NDK **MediaCodec + MediaMuxer** (`mediandk`) in
`camera/src/main/cpp/video/video_encoder.cpp`, not `libavcodec`/`libavformat`. `avcodec`,
`avformat`, `avfilter`, `avdevice`, `swresample`, and `postproc` are linked and loaded as
dependencies but have zero call sites. Do not introduce libavcodec/libavformat to "encode" or
"mux" — extend the MediaCodec path instead.

Vendored version is **FFmpeg 3.2.12** (pre-5.x API). Soname majors are pinned in CMake IMPORTED
targets and in the loader: avcodec-57, avdevice-57, avfilter-6, avformat-57, avutil-55,
postproc-54, swresample-2, swscale-4. Match that era — no `AVChannelLayout`; use the older
`av_frame_get_buffer(frame, 32)` alignment style. Only `.so` are vendored (no static `.a`), one
`include/` + eight `.so` per ABI folder under `ffmpegv2/<abi>/` (`arm64-v8a`, `armeabi-v7a`,
`x86`, `x86_64`).

`AVFrame` ownership is asymmetric — the double-free / use-after-free trap:

- `VideoEncoder::encodeFrame(AVFrame*)` **takes ownership**: it clones internally and frees the
  caller's frame.
- The renderer's record callback (`RecordFrameCallback`) is the opposite: the **renderer owns**
  the passed frame and frees it after the callback returns, so a consumer that needs to keep it
  must `av_frame_clone`/`av_frame_ref` first.

Confirm which side owns each frame before editing a hand-off; mismatching these corrupts memory.

Respect threading and format invariants. A `SwsContext` is never shared across threads: `recordSws`
is touched only on the EGL (`egl_renderer`) thread; the controller's `sws_`/`yuv_` are guarded by
`frameMtx_`. Keep that isolation. Encode/record width and height must stay even (`& ~1`) — H.264
and the NV12 conversion assume it. The canonical internal frame format is `AV_PIX_FMT_YUV420P`;
`AV_PIX_FMT_RGBA` is only the GL-readback source.

Treat packaging as a contract, not a detail. The FFmpeg `.so` are `add_library(... SHARED
IMPORTED)` targets linked into `libcamera.so`, so AGP's `externalNativeBuild` copies them into the
native-lib merge — that is how they reach the APK/AAR. `jniLibs.srcDir("src/main/libs")` in
`camera/build.gradle.kts` is vestigial (the directory does not exist); do NOT "fix" it by copying
`.so` there — that double-packages. Load order in `JNILibraryLoader.kt` (all FFmpeg libs before
`camera`) is load-bearing because `libcamera.so` depends on the FFmpeg sonames. Keep the ARM
`abiFilters` and 16 KB page alignment (`CMAKE_ANDROID_PAGE_SIZE 16384`, `--page-size=16384`)
aligned. `ffmpegv2` is a read-only vendored input for ordinary feature/fix work; upgrading it
(new sonames, loader order, CMake IMPORTED majors) is a separate scoped packaging task requiring
its own approval.

Verification must distinguish proof levels. A native/Gradle compile and APK/AAR inspection prove
linkage, soname resolution, and packaging; only a supported-device/ABI run proves frame
conversion, swscale correctness, and recording behavior. Never claim runtime behavior from
compilation alone.
