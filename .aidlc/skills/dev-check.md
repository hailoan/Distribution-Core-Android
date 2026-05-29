# Dev Check Skill

You are a Senior Android Developer doing a pre-PR self-review on **Distribution-Core-Android**.

Input: `output/feature/`, `output/UNIT-TEST-REPORT.md`, `output/CODE-REVIEW.md`

Fast, targeted runtime-safety check — not a full review.

---

## Hilt DI
- Every injected class has a `@Provides` in a `@Module @InstallIn(...)` class.
- `@HiltViewModel` on every ViewModel with constructor-injected dependencies.
- Multiple `IClient` bindings use a `@Qualifier @Retention(AnnotationRetention.BINARY)` annotation — missing qualifier = Hilt compile error "multiple bindings".
- No `@Singleton` capturing a non-`@ApplicationContext` `Context`.

---

## Network / Token (`InitNetwork`)
- `InitNetwork.xxxTokenManager` / `xxxRetryToken` set in `Application.onCreate()` **before** any Hilt-provided component is used — late assignment = null bearer token, no NPE but requests will be unauthenticated.
- `RetryTokenManager.handleRefreshToken {}` used in `IRetryToken.refreshTokensApi()` — bypass = double refresh race on token storage.
- No `Authorization` header hardcoded anywhere.
- No `TODO("Not yet implemented")` left in `ITokenManager` or `IRetryToken` implementations — these crash at the first network call.

---

## `BaseViewModel` coroutine safety
- `sendMutation(mutation)` called from inside `viewModelScope.launch {}` — it suspends (`Channel.send`); calling outside a coroutine throws.
- `handleAction` does not call suspend functions directly — launch a coroutine first.
- No `Dispatchers.Main` for IO work inside ViewModel.

---

## General coroutine safety
- No `GlobalScope`.
- No `runBlocking` on the main thread — use `lifecycleScope.launch {}` in Activities/Fragments.
- `ReentrantMutex` (`:core`) used instead of `Mutex` when the same coroutine context re-enters the lock — plain `Mutex` deadlocks.

---

## Camera / NDK / C++ / OpenGL ES 3 / FFmpeg
- **JNI thread safety:** callbacks from the native EGL thread or encoder thread that call back into Kotlin must `g_jvm->GetEnv` + `AttachCurrentThread` / `DetachCurrentThread`. Direct JNI calls from unattached threads = crash.
- **JNI ref leaks:** every `NewGlobalRef` has a paired `DeleteGlobalRef`. Every `FindClass` / `GetObjectClass` local ref inside a loop is released with `DeleteLocalRef`.
- **GL thread rule:** all `gl*` calls run only on the EGL thread (`SingleThreadExecutor("egl_renderer")`). GL calls from `AImageReader` callback or encoder thread = undefined behavior.
- **AVFrame lifecycle:** allocated with `av_frame_alloc()`, freed with `av_frame_free(&ptr)`. Never `delete frame`.
- **SwsContext lifecycle:** freed with `sws_freeContext(sws_)`, not `free()`.
- **16 KB page alignment:** `CMAKE_ANDROID_PAGE_SIZE 16384` must remain in `CMakeLists.txt` — removing it breaks Android 15+ load on devices with 16 KB pages.
- **New GLSL uniform:** name in shader GLSL must match the `cU*` string constant in `VideoGl` exactly. Mismatch = `glGetUniformLocation` returns -1 = silent no-op, not a crash. Must be caught in visual testing.
- **New `external fun`:** matching `JNIEXPORT JNICALL Java_com_chiistudio_camerandk_jni_NativeRenderer_<name>` must exist in `camera_native.cpp`. Missing = `UnsatisfiedLinkError` at runtime.
- **Camera mode guard:** `startRecording` / `stopRecording` only valid in `CaptureMode::VIDEO`. Calling in PHOTO or PREVIEW = early return or assertion in `CameraController`.
- **`JNILibraryLoader.initData()`** must be called before any `NativeRenderer.external` method — calling externals before the `.so` is loaded = crash.

---

## Room / Database
- `Migration` provided for every schema change.
- Live DAOs return `Flow<List<T>>`, not `List<T>`.
- No string concatenation in `@Query`.

---

## Dead code / debug artifacts
- No `Log.e("LOAN", ...)` in `:core`, `:network`, `:camera` — logging in libraries pollutes the consumer's logcat.
- No `TODO("Not yet implemented")` in Hilt-provided classes or JNI callbacks.
- No commented-out code.

---

## Build / publishing
- New dependency added to `gradle/libs.versions.toml`.
- `GITHUB_USERNAME`, `GITHUB_ACCESS_TOKEN`, `GITHUB_PUBLISH` never committed.
- `CMAKE_ANDROID_PAGE_SIZE 16384` not removed from `CMakeLists.txt`.
- New `.cpp` source file added to `add_library(camera SHARED ...)` in `CMakeLists.txt`.

---

## Output format — save to `output/DEV-CHECK.md`
1. Hilt DI (Pass / Issues found)
2. Network / Token (Pass / Issues found)
3. `BaseViewModel` coroutine safety (Pass / Issues found)
4. General coroutine safety (Pass / Issues found)
5. Camera / NDK / C++ / GL / FFmpeg (Pass / Issues found)
6. Room / Database (Pass / Issues found)
7. Dead code / debug artifacts (Pass / Issues found)
8. Build / publishing (Pass / Issues found)
9. Summary — items to fix, or "Ready to push" if all pass
