AUTOMATION: CONTINUE

# UNIT-TEST-REPORT — preview-video-opengles-by-ndk

Testing stage for the `:videolib` OpenGL ES preview. Owned test contract from the CHANGESET Testing
Handoff = one Test-ID (**T-LOAD**, instrumented). No production behavior changed.

## 1. Test implementation

| FR-ID | SC-ID | AC-ID | Work-ID | production Task-ID | testing Task-ID | changed production path/symbol | Test-ID | test path/symbol | level | authored status |
|---|---|---|---|---|---|---|---|---|---|---|
| FR-1, FR-2 | SC-1 | AC-4, AC-5(load), AC-6 | W1 | T4 | T6 | `videolib/src/main/java/com/cii/videolib/VideoPreview.kt`; `Java_com_cii_videolib_VideoPreview_*` (`videolib.cpp`) | T-LOAD | `videolib/src/androidTest/java/com/cii/videolib/PreviewBindingTest.kt` → `videoPreview_constructs_loadsLibraryAndResolvesNativeCreate`, `previewNativeMethods_resolve_andNoOpSafely_withoutSurface`, `release_isIdempotent`, `pushFrame_ignoresUndersizedBuffer_withoutCrashing` | Android instrumented | authored |
| FR-2 | SC-1 | R3 (regression) | W1 | T4/T5 | T6 | `videolib.cpp` `Java_com_cii_videolib_NativeLib_*` (unchanged); `CMakeLists.txt` (+EGL/GLESv3) | T-LOAD | `PreviewBindingTest.kt` → `nativeLib_stringFromJNI_stillResolves_afterPreviewAdded`, `nativeLib_ffmpegVersion_stillResolves_afterPreviewAdded` | Android instrumented | authored (`added-by-testing` regression, per handoff R3 scope) |

Mirrors the module's existing instrumented convention (`FfmpegLinkageTest.kt`, `ExampleInstrumentedTest.kt`): `AndroidJUnit4` runner, `org.junit.Assert.*`, `com.cii.videolib` package, `androidTest` source set. No new test dependency introduced.

## 2. Coverage matrix

| Test-ID | behavior/transition/error/risk | fixture boundary | assertion scope | coverage status | evidence |
|---|---|---|---|---|---|
| T-LOAD | `loadLibrary("videolib")` + `nativeCreate` resolve (AC-5 load) | real `.so`, no `Surface` | construct→release without `UnsatisfiedLinkError` | covered (authored) | `videoPreview_constructs_...` |
| T-LOAD | all preview exports resolve + Idle-state no-op safety (AC-4 thread-marshalled path reachable; AC-6 no FFmpeg/video/camera on this path) | direct `ByteBuffer`, no `Surface` | `requestPattern`/`pushFrame`/`detachSurface` return without crash pre-attach | covered (authored) | `previewNativeMethods_resolve_andNoOpSafely_withoutSurface` |
| T-LOAD | `release()` idempotency (zero-handle guard; no double-free) | none | second `release()` is a safe no-op | covered (authored) | `release_isIdempotent` |
| T-LOAD | `pushFrame` Kotlin-side capacity guard | direct undersized buffer | undersized frame ignored, no native call, no crash | covered (authored) | `pushFrame_ignoresUndersizedBuffer_withoutCrashing` |
| T-LOAD | R3 regression: existing `NativeLib` exports unaffected by added preview code + EGL/GLESv3 link | real `.so` | `stringFromJNI`=="Hello from C++"; `nativeFFmpegVersion` contains "avformat" | covered (authored) | `nativeLib_*_stillResolves_afterPreviewAdded` |

## 3. Execution results

| Test-ID/command scope | exact command | environment | executed status | result | failure evidence |
|---|---|---|---|---|---|
| T-LOAD (all 6 cases) | `./gradlew :videolib:connectedDebugAndroidTest` (or `--tests com.cii.videolib.PreviewBindingTest`) | `adb` present, **no device/emulator connected** (`adb devices` = empty); instrumented test requires a device + NDK native build | **not executed** | — | environment blocker: no connected `arm64-v8a`/`armeabi-v7a` device or emulator; `emulator` binary not on PATH. Also requires private-repo env vars for Gradle configuration (packet §10). |

No test command was run to completion. Nothing is claimed as passed. No build/release task was run.

## 4. Failed cases and root cause

None — no test executed, so there is no product/test/environment *failure* to attribute. The blocker is a missing device (environment availability), not a defect.

## 5. Gaps and recommendations

| Gap | mapped to | recommendation |
|---|---|---|
| T-LOAD authored but not executed | T-LOAD / T4 | run `./gradlew :videolib:connectedDebugAndroidTest` on a connected GLES 3.0 device/emulator (both ABIs if available) in the integration-testing stage; requires private-repo env vars. |
| Visible-output ACs not unit-testable | AC-1 (host RGBA frame), AC-2 (test pattern), AC-3 (surface destroy/recreate) | intentionally out of JVM/instrumented-without-Surface scope; owned by device check **IT3** (CHANGESET §6). `attachSurface` deliberately not exercised here — it needs a real `Surface`+EGL. |
| Native lifecycle/threading correctness (R1) | AC-3, AC-4 | not provable by symbol-resolution test; device-verified under IT3. |
| Native/link correctness (R2) | AC-6, R2 | proved by build, not by this stage; owned by **IT1** `:videolib:assembleDebug` (both ABIs). |

No JVM (`src/test`) unit test was added: the changed surface is a JNI facade whose only pure-Kotlin logic is the `pushFrame` direct/capacity guard, which cannot be meaningfully asserted without the loaded native library (construction calls `nativeCreate`). It is covered at the instrumented level (T-LOAD) instead — consistent with the module having no JVM behavior tests for native paths.

---

Guard: CHANGESET `AUTOMATION: CONTINUE`; T-LOAD (the only owned Test-ID) is fully authored; the sole
execution blocker is device availability, reported honestly. Hand off to integration-testing (IT1–IT3).
