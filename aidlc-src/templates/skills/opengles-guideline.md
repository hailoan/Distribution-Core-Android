---
name: OpenGL ES Guideline
description: Protect the single-thread EGL context, the YUV-texture (non-OES) upload path, Kotlin-driven shader assembly, and the FBO/PBO capture pipeline when camera GL/EGL/GLSL surfaces are touched. Read-only guidance.
---

## OpenGL ES Guideline

Load only when a change touches the camera GL stack: `camera/src/main/cpp/gl/**`,
`camera/src/main/assets/glsl/*.glsl`, the Kotlin shader glue
(`utils/ShaderBuilder.kt`, `model/AdjustType.kt`, `utils/AssetReader.kt`), EGL context/thread
lifecycle, or the FBO/PBO capture/record path. This is the detail behind
`native-boundary-guideline`; keep that umbrella guard in effect too. All GL/EGL code is C++ (NDK)
driven from Kotlin over JNI — there is no `GLSurfaceView`/`android.opengl.*` path.

Everything runs on one GL thread. Every GL/EGL call must be marshalled onto `EGLRenderer`'s
`thread->runInThread(...)` (the `egl_renderer` `SingleThreadExecutor`). The single EGL context
(GLES 3.0, shaders are `#version 300 es`) is created on and current only on that thread, and it
serves preview, one-shot capture, and record. Never call GL from the Camera2 image thread, an
encoder thread, or a JNI caller thread.

There is no external OES texture / SurfaceTexture path — the biggest gotcha. Do not assume
`samplerExternalOES`, `GL_TEXTURE_EXTERNAL_OES`, `SurfaceTexture`, or `updateTexImage` exist; they
are absent. Camera frames arrive as YUV420P `AVFrame` and are uploaded every frame as three
`GL_R8` textures (Y at w×h, U and V at w/2×h/2) in `bindUniformYUV`, honoring
`GL_UNPACK_ROW_LENGTH = linesize[n]` per plane and `GL_UNPACK_ALIGNMENT 1`. YUV→RGB happens in the
fragment shader (`getColor` in `assets/glsl/base_fragment_shade_top_camera.glsl`, BT.601
constants), not on the CPU and not via an OES sampler.

Shader assembly lives in Kotlin, so edits span two languages. `ShaderBuilder.buildShader`
concatenates, in order: `base_fragment_shade_top_camera.glsl` + `frag_base_shader_blend.glsl` +
`frag_base_shader_adjust.glsl` + the filter string + the effect string + a generated `main()`. The
adjustment chain and its order are defined by the `AdjustType` enum (`model/AdjustType.kt`), each
entry emitting one GLSL statement — not by C++. Uniform names are duplicated between the C++
`VideoGl` `cU*` name constants (`gl/video_gl.h`) and the GLSL `uniform` declarations; they must
stay in sync, and `initProgram` re-resolves every attribute/uniform location after each program
rebuild (on `initVideoGL` and every `updateFilter`). Shaders are compiled at runtime from the
strings passed via JNI (`nativeInit`, `changeFilter`); there is no precompiled program binary.
Filter/overlay/LUT textures cross the `changeFilter` boundary as **direct** `ByteBuffer`s
(`GetDirectBufferAddress`) — non-direct buffers break the zero-copy read.

The record/capture path is FBO + PBO, EGL-thread-only. There is exactly one record FBO/texture
(`GL_RGBA8`, `recordFbo`/`recordTex`) plus a double-buffered PBO ring (`recordPbo[2]`,
`GL_PIXEL_PACK_BUFFER`, `GL_STREAM_READ`), built in `ensureRecordResources`, rebuilt on size
change, freed in `releaseRecordResources`; there is no EGL pbuffer surface. Recording renders the
same filter pass into the FBO first (`renderRecordPass`, before the screen pass), forcing
`rotation=0` (MP4 rotation is a muxer hint), while the on-screen pass applies `-sensorOrientation`.
Readback is asynchronous (one-frame latency): `glReadPixels` into the current PBO, then map the
previous PBO (`glMapBufferRange`, `GL_MAP_READ_BIT`) and memcpy into staging; the first frames
after start emit nothing while the ring primes. Both `glReadPixels` paths are bottom-left origin —
the photo path is flipped in Kotlin (`GLPreview.captureFrame`), the record path via a negative
swscale source stride. Even record dimensions (`& ~1`) are required. See `ffmpeg-guideline` for the
`sws_scale` / `AVFrame` half of this hand-off.

Know the stubs and gaps before relying on them. `nativeCleanup` and `nativeRender` are empty; the
EGL `cleanup()` is not called on surface destroy. There is currently no JNI setter that pushes
`VideoConfigure` adjustment values (brightness/contrast/saturation/HSL shifts/…) to the shader
uniforms — those uniforms use the C++ `VideoGl` struct defaults, and Kotlin `setBrightness` maps
to HAL AE exposure compensation (`nativeSetExposureCompensation`), not the shader `u_brightness`.
Do not document a live-adjustment path that is not wired.

Verification must distinguish proof levels. A native compile proves the shader-string wiring and
JNI signatures compile and link; only a supported-device run proves shader correctness, YUV upload,
filtering, capture, and record rendering. Never claim GL runtime behavior from compilation alone.
