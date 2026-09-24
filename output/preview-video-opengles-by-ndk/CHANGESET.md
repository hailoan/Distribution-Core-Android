AUTOMATION: CONTINUE

# CHANGESET — preview-video-opengles-by-ndk

## 1. Implementation outcome

- **Status:** completed — all android-dev tasks (T1–T5) implemented. T6 is testing-owned (handoff §5); IT1–IT3 integration-owned (handoff §6).
- **Task range:** T1, T2, T3, T4, T5 (Waves 1–4 of IMPLEMENT-PLAN).
- **Module:** `:videolib` only (FR-2 preserved). No `:app` edit (design D-3: host owns the `Surface`; no forced consumer change).
- **Deviations:** one design-intentional impl-local choice (U-1) resolved — the Kotlin facade is a **new `VideoPreview` class**, not an extension of `NativeLib`. Reason: the preview owns a native handle (lifecycle state) whereas `NativeLib` is stateless; a separate class keeps `NativeLib.kt` and its two existing JNI exports byte-for-byte untouched (tightest R3 guarantee). Still additive and within `com.cii.videolib`.

## 2. Actual change manifest

| FR | SC | AC | Work | Task | Change-ID | module | affected consumers | Design-Ref | planned action | actual path | actual symbol | diff status | purpose | Test/Check | verification |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| FR-1 | SC-1 | AC-4 | W1 | T1 | C6 | videolib | none | none | create | `videolib/src/main/cpp/render_thread_executor.h` | `RenderThreadExecutor` (`runAsync`,`runSync`) | added | single render-thread serialization; drain+join on destroy | IT1 | source-inspection |
| FR-1,FR-3 | SC-1,SC-2 | AC-1,AC-2,AC-5,AC-6 | W1 | T2 | C5 | videolib | none | none | create | `videolib/src/main/cpp/gl_program.{h,cpp}` | `GlProgram` (`init`,`drawFrame`,`drawTestPattern`,`release`) | added | GLES3 passthrough RGBA program + test pattern; no YUV/FFmpeg/OES | IT1, IT3 | source-inspection |
| FR-1 | SC-1 | AC-3,AC-4,AC-5 | W1 | T3 | C4 | videolib | none | none | create | `videolib/src/main/cpp/preview_renderer.{h,cpp}` | `PreviewRenderer` + `State` enum | added | EGL lifecycle/state model; ordered idempotent teardown; single `ANativeWindow` release | IT1, IT3 | source-inspection |
| FR-1,FR-2 | SC-1 | AC-1,AC-2,AC-4 | W1 | T4 | C1 | videolib | app, external (JNI) | none | modify | `videolib/src/main/cpp/videolib.cpp` | `Java_com_cii_videolib_VideoPreview_*` (6 exports) | added (existing 2 exports unchanged) | JNI entry points → renderer; direct-buffer zero-copy | T-LOAD, IT1 | source-inspection |
| FR-1,FR-2 | SC-1 | AC-1,AC-2 | W1 | T4 | C3 | videolib | app, external (public API) | none | create | `videolib/src/main/java/com/cii/videolib/VideoPreview.kt` | `VideoPreview` (`attachSurface`,`pushFrame`,`requestPattern`,`detachSurface`,`release`) | added | public Kotlin preview facade; `loadLibrary("videolib")` | T-LOAD, IT2 | source-inspection |
| FR-2 | SC-1 | AC-6 | W1 | T5 | C2 | videolib | app (build closure) | none | modify | `videolib/src/main/cpp/CMakeLists.txt` | `add_library` sources; `target_link_libraries` +`EGL`+`GLESv3` | modified | build/link wiring; FFmpeg group/`-Bsymbolic`/16 KB/ABI preserved | IT1, IT2 | source-inspection |

All planned Change-IDs (C1–C6) completed. C7 (test source) is owned by the testing stage → §5. Every changed production path maps to an approved Change-ID.

## 3. Task completion

| Task | preconditions | invariants checked | done condition | result | evidence |
|---|---|---|---|---|---|
| T1 | — | drain+join on destroy; runs on one render thread; no new global locks | executor header self-consistent | done | `render_thread_executor.h`: dtor sets `stop_`, notifies, `join()`s; `runSync` waits on `std::future` |
| T2 | — | GLES3 `#version 300 es`; no OES/YUV/`AVFrame`; **no FFmpeg include**; compile/link failure returns safe | draw path present by inspection | done | `gl_program.cpp` includes only `<android/log.h>`,`<cmath>`,`<cstdlib>`,`<vector>` + `gl_program.h`; `linkProgram` returns 0 on failure; `drawFrame` no-ops when `program_==0` (AC-5) |
| T3 | T1,T2 | all GL/EGL via executor (AC-4); ordered teardown; one `ANativeWindow` release; init fail→`Failed` (AC-3,AC-5,R1) | transitions + idempotent teardown present | done | `preview_renderer.cpp`: every GL op inside `executor_.runSync`; `teardownEglLocked` releases GL(current)→unmake→destroy ctx/surface→terminate; `releaseSurface` releases window once + early-returns when already Released; failure path sets `Failed` |
| T4 | T3 | export name `Java_com_cii_videolib_VideoPreview_<m>` matches Kotlin exactly; existing 2 exports + `loadLibrary` preserved (R3,D-5); direct `ByteBuffer` zero-copy | both sides declared, names matched | done | 6 exports ↔ 6 `external fun`s verified (jlong/jobject/jint/jboolean/void); `stringFromJNI`+`nativeFFmpegVersion` unchanged; `NativeLib.kt` untouched; `GetDirectBufferAddress` guarded, Kotlin pre-validates `isDirect`+capacity |
| T5 | T1–T4 files exist | FFmpeg `--start-group…--end-group` order, arm64 `-Wl,-Bsymbolic`, `CMAKE_ANDROID_PAGE_SIZE 16384`/`max-page-size=16384`, ABI filter preserved; no new target (R2,D-6) | sources registered; `EGL`/`GLESv3` linked | done | `CMakeLists.txt`: two new sources added to the single `videolib` target; `EGL GLESv3` appended after `android log`; link group + page-size + `-Bsymbolic` blocks unchanged |

## 4. Authorized command results

- `:videolib:assembleDebug` (both ABIs) — **not run — authorization required.** Owned by integration-testing (Check IT1); it is the first command that proves the EGL/GLESv3 link and JNI symbol resolution. Suggested: `./gradlew :videolib:assembleDebug`.
- `:app:assembleDebug` — **not run — authorization required.** Owned by IT2 (consumer build closure).
- No compile/test/static command was executed in this stage. All §2/§3 verification is source inspection only.

## 5. Testing Handoff

| testing Task-ID | Work-ID | AC/risk | Test-ID | level | target component/contract | behavior/error scope | fake/fixture boundary | relevant paths/symbols | depends on | execution expectation |
|---|---|---|---|---|---|---|---|---|---|---|
| T6 | W1 | AC-4, AC-5(load), AC-6, R3 | T-LOAD | Android instrumented (`androidTest`) | `VideoPreview` ↔ JNI binding | native methods resolve (no `UnsatisfiedLinkError`); `loadLibrary("videolib")` succeeds; construct+`release()` with no FFmpeg/video/camera call; **regression:** existing `NativeLib.stringFromJNI`/`nativeFFmpegVersion` still resolve | none (real `.so`, no `Surface`, no pixel assert) | `videolib/src/main/java/com/cii/videolib/VideoPreview.kt`; `Java_com_cii_videolib_VideoPreview_*`; extends `FfmpegLinkageTest.kt` convention → new `videolib/src/androidTest/java/com/cii/videolib/PreviewBindingTest.kt` | T4 (author), T5 (execute) | device/emulator-dependent — authored-only until testing stage runs it |

Visual ACs (AC-1 host frame, AC-2 pattern, AC-3 surface destroy/recreate) are **not** JUnit-assertable without a real `Surface`+pixel readback → covered by IT3 device check, not T-LOAD.

## 6. Integration Handoff

| integration Task-ID | Check-ID | changed module | affected consumer/external contract | boundary | exact command or device/manual check | required environment | blocking policy |
|---|---|---|---|---|---|---|---|
| IT-task-1 | IT1 | videolib | self (native/ABI) | CMake link + ABI packaging | `./gradlew :videolib:assembleDebug` (arm64-v8a + armeabi-v7a) | NDK 29.0.14206865, CMake 3.22.1 | blocking — proves EGL/GLESv3 added without breaking FFmpeg static group / `-Bsymbolic` / 16 KB (R2) |
| IT-task-2 | IT2 | videolib | `app` (project dependency) | additive public Kotlin/JNI API | `./gradlew :app:assembleDebug` | Android SDK | blocking — consumer builds; existing `NativeLib` names intact (R3) |
| IT-task-3 | IT3 | videolib | end user (SC-1) | Surface rendering + native lifecycle | supported-device run: RGBA frame visible (AC-1), test pattern visible (AC-2), surface destroy→recreate with no crash/leak (AC-3), thread affinity holds (AC-4) | physical/emulator device, GLES 3.0 | blocking for SC-1 — GL runtime + visible output unprovable by compile/JUnit (R1,R5) |

---

Guard: predecessors `AUTOMATION: CONTINUE`; implementation complete, no STOP condition. Hand off to testing (T6/T-LOAD) and integration-testing (IT1–IT3).
