# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
./gradlew assembleDebug          # Build debug AAR
./gradlew assembleRelease        # Build release AAR
./gradlew test                   # Run unit tests
./gradlew connectedAndroidTest   # Run instrumented tests (requires device/emulator)

# Run a single unit test
./gradlew test --tests "com.chiistudio.camerandk.ExampleUnitTest.addition_isCorrect"

# Run a single instrumented test
./gradlew connectedAndroidTest --tests "com.chiistudio.camerandk.ExampleInstrumentedTest.useAppContext"
```

**Required env vars for publishing:**
- `GITHUB_USERNAME`, `GITHUB_ACCESS_TOKEN`, `GITHUB_PUBLISH` — used by `settings.gradle.kts` for the GitHub Maven repository.

**Coordinates:** `com.chiistudio:camerandk:1.0.0`, min SDK 24, compile SDK 36, JVM 17, NDK 29.

## Architecture

This is an Android library (`com.chiistudio.camerandk`) that provides a camera preview with real-time OpenGL ES 3.0 shader-based filtering, backed by the Camera2 NDK and pre-compiled FFmpeg.

### Layer stack (top → bottom)

1. **Kotlin API** — `GLPreview` (SurfaceView subclass) is the main entry point. Consumers configure it via `CameraBuilder`, call adjustment methods from `IAdjustVideo`/`IFilter`/`IColorVideo`, and drive video parameters through `VideoConfigure`.

2. **JNI bridge** — `NativeRenderer` (singleton object) declares all `external` methods. `JNILibraryLoader.initData()` must be called first to load FFmpeg libs and the `camera` `.so` in dependency order.

3. **C++ core** (`src/main/cpp/`):
   - `camera_native.cpp` — JNI entry points wired to `Java_com_chiistudio_camerandk_jni_NativeRenderer_*`.
   - `camera/` — `CameraController` wraps Camera2 NDK (`ACameraManager`, `ACameraDevice`, `ACameraCaptureSession`). Supports PREVIEW/PHOTO/VIDEO modes and 480p–4K resolution presets.
   - `gl/` — `EGLRenderer` owns the EGL context and runs on a `SingleThreadExecutor`. `VideoGL` and `GLUnit` handle shader programs and YUV texture upload.
   - `ffmpegv2/` — pre-compiled FFmpeg (avcodec-57, swscale-4, etc.) for each ABI (arm64-v8a, armeabi-v7a, x86, x86_64).

### Shader pipeline

- GLSL source lives in `src/main/assets/glsl/`.
- `ShaderBuilder.buildShader()` composes a final fragment shader by concatenating base shader files with per-adjustment GLSL snippets from the `AdjustType` enum.
- `changeFilter()` on `NativeRenderer` passes compiled vertex + fragment shader strings plus optional `.acv` / `.xmp` curve data to the native layer.

### Key model types

| Class | Purpose |
|---|---|
| `CameraBuilder` | Builder for resolution, shaders, duration |
| `VideoConfigure` | All numeric adjustments with documented value ranges (e.g. brightness −0.5…0.5) |
| `AdjustType` | Enum → GLSL snippet mapping for each adjustment |
| `AdjustColorType` | 8 color ranges (RED, ORANGE … PINK) for HSL color mixing |
| `Vec3` | Kotlin data class mirrored in C++ for level/color values |

### Curve / filter support

- `AssetReader.parseACV()` parses Photoshop `.acv` binary files; `interpolateCurve()` produces a 256-point LUT.
- `CurveFilter` performs spline interpolation for smooth curve application.
- Image overlays are passed as a `List<String>` of asset paths through `IFilter.changeFilter()`.
