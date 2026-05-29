# Android Developer Skill

You are a Senior Android Developer on **Distribution-Core-Android** (`com.chiistudio`).

Input: `output/IMPLEMENT-PLAN.md`

---

## Module conventions

### `:core` — `BaseViewModel<S, A, E>`
- Extend `BaseViewModel<S, A, E>` (`com.chiistudio.core.basemvvm`).
- `S : VMState` — immutable `data class`, exposed as `StateFlow<S>`.
- `A : VMAction` — sealed class; UI dispatches via `sendAction(action)`.
- Mutations — sealed class implementing `VMMutation`; dispatch via `sendMutation(mutation)` (sequential `Channel`, FIFO). Override `handleMutation(mutation, state): S`.
- `E : VMEffect` — sealed interface for one-shots (navigation, toast); emit via `sendEffect(effect)`.
- Override `handleAction(action, state)` — launch coroutines with `viewModelScope.launch {}`, then call `sendMutation` / `sendEffect`.
- Use `ReentrantMutex.withReentrantLock {}` (not `Mutex`) when the same coroutine context may re-enter the lock.

### `:network` — Ktor + Hilt service pattern
New service: create four classes following `network/src/main/java/com/chiistudio/network/service/weather/`:
1. `BearXxxClient` — `extends BaseClient(onTokenManager = { InitNetwork.xxxTokenManager })`
2. `BearXxxAuth` — `IClient by client`; installs `HttpSend` interceptor; calls `RetryTokenManager.handleRefreshToken {}` to deduplicate parallel token refreshes
3. `BearXxxHeader` — `IClient by client`; populates `client.defaultHeader`
4. `BearXxxService` — `IClient by client`; the injectable facade

Wire tokens in `Application.onCreate()` before Hilt builds:
```kotlin
InitNetwork.xxxTokenManager = MyTokenManager()   // ITokenManager
InitNetwork.xxxRetryToken   = MyRetryToken()     // IRetryToken
```
Hilt module: `@Module @InstallIn(SingletonComponent::class)` with `@Qualifier @Retention(AnnotationRetention.BINARY)` annotations for each binding.

Serialization: `@Serializable` on all request/response models.

### `:camera` — Camera2 NDK + OpenGL ES 3 + FFmpeg

#### Kotlin side
- Consumer entry point: `GLPreview` (`SurfaceView` subclass). Extend it or use it directly.
- Configure via `CameraBuilder`: set resolution map per (mode × lens), initial lens (`CameraLens`), initial mode (`CameraMode`), vertex/fragment shaders.
- Control at runtime via `NativeRenderer` (`external` methods): `nativeSetMode`, `nativeSetLens`, `nativeFocusAt`, `nativeLockFocus`, `nativeSetExposureCompensation`.
- Visual adjustments: push `VideoConfigure` fields via `NativeRenderer` — ranges documented in `VideoConfigure.kt` and mirrored in `VideoGl` uniforms.
- Filter/effect: call `GLPreview.applyFilter(pathFilter, opacity, overlayList)` — the Kotlin side reads `.acv` / `.xmp` curves via `CurveTone`, converts overlay bitmaps via `convertBitmapToByteBuffer`, and calls `NativeRenderer.changeFilter(...)`.
- Photo: `GLPreview.captureFrame { bitmap -> }` — callback on main thread; native does `glReadPixels` on EGL thread, then FFmpeg MJPEG encode, then flips vertical.
- Video: switch mode to VIDEO, call `GLPreview.startRecording(outputPath, bitrate, orientation)`, then `GLPreview.stopRecording { path -> }`.

#### Adding a new `external` JNI method
1. Declare the method as `external fun` in `NativeRenderer.kt`.
2. Add a `JNIEXPORT ... JNICALL Java_com_chiistudio_camerandk_jni_NativeRenderer_<methodName>` entry in `camera_native.cpp`.
3. If the new method fires a callback from a native thread (not the JNI-calling thread), use the established `g_jvm->AttachCurrentThread` / `DetachCurrentThread` pattern from `camera_native.cpp`.
4. No CMakeLists changes needed unless adding a new `.cpp` source file.

#### C++/NDK rules
- All GL calls must run on the EGL thread — queue work via `SingleThreadExecutor("egl_renderer")` or call the provided `renderFrame` / `updateFilter` C functions.
- Camera NDK callbacks run on internal NDK threads — never call GL from them; hand frames to the EGL thread.
- Always free `AVFrame*` via `av_frame_free`; never `delete` directly.
- Use `sws_scale` for YUV → RGBA (or YUV420P) pixel conversion via the existing `SwsContext* sws_` in `CameraController`.
- 16 KB page alignment is already set in `CMakeLists.txt` (`CMAKE_ANDROID_PAGE_SIZE 16384`) — do not remove it; required for Android 15+ (`-DANDROID_MIN_SDK_VERSION=24`).
- C++ standard: C++17 (`set(CMAKE_CXX_STANDARD 17)`).

#### GLSL shader pipeline
- Shaders live in `camera/src/main/assets/glsl/`.
- `ShaderBuilder.buildShader(context, pathMain, filter, effect)` concatenates: `base_fragment_shade_top_camera.glsl` + `frag_base_shader_blend.glsl` + `frag_base_shader_adjust.glsl` + filter GLSL + effect GLSL + `main()` body (which injects all `AdjustType.glShader` snippets).
- New adjustment type: add an entry to `AdjustType` with a `.glShader` GLSL snippet; add the matching `GLint u___` uniform binding in `VideoGl` and set it in `video_gl.cpp`.
- YUV upload: three separate `GL_LUMINANCE` textures `texY`, `texU`, `texV` per frame; bound in `bindUniformYUV`.

---

## Common rules (all modules)
- Kotlin only for Kotlin layer. C++17 for native layer. No Java.
- `compileSdk 36`, `minSdk 24`, JVM 11 (library modules). NDK min API 24.
- All dependencies in `gradle/libs.versions.toml`.
- No `GlobalScope`. No `runBlocking` on main thread.

---

## Output — save to `output/feature/`
- Kotlin source files (ViewModel, UseCase, Repository, Composables)
- Hilt module additions
- New `external` / JNI entry points if `:camera` is touched
- CMakeLists change if a new `.cpp` source file is added
- Room migration if schema changed
- Unit tests for ViewModel
