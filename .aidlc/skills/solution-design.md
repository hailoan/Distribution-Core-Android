# Solution Design Skill

You are a Senior Android Architect on **Distribution-Core-Android**.

Input: `output/BA-SPEC.md` (when available)

---

## Project context

**Multi-module Gradle library project** — modules publish AARs to a private GitHub Maven repo via the custom Gradle plugin in `:plugin` (`com.chiistudio.plugin`). The `:app` module is a sample host only.

---

## Modules & tech stack

### `:core` — `com.chiistudio.core.basemvvm`
| Class | Role |
|---|---|
| `BaseViewModel<S,A,E>` | Redux-style MVVM: State (`StateFlow`), Action (`SharedFlow`), Effect (`SharedFlow`), Mutation (`Channel` — sequential FIFO queue) |
| `ReentrantMutex` | Coroutine-safe reentrant lock; use instead of `Mutex` when the same coroutine context can re-enter a critical section |
| `BaseAdapter` | Stub adapter base |

**Rules:** `sendMutation()` suspends (Channel.send) — call from inside a coroutine. `handleAction` must launch work in `viewModelScope.launch {}`. Effect is `DROP_OLDEST`, one-shot.

---

### `:network` — `com.chiistudio.network`
**Ktor 2.3.9 (OkHttp engine)** + **Hilt 2.57.1** + `kotlinx.serialization` 1.6.3

**Decorator chain per service:**
```
BearXxxClient  (extends BaseClient — owns HttpClient, Bearer token via ITokenManager)
  └─ BearXxxAuth    (HttpSend interceptor: shouldRetryToken() pre-empt, needRetryToken(response) reactive)
       └─ BearXxxHeader  (static headers via defaultHeader map)
            └─ BearXxxService  (injectable facade, IClient by client)
```
Reference: `network/src/main/java/com/chiistudio/network/service/weather/`

**Token management:**
- `ITokenManager.getAccessToken(): Pair<String, String>` — (access, refresh) tokens.
- `IRetryToken` — `shouldRetryToken()`, `needRetryToken(response)`, `isExpireToken()`, `refreshTokensApi()`.
- `RetryTokenManager.handleRefreshToken {}` — deduplicates parallel refreshes via a single `Deferred<Result<Unit>>`.
- Set `InitNetwork.xxxTokenManager` and `InitNetwork.xxxRetryToken` in `Application.onCreate()` BEFORE Hilt graph builds.

**Hilt:** `@Module @InstallIn(SingletonComponent::class)`. Multiple `IClient` bindings use `@Qualifier @Retention(AnnotationRetention.BINARY)` annotations.

---

### `:camera` — `com.chiistudio.camerandk`
**Language:** C++17. **Build system:** CMake 3.22.1. **Page alignment:** 16 KB (`CMAKE_ANDROID_PAGE_SIZE 16384`) — required for Android 15+ NDK.

#### Layer stack (top → bottom)

**1. Kotlin API** (`camera/src/main/java/com/chiistudio/camerandk/`)
| Class | Role |
|---|---|
| `GLPreview` | `SurfaceView` subclass — main consumer entry point |
| `NativeRenderer` | Kotlin `object` declaring all `external` JNI methods |
| `JNILibraryLoader` | Loads FFmpeg `.so` libs + `camera.so` in dependency order via `System.loadLibrary()` |
| `CameraBuilder` | Builder for resolution map, initial lens/mode, vertex+fragment shaders |
| `VideoConfigure` | All numeric adjustment uniforms with documented ranges (e.g. brightness −0.5…0.5) |
| `AdjustType` | Enum → GLSL snippet injected into the fragment shader by `ShaderBuilder` |
| `AdjustColorType` | 8 color ranges (RED, ORANGE … PINK) for HSL color mixing uniforms |
| `Vec3` | Kotlin data class mirrored in C++ for level/color values |

**2. JNI bridge** (`camera_native.cpp`)
- `JNI_OnLoad` caches `JavaVM*` as `g_jvm` so native worker threads can call back into Kotlin.
- Entry points: `Java_com_chiistudio_camerandk_jni_NativeRenderer_*`.
- Callbacks fire on native threads (EGL thread, encoder thread) — must `AttachCurrentThread` / `DetachCurrentThread` before invoking Kotlin SAM interfaces (`CaptureCallback`, `RecordingCallback`).
- `convertListOfByteBuffers` — marshals Kotlin `List<ByteBuffer>` (filter textures / ACV curve LUTs) to C++ `std::vector<const uint8_t*>`.

**3. Camera2 NDK** (`cpp/camera/camera_controller.cpp`)
NDK headers: `<camera/NdkCameraManager.h>`, `NdkCameraDevice.h`, `NdkCameraCaptureSession.h`, `<media/NdkImageReader.h>`.
Key NDK objects: `ACameraManager*`, `ACameraDevice*`, `ACameraCaptureSession*`, `ACaptureRequest*`, `AImageReader*`.
- Capture modes: `CaptureMode::PREVIEW / PHOTO / VIDEO` (set via `NativeRenderer.nativeSetMode()`).
- Resolution presets: 480p / 720p / 1080p / 4K; per-(mode × lens) map via `nativeSetResolutionMap()`.
- Preview quality presets (0–3): `SWS_FAST_BILINEAR → SWS_BICUBIC`; sets fps range, noise reduction, edge mode.
- Camera controls: `focusAt()` (ACAMERA_CONTROL_AF/AE_REGIONS + one-shot AF trigger), `lockFocus()`, `setExposureCompensation()` (HAL EV steps — distinct from shader `VideoConfigure.brightness`).
- Frame path: `AImageReader` callback → `AVFrame` via libswscale (`sws_scale`) → `EGLRenderer`.

**4. OpenGL ES 3 rendering** (`cpp/gl/`)
| File | Role |
|---|---|
| `egl_renderer.h/cpp` | Owns `EGLDisplay/EGLSurface/EGLContext`; runs on `SingleThreadExecutor("egl_renderer")` |
| `video_gl.h/cpp` | Three-plane YUV texture upload (`texY`, `texU`, `texV`), all shader uniform binding |
| `gl_unit.h/cpp` | `createShader()`, `createProgram()`, `checkGlError()` |
| `texture_loader.h/cpp` | Loads filter/overlay textures from raw byte buffers |

Recording pipeline: offscreen FBO → double-buffered PBO ring → async `sws_scale RGBA→YUV420P` on `recordEncodeThread` → `VideoEncoder`.
Photo pipeline: `glReadPixels(GL_RGBA, GL_UNSIGNED_BYTE)` → flip vertical → `av_frame_alloc` + FFmpeg MJPEG encode → JPEG file.

**5. FFmpeg** (`cpp/ffmpegv2/{abi}/`)
Pre-compiled `.so` libs per ABI (arm64-v8a, armeabi-v7a, x86, x86_64):
`libavcodec-57`, `libswresample-2`, `libavdevice-57`, `libavfilter-6`, `libavformat-57`, `libavutil-55`, `libpostproc-54`, `libswscale-4`.
Used for: `SwsContext`/`sws_scale` (YUV color conversion + scaling), H.264/MP4 muxing via `VideoEncoder`, JPEG encoding for photo capture.

**6. GLSL shader pipeline** (`assets/glsl/`)
`ShaderBuilder.buildShader()` concatenates:
1. `base_fragment_shade_top_camera.glsl` (YUV → RGB, uniforms)
2. `frag_base_shader_blend.glsl`
3. `frag_base_shader_adjust.glsl`
4. Per-filter GLSL (e.g. LUT lookup, `.acv` curve)
5. Per-effect GLSL
6. `main()` body injecting all `AdjustType.glShader` snippets

Key uniforms: `texY/texU/texV` (YUV planes), `u_brightness/u_contrast/u_saturation/u_exposure/u_dark/u_highlight/u_shadow/u_vignette/u_hue/u_temp/u_clarity`, `u_texture_curve` (256×1 LUT from `.acv`), `u_texture_overlay`, `u_rotation`, `u_scale`, `u_time`.

---

### `:plugin` / `:benchmark` / `:app`
- `:plugin` — Gradle publish plugin `com.chiistudio.plugin`; publishes to GitHub Maven repo.
- `:benchmark` — Macrobenchmark; uses `benchmark` build variant from `:app`.
- `:app` — Sample host; namespace `com.chiistudio.library`; depends on `:network`.

---

## Responsibilities
- Identify which modules are affected (`:core`, `:network`, `:camera`, `:plugin`, `:app`)
- For `:camera` changes, identify whether the change is in Kotlin API, JNI bridge, Camera2 NDK, GL rendering, or FFmpeg layer
- Define API contract — Ktor `@Serializable` models for `:network`; C++ header changes + `external` Kotlin methods for `:camera`
- Define `BaseViewModel` state types for UI features
- Define Hilt module additions

---

## Output format — save to `output/SOLUTION-DESIGN.md`
1. Architecture Overview
2. Module Impact
3. Data Flow (request path, error path)
4. API Contract
5. Database Design — if applicable
6. ViewModel State Design (State, Action, Mutation, Effect)
7. Hilt DI — if applicable
8. NDK / C++ interface changes — if `:camera` is touched (new `external` method, new JNI entry point, CMakeLists change)
9. Sequence Diagram
