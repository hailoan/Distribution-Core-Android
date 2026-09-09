AUTOMATION: CONTINUE

# CHANGESET — enhance-preview-video-async

## 1. Implementation outcome

- **Outcome:** COMPLETED (verification-only). android-dev owns Wave 1 tasks **T-01** and **T-02**;
  both are source-conformance checks (`make no edit unless the approved contract is contradicted`).
- **Approved task range executed:** T-01, T-02. Producing Checks **S-01** and **S-02**.
- **Production delta:** none. Source inspection confirms the current `videolib` production path already
  satisfies D-2 through D-9; no contract was contradicted, so no `videolib` production, native, Gradle,
  CMake, or vendored FFmpeg file was edited (plan §1: "No production coding is planned"; D-9).
- **Deviations:** none.
- **Guard:** Guarded `impl-flow`. Both predecessor artifacts begin `AUTOMATION: CONTINUE`
  (SOLUTION-DESIGN.md, IMPLEMENT-PLAN.md). No missing material decision, contradiction, or
  unauthorized high-risk crossing was found; this CHANGESET therefore continues.
- **Downstream ownership (not executed here):** T-03/T-04/T-05 are testing-owned; T-06/T-07/T-08/T-09
  are integration-testing-owned. They are carried in the handoffs below, not implemented.

## 2. Actual change manifest

Design-Ref is `none — requirement/code-driven` for every entry (plan §2). `diff status` legend:
`verified-unchanged` = inspected, conforms, deliberately not edited; `handoff` = owned by a later
stage, no android-dev action.

| FR-ID | SC-ID | AC-ID | Work/Story | Task-ID | Change-ID | Owning module | Affected consumers/contracts | Planned action | Actual path | Actual symbol/key | Diff status | Purpose | Test/Check-ID | Verification |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| FR-1 | SC-1 | AC-1,4,5 | Story-1/W-1 | T-01 | C-01 | `videolib` | `app` (source/runtime); unknown external AAR | verify | `videolib/src/main/java/com/cii/videolib/VideoPreview.kt` | `play`,`stop`,`detachSurface`,`release`,`receiveNativeEvent`,`enqueueNativeEvent`,`invalidatePlaybackListener`, native decls, error constants | verified-unchanged | Preserve immediate acceptance, one-active-attempt, main-thread exactly-once terminal, stale-event suppression | S-01; TEST-01,04,05,06 | source inspection — conforms (VideoPreview.kt:60-211) |
| FR-1 | SC-1 | AC-1,5 | Story-1/W-1 | T-01 | C-02 | `videolib` | `app`; unknown external AAR; JNI ABI | verify | `videolib/src/main/cpp/videolib.cpp` | `PlaybackJniBridge`; `Java_com_cii_videolib_VideoPreview_nativeCreate/nativePlay/nativeStop/nativeReleaseSurface/nativeDestroy` (+`nativeSurfaceAvailable/nativePushFrame/nativeRequestPattern`) | verified-unchanged | Preserve JNI names/descriptors, copied path ownership, JavaVM attach/detach, global-ref lifetime, error mapping | S-01; TEST-06,07 | source inspection — conforms (videolib.cpp:20-256) |
| FR-1 | SC-1 | AC-1–5 | Story-1/W-1 | T-02 | C-03 | `videolib` | native state contract | verify | `videolib/src/main/cpp/video_playback.h` | `PlaybackState`,`PlaybackErrorCode`,`VideoPlayback` fields/decls | verified-unchanged | Preserve state, cancellation, worker, callback, surface, renderer ownership | S-02; TEST-01–05 | source inspection — conforms (video_playback.h:18-90) |
| FR-1 | SC-1 | AC-1–5 | Story-1/W-1 | T-02 | C-04 | `videolib` | FFmpeg decode/frame ownership | verify | `videolib/src/main/cpp/video_playback.cpp` | `AttemptResources`,`interruptInput`,`play`,`stop`,`releaseSurface`,`release`,`markPlaying`,`isCancelled`,`waitUntil`,`decodeAttempt`,`runAttempt`,`finishAttempt` | verified-unchanged | Preserve incremental decode, monotonic/fallback pacing, per-frame present, one terminal claim, cancellation, stop-before-release | S-02; TEST-01–05 | source inspection — conforms (video_playback.cpp:28-536) |
| FR-1 | SC-1 | AC-2,3,4 | Story-1/W-1 | T-02 | C-05 | `videolib` | EGL renderer lifecycle | verify | `videolib/src/main/cpp/preview_renderer.h` | `PreviewRenderer::pushFrame`,`releaseSurface`; EGL/window state | verified-unchanged | Preserve synchronous producer-buffer lifetime, render-thread EGL/window ownership | S-02; TEST-01,04 | source inspection — conforms (preview_renderer.h:25-72) |
| FR-1 | SC-1 | AC-2,3,4 | Story-1/W-1 | T-02 | C-06 | `videolib` | EGL renderer lifecycle | verify | `videolib/src/main/cpp/preview_renderer.cpp` | `pushFrame`,`teardownEglLocked`,`releaseSurface` | verified-unchanged | Keep GL upload/draw/swap on executor; EGL destruction ordered with context current | S-02; TEST-01,04 | source inspection — conforms (preview_renderer.cpp:102-192) |
| FR-1 | SC-1 | AC-2,3 | Story-1/W-1 | T-02 | C-07 | `videolib` | render-thread executor | verify | `videolib/src/main/cpp/render_thread_executor.h` | `runSync`, destructor, task queue | verified-unchanged | Confirm presentation completion bounds borrowed RGBA; destructor drains work | S-02; TEST-01 | source inspection — conforms (render_thread_executor.h:27-89) |
| FR-1 | SC-1 | AC-1–5 | Story-1/W-1 | T-03 | C-08 | `videolib` | test fixture | extend (new) | `videolib/src/androidTest/java/com/cii/videolib/PlaybackSurfaceProbe.kt` | `PlaybackSurfaceProbe` | handoff | Consumable Surface + ordered frame/terminal observation | — | testing-owned (T-03) |
| FR-1 | SC-1 | AC-1–5 | Story-1/W-1 | T-03 | C-09 | `videolib` | test fixture | extend (new) | `videolib/src/androidTest/assets/progressive_preview_long.mp4` | deterministic multi-frame video | handoff | Local readable multi-frame fixture | — | testing-owned (T-03) |
| FR-1 | SC-1 | AC-1,2,3 | Story-1/W-1 | T-04 | C-10 | `videolib` | progressive tests | extend (new) | `videolib/src/androidTest/java/com/cii/videolib/ProgressivePlaybackInstrumentedTest.kt` | `ProgressivePlaybackInstrumentedTest` | handoff | Async acceptance, first-frame-before-EOF, ordered progression, single-attempt reject | TEST-01,02 | testing-owned (T-04) |
| FR-1 | SC-1 | AC-4,5 | Story-1/W-2 | T-05 | C-11 | `videolib` | lifecycle tests | extend (new) | `videolib/src/androidTest/java/com/cii/videolib/PlaybackLifecycleInstrumentedTest.kt` | `PlaybackLifecycleInstrumentedTest` | handoff | Stop/release/detach invalidation, terminal arbitration, error categories, retry | TEST-03,04,05,06 | testing-owned (T-05) |
| FR-1 | SC-1 | AC-6 | W-3 | T-01/T-06 | C-12 | `videolib` | JNI regression | verify | `videolib/src/androidTest/java/com/cii/videolib/PreviewBindingTest.kt` | `PreviewBindingTest` | verified-unchanged | Retain export resolution + no-surface lifecycle regression | TEST-07 | source inspection — present, retained |
| FR-2 | SC-2 | AC-6 | W-3 | T-07 | C-13 | `videolib` | FFmpeg/native linkage | verify | `videolib/src/androidTest/java/com/cii/videolib/FfmpegLinkageTest.kt` | `FfmpegLinkageTest` | verified-unchanged | Retain FFmpeg symbol + library-load proof | TEST-07 | source inspection — present, retained |
| FR-2 | SC-2 | AC-6 | W-3 | T-07 | C-14 | `videolib` | native package contract | verify | `videolib/build.gradle.kts` | `minSdk`,`ndkVersion`,`abiFilters`,CMake args | verified-unchanged (read) | Confirm min SDK 21, NDK 29, both ARM ABIs, 16 KB flags | I-02; TEST-07 | build-file read pending in T-07 |
| FR-2 | SC-2 | AC-6 | W-3 | T-07 | C-15 | `videolib` | native package contract | verify | `videolib/src/main/cpp/CMakeLists.txt` | `videolib` target, FFmpeg imports/link, EGL/GLES, 16 KB link opts | verified-unchanged (read) | Confirm one aligned loadable `libvideolib.so` | I-02,I-03; TEST-07 | build-file read pending in T-07 |
| FR-2 | SC-2 | AC-6 | W-3 | T-07 | C-16 | `videolib` (generated) | external AAR / ABI | verify (no edit) | `videolib/build/outputs/aar/videolib-debug.aar` | `jni/arm64-v8a/libvideolib.so`, `jni/armeabi-v7a/libvideolib.so` | handoff | ABI payloads, JNI exports, FFmpeg resolution, LOAD alignment | I-03; TEST-07 | integration-testing-owned (T-07) |
| FR-2 | SC-2 | AC-6 | W-3 | T-08 | C-17 | `app` | direct consumer edge | verify (no edit) | `app/build.gradle.kts` | `implementation(project(":videolib"))` | verified-unchanged | Validate producer/consumer Gradle edge | I-04 | integration-testing-owned (T-08) |
| FR-2 | SC-2 | AC-6 | W-3 | T-08 | C-18 | `app` | direct consumer source | verify (no edit) | `app/src/main/java/com/chiistudio/library/MainActivity2.kt` | `tryStartPlayback` | verified-unchanged | Validate host compiles against retained contract | I-04 | integration-testing-owned (T-08) |

Every planned Change-ID (C-01 … C-18) is accounted for: C-01–C-07, C-12, C-13 as android-dev
`verified-unchanged`; C-08–C-11, C-16–C-18 as `handoff` to their owning stage; C-14/C-15 read-only in
T-07. No changed production path exists that does not map back to an approved verify Change-ID.

## 3. Task completion

| Task-ID | Preconditions | Invariants checked | Done condition | Result | Evidence |
| --- | --- | --- | --- | --- | --- |
| T-01 | D-3/D-6/D-8 approved; current Kotlin/JNI source | Public signatures, 8 JNI names + `(J)V`/`(JI)V` descriptors, Boolean/attempt-id acceptance, main-thread exactly-once delivery, error values 1/2/(3)/4, stale-attempt filter | Inspection confirms `nativePlay` copies path, starts worker, returns attempt ID; callbacks cross a thread-valid `JNIEnv` into attempt-filtered main-handler delivery; else planning blocker | DONE — conforms, no contradiction, no edit | See S-01 below |
| T-02 | D-2–D-9 approved; current C++ source | No full-file cache; ≤1 RGBA frame crosses synchronously; `AVPacket`/`AVFrame`/`SwsContext` attempt-owned; EGL on one render thread; cancellation invalidates + joins before renderer/window destruction | Inspection traces first + subsequent frames through pacing, swscale, synchronous swap, unref, EOF/terminal arbitration, stop/detach/release order with no missing ownership edge; else planning blocker | DONE — conforms, no contradiction, no edit | See S-02 below |

### Check S-01 — C-01/C-02 public/JNI path (owner T-01)

Exact trace (all source inspection, no execution):

1. **Copied JNI path ownership** — `nativePlay` reads via `GetStringUTFChars`, copies into `pathCopy`
   inside try/catch, and `ReleaseStringUTFChars` on every return branch before calling
   `playback->play(pathCopy)` (videolib.cpp:185-205). `play` launches the worker with `path` passed
   **by value** (video_playback.cpp:165), so no `jstring`/`JNIEnv` memory escapes to the worker thread.
2. **Worker acceptance token** — `play` claims one attempt under `stateMutex_`, sets `Starting`,
   spawns `runAttempt`, and returns `attemptId` (nonzero) without awaiting media work
   (video_playback.cpp:146-172). Kotlin `play` maps nonzero→`true`, `NO_ATTEMPT(0)`→`false`
   (VideoPreview.kt:71-85). Async acceptance (D-3/AC-1) holds.
3. **Stable descriptors/error values** — 8 Kotlin `external fun` (VideoPreview.kt:204-211) map 1:1 to
   the 8 `Java_com_cii_videolib_VideoPreview_*` exports (videolib.cpp:152-256). Callback method IDs
   `onNativePlaybackCompleted "(J)V"` and `onNativePlaybackError "(JI)V"` (videolib.cpp:31-34) match the
   Kotlin `@Keep` signatures (VideoPreview.kt:146-152). Error ints exact: native
   `InputOpen=1,UnsupportedVideo=2,Decode=3,Render=4` (video_playback.h:29-34) ↔ Kotlin
   `INPUT_OPEN=1,UNSUPPORTED_VIDEO=2,RENDER=4, else→DECODE` (VideoPreview.kt:153-166,215-217). `Decode=3`
   correctly lands in Kotlin's `else` branch.
4. **Thread-valid callback bridge** — `PlaybackJniBridge::environment` uses `GetEnv`, and on
   `JNI_EDETACHED` calls `AttachCurrentThread`, detaching only if it attached (videolib.cpp:90-103,
   64-87). The global ref is created in the ctor and deleted in the dtor; its lifetime is bound to the
   `VideoPlayback` via the `shared_ptr<PlaybackJniBridge>` captured in the terminal callback
   (videolib.cpp:28,46-58,155-162), released at `nativeDestroy`→`delete playback` (videolib.cpp:252-256).
5. **Attempt filtering + main delivery** — native emits exactly one terminal via `terminalClaimed_`
   (video_playback.cpp:509-535). Kotlin `enqueueNativeEvent` posts to the main `Handler`, and inside the
   post filters `activeAttemptId != event.attemptId` → drop, else nulls attempt+listener so delivery is
   once-only (VideoPreview.kt:179-193). The `startPending`/`pendingNativeEvent` guard
   (VideoPreview.kt:64-84,169-177) closes the race where native completes before `nativePlay` returns.
   `stop`/`release` call `invalidatePlaybackListener` (clears attempt+listener) **before** native
   `nativeStop`/`nativeDestroy` (VideoPreview.kt:92-98,135-142,195-202), so a cancelled attempt cannot
   surface a later frame/event. AC-4/AC-5 hold. **No contradiction.**

### Check S-02 — C-03–C-07 native decode/render path (owner T-02)

Exact trace (all source inspection, no execution):

1. **Per-attempt FFmpeg cleanup** — `AttemptResources` RAII frees `sws`/`frame`/`packet`/`codec` and
   splits `avformat_close_input` (opened) vs `avformat_free_context` (allocated-not-opened) via
   `inputOpened` (video_playback.cpp:28-50). `av_packet_unref` runs on every packet branch and after the
   read loop (video_playback.cpp:465,473,476). No resource is shared across attempts.
2. **Frame emission before EOF** — the read loop `av_read_frame`→`avcodec_send_packet`→`receiveFrames`
   presents each decoded frame inside the ongoing loop; drain (`send_packet(nullptr)`) only runs after
   `AVERROR_EOF` (video_playback.cpp:460-490). Progressive presentation (AC-2) — no full-file
   accumulation; `rgba` is a single reused scratch vector (video_playback.cpp:364,426).
3. **Monotonic/fallback pacing** — presentation time is `sourceUs - firstSourceUs`, clamped
   non-decreasing via `lastPresentationUs`; missing `best_effort_timestamp` uses
   `lastPresentationUs + fallbackFrameDurationUs` from `av_guess_frame_rate` (default 33333 µs)
   (video_playback.cpp:351-396). Clock origin is set at the first frame, not at EOF (D-4).
4. **Synchronous one-frame handoff** — present happens under `rendererMutex_` via
   `renderer_.pushFrame`, which `runSync`s GL upload/draw/swap to completion **before**
   `av_frame_unref` and before the next decode iteration reuses `rgba`
   (video_playback.cpp:441-455; preview_renderer.cpp:102-135; render_thread_executor.h:52-64).
   At most one decode-produced frame is in flight; no queue/cache exists (D-5). `runSync` waits on a
   `std::future`, so the borrowed `pixels` outlive the GL copy.
5. **Cancellation** — `interruptInput` aborts blocking FFmpeg I/O via
   `format->interrupt_callback` bound to `cancelRequested_` (video_playback.cpp:60-63,312-313);
   `waitUntil` wakes on `waitCv_` and re-checks `isCancelled` (video_playback.cpp:290-298); present is
   gated by `isCancelled` under the render lock (video_playback.cpp:444). `isCancelled` covers
   cancel flag, terminal-claimed, attempt mismatch, Released, and surface-not-ready
   (video_playback.cpp:281-288).
6. **Terminal claim + ordering** — `finishAttempt` claims once under `terminalClaimed_`, sets
   Completed/Failed, and on Render failure releases the surface before the callback
   (video_playback.cpp:509-535). `runAttempt` suppresses `finishAttempt` when cancelled
   (video_playback.cpp:504-506) so cancellation wins the race (AC-5). EOF with no presented frame →
   `Decode` (video_playback.cpp:491-493).
7. **Stop-before-release order** — `stop`/`releaseSurface`/`release` claim under `stateMutex_`, set
   `cancelRequested_`, `notify_all` the wait CV, then **move + join** the worker, and only then call
   `renderer_.releaseSurface()` (video_playback.cpp:175-268). `releaseSurface` synchronously
   `teardownEglLocked` on the executor with the context current, then releases the one
   `ANativeWindow` ref exactly once (preview_renderer.cpp:155-192). The `RenderThreadExecutor`
   destructor drains and joins its worker (render_thread_executor.h:27-36), and `~PreviewRenderer`
   releases the surface before the executor member is destroyed (preview_renderer.cpp:13-17). D-7 holds.
   **No contradiction.**

## 4. Authorized command results

No command was executed in this stage. android-dev's owned tasks (T-01, T-02) are source-only Checks
S-01/S-02 (plan §7); no compile/test/static-check is assigned to them, and no separate build
authorization was requested or granted. All evidence above is source inspection only.

| Boundary | Smallest relevant command (for the next stage/user to authorize) | Status |
| --- | --- | --- |
| `videolib` Kotlin/native compile+link | `./gradlew :videolib:assembleDebug` | not run — authorization required (owner T-07 / Check I-02) |
| `videolib` device instrumentation | `./gradlew :videolib:connectedDebugAndroidTest` (attached supported ARM device) | not run — authorization required (owner T-06 / Check I-01) |
| Packaged AAR/ELF inspection | extract `videolib-debug.aar`; `llvm-readelf`/`llvm-nm` on each `libvideolib.so` for JNI exports, unresolved FFmpeg refs, LOAD alignment `>= 0x4000` | not run — authorization required (owner T-07 / Check I-03) |
| Direct consumer compile+package | `./gradlew :app:assembleDebug` | not run — authorization required (owner T-08 / Check I-04) |

No source-inspection result above is presented as a compile, link, load, or runtime result. Per the
native-boundary guideline, visible-frame, EGL, FFmpeg-load, and per-ABI runtime behavior cannot be
claimed from source inspection and remain owned by the device/package checks below.

## 5. Testing Handoff

Preserves the approved test contract (plan §7 / §4). Fixture (T-03) is a prerequisite for T-04/T-05;
T-04 and T-05 write disjoint files and only read the shared fixture (serialize fixture changes through
T-03).

| Testing Task-ID | Work/Story | AC-ID/risk | Test-ID | Level | Target component/contract | Behavior/error scope | Fake/fixture boundary | Relevant changed paths/symbols | Depends on | Execution expectation |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| T-03 | Story-1/W-1 | fixture for AC-1–5 | (fixture self-check) | Android instrumentation setup | `PlaybackSurfaceProbe` + local video asset | Consumable Surface, ordered frame/terminal observation, deterministic cleanup; multi-frame deterministic video | C-08 probe, C-09 asset; no production hook | `VideoPreview` public API (T-01), native flow (T-02) | T-01, T-02 | Supported EGL/GLES 3.0 ARM device/emulator |
| T-04 | Story-1/W-1 | AC-1,2,3; poor-startup, unbounded-queue | TEST-01 | Android instrumentation + real EGL surface | `play`→JNI→`decodeAttempt`→`pushFrame` | Acceptance before terminal; first visible frame before EOF; ordered subsequent frames; second `play` rejected | C-08 probe, C-09 video; no mock decoder/renderer | C-01,C-02,C-04,C-06 | T-03 | Supported device; no numeric latency claim |
| T-04 | Story-1/W-1 | AC-5 | TEST-02 | Android instrumentation | Natural EOF + Kotlin listener dispatch | Exactly one completion after ≥1 presented frame; callback on main; later attempt accepted | C-08/C-09 | C-01 `enqueueNativeEvent`, C-04 `finishAttempt` | T-03 | Supported device |
| T-05 | Story-1/W-2 | AC-4; stop/release race | TEST-03 | Android instrumentation | `stop` + native `VideoPlayback::stop` | Stop during pace/present returns after quiescence; no later prior-attempt frame/callback; repeated stop safe; retry succeeds | C-08/C-09 ordered recorder | C-01 `stop`/`invalidatePlaybackListener`, C-04 `stop` | T-03 | Event/latch synchronization, supported device |
| T-05 | Story-1/W-2 | AC-4,5; EGL/window lifetime | TEST-04 | Android instrumentation | `detachSurface`/`releaseSurface`, render-failure delivery, reattach | Surface loss invalidates, quiesces decode before EGL/window teardown, ≤1 render failure when not cancelled, rejects play until reattach | C-08 surface control, C-09 | C-01 `detachSurface`, C-04 `releaseSurface`, C-06 teardown | T-03 | Real EGL/window, supported device |
| T-05 | Story-1/W-2 | AC-4; stale JNI/global-ref | TEST-05 | Android instrumentation | `release`, native destruction, callback bridge | Release during active work suppresses prior-attempt visibility; repeated release/stop/detach safe; released instance rejects play | C-08/C-09; strong refs for test lifetime only | C-01 `release`, C-02 dtor/global-ref, C-04 `release` | T-03 | Supported device |
| T-05 | Story-1/W-2 | AC-5 | TEST-06 | Android instrumentation | `PlaybackError` mapping + terminal arbitration | Missing/unreadable→`INPUT_OPEN`; readable non-video→`UNSUPPORTED_VIDEO`; one error on main per non-cancelled attempt; cancellation wins over racing error | temp app-cache files + C-08; no external storage/network | C-01 error mapping, C-04 `decodeAttempt` categories | T-03 | Supported device |

## 6. Integration Handoff

Preserves every planned Check-ID (plan §7 module integration matrix). T-06 and T-07 may run
concurrently after their predecessors; T-08 after T-07; T-09 after T-06+T-07+T-08.

| Integration Task-ID | Check-ID | Changed module | Affected consumer/external contract | Boundary | Exact command / device check | Required environment | Blocking policy |
| --- | --- | --- | --- | --- | --- | --- | --- |
| T-06 | I-01 | `videolib` tests | Public/JNI/native runtime | Kotlin→JNI→FFmpeg worker→EGL surface→Kotlin callback | `./gradlew :videolib:connectedDebugAndroidTest` (TEST-01–07) | Attached supported ARM device, EGL/GLES 3.0 | Blocking: compile cannot prove visible/threaded/lifecycle behavior; record ABI/API/device |
| T-07 | I-02 | `videolib` (production unchanged) | Native build/link contract | CMake, FFmpeg static archives, EGL/GLES, `libvideolib.so` | `./gradlew :videolib:assembleDebug` | NDK 29 toolchain | Blocking for T-08 |
| T-07 | I-03 | `videolib` (production unchanged) | Unknown external AAR consumers; supported ABIs | Generated AAR/JNI/ELF | Extract `videolib/build/outputs/aar/videolib-debug.aar`; verify both `jni/arm64-v8a` + `jni/armeabi-v7a` `libvideolib.so`; `llvm-readelf`/`llvm-nm` for `Java_com_cii_videolib_VideoPreview_*` exports, no unresolved FFmpeg refs, every LOAD alignment `>= 0x4000` | NDK 29 `llvm-readelf`/`llvm-nm` | Blocking: build success alone does not prove per-ABI payload/alignment |
| T-08 | I-04 | `videolib` tests only; `app` unchanged | Direct in-repo `app` consumer | Gradle project dependency, Kotlin source compat, APK native packaging | `./gradlew :app:assembleDebug` (retains `tryStartPlayback`) | Standard Android build | Blocking after T-07 |
| T-09 | I-05 | `videolib` (production unchanged) | Runtime on both ARM families | Local playback, FFmpeg decode, EGL present, lifecycle/callback | Run TEST-01–07 on `arm64-v8a` and `armeabi-v7a`; record device/API/ABI; plus unchanged `app` visible-playback observation | One device per ABI family (in aggregate) | Blocking final closure; unavailable hardware = pending environment-dependent evidence, not a pass |

Guarded flow: this CHANGESET begins `AUTOMATION: CONTINUE`, so the handoff to testing (T-03→T-05)
and integration-testing (T-06→T-09) proceeds. No production, native, Gradle, CMake, vendored FFmpeg,
generated-output, or `app` file was edited under this stage.
