# Integration Test Skill

You are a QA Engineer on **Distribution-Core-Android**.

Input: `output/CODE-REVIEW.md` (must be Approved or Approved with comments)

Note: Actual device execution requires a human engineer on Android 7+ (minSdk 24). Camera / NDK tests require a **physical device** — emulators lack hardware camera.

---

## `:network` — Ktor + token lifecycle

| Scenario | What to verify |
|---|---|
| Valid token | `ITokenManager.getAccessToken()` called; `Authorization: Bearer <token>` present |
| Pre-emptive refresh | `IRetryToken.shouldRetryToken()` = true → `refreshTokensApi()` before request |
| Reactive refresh (401) | `needRetryToken(response)` = true → `refreshTokensApi()` → request retried |
| Parallel 401 race | Two coroutines hit 401 simultaneously → `RetryTokenManager` ensures **one** refresh, both get `Result.success()` |
| Refresh failure | `refreshTokensApi()` throws → both callers get `Result.failure()` |
| `InitNetwork` not set | `InitNetwork.xxxTokenManager == null` → no NPE, but no `Authorization` header |

Run: `./gradlew :network:test`

---

## `:core` — `BaseViewModel` + `ReentrantMutex`

| Scenario | What to verify |
|---|---|
| Concurrent mutations | N actions dispatched simultaneously → mutations processed FIFO; final state deterministic |
| Debounce (cancel + re-dispatch) | `Action.Search` with job cancel → only last text reaches mutation |
| Effect delivery | `sendEffect()` → `SharedFlow` collector receives it exactly once |
| Reentrant lock | Same coroutine enters `withReentrantLock` recursively → no deadlock |
| Mutual exclusion | Two coroutines → second waits until first exits lock; `results` order is `[1, 2, 3]` |

Run: `./gradlew :core:test`

---

## `:camera` — NDK / C++ / OpenGL ES 3 / FFmpeg (physical device required)

### JNI / library loading
- `JNILibraryLoader.initData()` called before any `NativeRenderer.external` method → no `UnsatisfiedLinkError`.
- All FFmpeg `.so` files load for the device ABI (arm64-v8a on most modern phones).
- 16 KB page test: install the AAR on an Android 15 device with 16 KB page size and verify no `SIGSEGV` on load.

### Camera2 NDK lifecycle
| Scenario | What to verify |
|---|---|
| `GLPreview` created + surface ready | `NativeRenderer.nativeInit` → Camera opens, preview renders |
| Lens switch | `nativeSetLens(1)` (front) → camera reopens, preview continues without freeze |
| Mode switch PREVIEW→VIDEO | `nativeSetMode(VIDEO)` → `startRecording` returns true, MP4 file created |
| Recording stop | `stopRecording { path }` → MP4 playable, rotation hint correct |
| Photo capture | `captureFrame { bitmap }` → bitmap non-null, correct orientation (vertical flip applied) |
| Focus tap | `nativeFocusAt(x, y, w, h)` → AF regions set, one-shot AF fires |
| AE compensation | `nativeSetExposureCompensation(ev)` → brightness visually changes |
| Surface destroyed | `nativeCleanup()` → no crash, camera released |

### OpenGL ES 3 shader pipeline
| Scenario | What to verify |
|---|---|
| Filter apply | `GLPreview.applyFilter(filterPath, opacity, overlayList)` → visual change in preview |
| `.acv` curve filter | ACV file parsed by `CurveTone`, 256×1 LUT passed to `u_texture_curve` → curve visible |
| Bitmap overlay | Overlay bitmap passed to `u_texture_overlay` → overlay visible at correct opacity |
| No uniform mismatch | Every `AdjustType` enum entry's GLSL snippet compiles without `GL_INVALID_OPERATION` |

### FFmpeg video encoding
| Scenario | What to verify |
|---|---|
| H.264 MP4 | Output file has valid moov atom, plays in standard player |
| Bitrate = 0 | Resolution-derived default applied (~6 Mbps at 1080p30), file size reasonable |
| Double-buffered PBO | First 1–2 frames may drop (PBO priming) — no crash, encoding continues |
| sws_scale RGBA→YUV420P | No color artifacts in recorded video |

---

## Build commands
```bash
./gradlew :core:test
./gradlew :network:test
./gradlew :core:test --tests "com.chiistudio.core.BaseViewModelUnitTest"
./gradlew :network:test --tests "com.chiistudio.network.RetryTokenUnitTest"
./gradlew connectedAndroidTest          # instrumented — requires device/emulator
./gradlew :camera:assembleDebug         # triggers CMake build; verify no compile errors
```

---

## Output format — save to `output/INTEGRATION-TEST.md`
1. Integration Test Plan (HTTP / Token Refresh / ViewModel / Camera / NDK)
2. Token Refresh Race Scenarios
3. Camera / NDK Scenarios (JNI loading, lifecycle, GL pipeline, FFmpeg)
4. Device Compatibility Matrix (minSdk 24, targetSdk 35; camera tests = physical device)
5. Known Risks and Mitigations
6. Execution Checklist for QA Engineer
