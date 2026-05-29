# Code Review Skill

You are a Principal Android Engineer on **Distribution-Core-Android**.

Input: `output/feature/`, `output/UNIT-TEST-REPORT.md`

---

## `:core` — `BaseViewModel`
- State mutated **only** inside `handleMutation(mutation, state): S` — never via `setState()` from `handleAction`.
- `sendMutation()` called inside a `viewModelScope.launch {}` — it calls `Channel.send()` which suspends.
- `sendEffect()` used for one-shot events, not persistent state.
- `handleAction` dispatches work by launching coroutines; no direct suspend calls.
- No business logic in Composables — they emit `sendAction()` and collect `state` / `effect` only.
- `ReentrantMutex` used (not `Mutex`) when the same coroutine context re-enters the critical section.

---

## `:network` — Hilt + Ktor
- New service follows the four-class chain: `BearXxxClient` → `BearXxxAuth` → `BearXxxHeader` → `BearXxxService` (see weather package).
- `RetryTokenManager.handleRefreshToken {}` inside `IRetryToken.refreshTokensApi()` — no direct parallel refresh calls.
- `InitNetwork.xxxTokenManager` / `xxxRetryToken` set before Hilt graph (in `Application.onCreate()`).
- `ContentType.Application.Json` on every JSON request body.
- Response deserialization errors mapped to a typed result/error.
- No hardcoded base URLs or tokens anywhere.
- `@Qualifier @Retention(AnnotationRetention.BINARY)` annotation for every multiple binding of the same interface type.
- No `@Inject` constructor on `abstract` class or `interface`.

---

## `:camera` — NDK / C++ / OpenGL ES 3 / FFmpeg

### JNI bridge (`camera_native.cpp`)
- Every new `external fun` in `NativeRenderer.kt` has a matching `JNIEXPORT JNICALL Java_com_chiistudio_camerandk_jni_NativeRenderer_<name>` in `camera_native.cpp`.
- Callbacks fired from native threads (EGL thread, encoder thread) use the `g_jvm->GetEnv` / `AttachCurrentThread` / `DetachCurrentThread` pattern — no direct JNI calls without attaching.
- `NewGlobalRef` used for Kotlin callback objects that must outlive the JNI call frame; `DeleteGlobalRef` called after the last use.
- `DeleteLocalRef` on every JNI local reference created in a loop (prevent ref table overflow).
- No JNI exception left unchecked — `ExceptionCheck()` + `ExceptionClear()` after every `CallVoidMethod`.

### Camera2 NDK (`camera_controller.cpp`)
- All `ACameraManager` / `ACameraDevice` / `ACameraCaptureSession` / `AImageReader` handles properly released in `release()` and `closeDevice()`.
- `ACaptureRequest` tag fields (`ACAMERA_CONTROL_AF_MODE`, `AE_REGIONS`, etc.) applied via `ACameraMetadata_const_entry` — no raw integer literals without matching `ACAMERA_*` constant.
- `CaptureMode` enum (PREVIEW / PHOTO / VIDEO) respected before operations (`startRecording` only valid in VIDEO mode, etc.).
- `sws_scale` called with the correct source format (`AV_PIX_FMT_NV21` or `AV_PIX_FMT_YUV420P`) matching the `AImageReader` format (`AIMAGE_FORMAT_YUV_420_888`).
- Frame `AVFrame*` allocated with `av_frame_alloc()` and freed with `av_frame_free()` — never `delete`.
- `std::mutex frameMtx_` held while accessing `yuv_` / `sws_` from the image callback thread.

### OpenGL ES 3 (`cpp/gl/`)
- All GL calls execute on the EGL thread (`SingleThreadExecutor("egl_renderer")`) — no GL calls from Camera2 image callbacks or encoder threads.
- New shader uniforms declared in both the GLSL source (in `assets/glsl/`) and the `VideoGl` struct (`cU*` name constant + `GLint u*` member).
- `glGetUniformLocation` called after `glUseProgram`; result checked for `-1` before upload.
- `checkGlError()` (`gl_unit.h`) called after every state-changing GL call in debug builds.
- Offscreen FBO / PBO resources allocated on the EGL thread and freed on `stopRecordCapture()` — no resource leaks.
- 16 KB page alignment flag (`CMAKE_ANDROID_PAGE_SIZE 16384`) preserved in `CMakeLists.txt`.

### GLSL shaders (`assets/glsl/`)
- New adjustments added to `AdjustType` enum with a correct `glShader` GLSL snippet; snippet injects into the `main()` body via `ShaderBuilder.buildShader()`.
- Uniform names in GLSL match the `cU*` constant string in `VideoGl` exactly (typo = silent black screen).
- Curve/overlay textures bound to the correct texture unit (`u_texture_curve`, `u_texture_overlay`, `u_texture_filter0/1/2`).

### FFmpeg
- Correct FFmpeg library linked in `CMakeLists.txt` when adding video/audio features (link `avcodec-57`, `swscale-4`, etc.).
- `SwsContext` freed with `sws_freeContext()`, not `free()`.
- No API from a library not listed in `CMakeLists.txt target_link_libraries`.

---

## Compose UI
- Composables stateless; state hoisted to ViewModel.
- No `remember { mutableStateOf(...) }` holding business state.
- `LaunchedEffect` keys stable and intentional.

## Library API surface
- Public API minimal; implementation details `internal`.
- `consumer-rules.pro` updated for new public classes.

---

## Output format — save to `output/CODE-REVIEW.md`
1. Critical Issues (must fix before merge)
2. Major Issues (should fix)
3. Minor Issues (nice to fix)
4. Performance Findings
5. Security Findings
6. Approval Status (Approved / Approved with comments / Request changes)
