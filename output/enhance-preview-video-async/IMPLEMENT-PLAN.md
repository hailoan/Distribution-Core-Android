AUTOMATION: CONTINUE

# IMPLEMENT-PLAN — enhance-preview-video-async

## 1. Planning control

### Source, outcome, and guard

| Item | Value |
| --- | --- |
| Source design revision | `SOLUTION-DESIGN.md`, SHA-256 `25e87503a9af70d77637caf7ec10a38c7f2b921e3465238ac983cc06172b5203` |
| Automation guard | Guarded `impl-flow`; source design begins with `AUTOMATION: CONTINUE`; no material decision or authority blocker was found. |
| Primary owner | `videolib`, evidenced by `VideoPreview` owning the public lifecycle, `VideoPlayback` owning decode/attempt state, `PreviewRenderer` owning EGL/GLES presentation, and `videolib.cpp` owning JNI callbacks. |
| Planned changed modules | `videolib` test source and test assets only. No production delta is justified because the current source already implements D-2 through D-9. |
| Unchanged consumers | `app` directly consumes `implementation(project(":videolib"))` and calls `VideoPreview.play`; unknown external AAR consumers remain compatibility-relevant. `benchmark` is an indirect app target but is outside the exercised playback path and receives no source/package contract change. |
| Plan outcome | Verify the existing production path as the approved implementation, add repeatable supported-device conformance coverage for AC-1 through AC-5, and close native/package/consumer verification for AC-6. |
| Handoff | No production coding is planned. Android-dev performs bounded source-conformance checks; testing adds the device test fixture and coverage; integration-testing runs compile, package, JNI, supported-device, and direct-consumer gates. |

### Ownership, dependency, contract, and verification closure

| Area | Closure |
| --- | --- |
| Owning module | `videolib` owns every implementation and test touchpoint. |
| Changed-module closure | `videolib` test-only changes do not alter its AAR contract. Native and public behavior is nevertheless verified because the acceptance criteria cross those existing boundaries. |
| Producer → consumer | `videolib` → `app` through `app/build.gradle.kts` project dependency and `MainActivity2.tryStartPlayback`; source/package compatibility is checked without changing `app`. |
| External consumers | Unknown; preserve Kotlin signatures, callback behavior, JNI exports, error values, min SDK 21, ARM ABI set, and native artifact behavior. Absence of in-repository callers is not used as compatibility evidence. |
| Public/native/build contracts | Preserve `VideoPreview.play/stop/detachSurface/release`, `PlaybackListener`, `PlaybackError`, the `Java_com_cii_videolib_VideoPreview_*` exports and `(J)V`/`(JI)V` callback descriptors, `VideoPlayback` attempt ownership, FFmpeg 7.1 static linkage, one-frame RGBA handoff, render-thread EGL affinity, `arm64-v8a`/`armeabi-v7a`, and 16 KB LOAD alignment. |
| Verification closure | `:videolib:assembleDebug`; `:videolib:connectedDebugAndroidTest` on supported ARM devices; packaged `libvideolib.so` ABI/JNI/alignment inspection; `:app:assembleDebug`; supported-device progressive playback, cancellation, surface-loss, completion/failure, and retry observation. |

### Assumptions

- The approved behavior is local, video-only preview; audio, network sources, seek, pause/resume, loop, and background playback remain out of scope.
- No numeric time-to-first-frame requirement is inferred. The observable threshold is first visible frame before terminal completion/full-file EOF.
- Device tests may use an implementation-local surface probe, but it must consume presented buffers and expose ordered frame/terminal observations without adding production hooks.
- The checked-in test video must be deterministic, contain distinguishable successive frames, be long enough for first-frame-before-completion observation, and use a codec supported by the bundled FFmpeg build. It is test input, not a production asset.
- Publishing, signing, upload, distribution, commit, and push are not part of this plan.

### Blockers and unresolved planning inputs

None. Availability of both supported ARM device families affects when the device-dependent checks can execute, not whether their scope is defined.

### Bounded investigation ledger

| ID | Evidence inspected | Planning conclusion |
| --- | --- | --- |
| INV-1 | Current code graph entry for `VideoPreview.play` plus source validation in `VideoPreview.kt`, `videolib.cpp`, and `MainActivity2.kt` | Direct flow is host → public facade → hand-mangled JNI → per-instance native owner; `app` is a compile/runtime consumer, not an implementation owner. |
| INV-2 | `VideoPlayback::play`, `decodeAttempt`, `waitUntil`, `finishAttempt`, `stop`, `releaseSurface`, and `release` | Playback is accepted by spawning a worker; decode/pacing/presentation occurs incrementally; attempt identity and cancellation arbitrate stale frames/events; teardown joins the worker before renderer release. |
| INV-3 | `PreviewRenderer::pushFrame`, `releaseSurface`, and `RenderThreadExecutor::runSync` | GL upload/draw/swap is serialized on one render thread and completes before producer RGBA storage is reused, bounding the handoff to one frame. |
| INV-4 | `videolib/build.gradle.kts`, `CMakeLists.txt`, bundled archive paths, and existing instrumented tests | Both ARM ABIs, FFmpeg static archives, JNI load coverage, min SDK 21, GLES/EGL linkage, and 16 KB link settings exist; no build-file edit is required. |
| INV-5 | Existing `videolib/src/androidTest` tree and media-file search | JNI/linkage tests exist, but no playback media fixture or progressive/lifecycle device test exists; this is the only planned file-writing gap. |

## 2. Change-surface inventory

All entries are non-UI: `Design-Ref: none — requirement/code-driven`.

| Change-ID | Existing/new | Action | Exact path | Symbol/resource/config key | Design-Ref(s) | Responsibility | Evidence/design decision | Shared/collision key |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| C-01 | existing | verify | `videolib/src/main/java/com/cii/videolib/VideoPreview.kt` | `VideoPreview.play`, `stop`, `detachSurface`, `release`, `receiveNativeEvent`, `enqueueNativeEvent`, `invalidatePlaybackListener`, native declarations/error constants | none — requirement/code-driven | Preserve immediate acceptance, one-active-attempt policy, main-thread exactly-once terminal delivery, and stale-event suppression. | AC-1, AC-4, AC-5; D-3, D-6, D-7 | `public-playback-contract` |
| C-02 | existing | verify | `videolib/src/main/cpp/videolib.cpp` | `PlaybackJniBridge`; `Java_com_cii_videolib_VideoPreview_nativeCreate/nativePlay/nativeStop/nativeReleaseSurface/nativeDestroy` | none — requirement/code-driven | Preserve JNI names/descriptors, copied path ownership, JavaVM attach/detach, global reference lifetime, and callback error mapping. | D-6, D-8; JNI boundary contract | `jni-playback-contract` |
| C-03 | existing | verify | `videolib/src/main/cpp/video_playback.h` | `PlaybackState`, `PlaybackErrorCode`, `VideoPlayback` state/worker/attempt fields and declarations | none — requirement/code-driven | Preserve the native state, cancellation, worker, callback, surface, and renderer ownership contract. | AC-1 through AC-5; D-2 through D-7 | `native-playback-state` |
| C-04 | existing | verify | `videolib/src/main/cpp/video_playback.cpp` | `AttemptResources`; `interruptInput`; `VideoPlayback::play`, `stop`, `releaseSurface`, `release`, `markPlaying`, `isCancelled`, `waitUntil`, `decodeAttempt`, `runAttempt`, `finishAttempt` | none — requirement/code-driven | Preserve incremental FFmpeg decode, monotonic/fallback pacing, per-frame conversion/presentation, one terminal claim, cancellation interruption, and stop-before-release ordering. | AC-1 through AC-5; D-3 through D-9 | `native-playback-state` |
| C-05 | existing | verify | `videolib/src/main/cpp/preview_renderer.h` | `PreviewRenderer::pushFrame`, `releaseSurface`; renderer-owned EGL/window state | none — requirement/code-driven | Preserve synchronous producer-buffer lifetime and render-thread EGL/window ownership. | AC-2 through AC-4; D-5, D-7 | `egl-renderer-lifecycle` |
| C-06 | existing | verify | `videolib/src/main/cpp/preview_renderer.cpp` | `PreviewRenderer::pushFrame`, `teardownEglLocked`, `releaseSurface` | none — requirement/code-driven | Keep GL upload/draw/swap on the renderer executor and EGL destruction ordered with a current context. | AC-2 through AC-4; renderer boundary contract | `egl-renderer-lifecycle` |
| C-07 | existing | verify | `videolib/src/main/cpp/render_thread_executor.h` | `RenderThreadExecutor::runSync`, destructor, task queue | none — requirement/code-driven | Confirm presentation completion bounds borrowed RGBA storage and executor destruction drains work. | D-2, D-5, D-7 | `egl-renderer-lifecycle` |
| C-08 | new | extend | `videolib/src/androidTest/java/com/cii/videolib/PlaybackSurfaceProbe.kt` | planned test-only `PlaybackSurfaceProbe` | none — requirement/code-driven | Provide a real consumable Android `Surface`, ordered frame observations, and deterministic cleanup for playback tests without production hooks. | Test fixture decision; EGL behavior requires a device | `playback-test-fixture` |
| C-09 | new | extend | `videolib/src/androidTest/assets/progressive_preview_long.mp4` | deterministic local multi-frame video fixture | none — requirement/code-driven | Supply a readable local video with visually distinguishable frame progression and enough presentation span to observe a frame before completion. | AC-1 through AC-5; no numeric latency target | `playback-test-fixture` |
| C-10 | new | extend | `videolib/src/androidTest/java/com/cii/videolib/ProgressivePlaybackInstrumentedTest.kt` | planned `ProgressivePlaybackInstrumentedTest` | none — requirement/code-driven | Verify asynchronous acceptance, first visible frame before completion, progressive ordered presentation, one-frame observable progress, and timestamp fallback coverage where fixture permits. | AC-1, AC-2, AC-3 | `progressive-playback-tests` |
| C-11 | new | extend | `videolib/src/androidTest/java/com/cii/videolib/PlaybackLifecycleInstrumentedTest.kt` | planned `PlaybackLifecycleInstrumentedTest` | none — requirement/code-driven | Verify stop/release/detach invalidation, terminal arbitration, error categories, main-thread delivery, idempotence, and retry. | AC-4, AC-5 | `playback-lifecycle-tests` |
| C-12 | existing | verify | `videolib/src/androidTest/java/com/cii/videolib/PreviewBindingTest.kt` | `PreviewBindingTest` JNI/lifecycle regression tests | none — requirement/code-driven | Retain existing native export resolution and no-surface lifecycle regression coverage. | JNI compatibility risk | `jni-playback-contract` |
| C-13 | existing | verify | `videolib/src/androidTest/java/com/cii/videolib/FfmpegLinkageTest.kt` | `FfmpegLinkageTest` | none — requirement/code-driven | Retain runtime proof that bundled FFmpeg symbols and the shared JNI library load. | FFmpeg/native linkage risk | `native-package-contract` |
| C-14 | existing | verify | `videolib/build.gradle.kts` | `minSdk`, `ndkVersion`, `abiFilters`, CMake arguments | none — requirement/code-driven | Confirm build configuration remains unchanged: min SDK 21, NDK 29, both ARM ABIs, and flexible/16 KB page flags. | D-8, AC-6 | `native-package-contract` |
| C-15 | existing | verify | `videolib/src/main/cpp/CMakeLists.txt` | `videolib` shared target, FFmpeg imports/link group, EGL/GLES linkage, 16 KB link options | none — requirement/code-driven | Confirm all native sources and static archives remain in one loadable, aligned `libvideolib.so`. | D-8, AC-6 | `native-package-contract` |
| C-16 | generated — do not edit | verify | `videolib/build/outputs/aar/videolib-debug.aar` | `jni/arm64-v8a/libvideolib.so`, `jni/armeabi-v7a/libvideolib.so` | none — requirement/code-driven | Inspect the generated AAR for both ABI payloads, required JNI exports, static FFmpeg resolution, and ELF LOAD alignment. | Native artifact verification obligation | `native-package-contract` |
| C-17 | existing | verify | `app/build.gradle.kts` | `implementation(project(":videolib"))` | none — requirement/code-driven | Validate the direct producer/consumer Gradle edge; do not edit it. | AC-6; SRC-3 | `app-videolib-consumer` |
| C-18 | existing | verify | `app/src/main/java/com/chiistudio/library/MainActivity2.kt` | `MainActivity2.tryStartPlayback` | none — requirement/code-driven | Validate the unchanged host compiles against and can exercise the retained public playback contract. | AC-6; direct consumer evidence | `app-videolib-consumer` |

## 3. Work-item backlog

| FR-ID | SC-ID | AC-ID | Work-ID/Story-ID | Outcome | Module/screen | Depends on |
| --- | --- | --- | --- | --- | --- | --- |
| FR-1 | SC-1 | AC-1, AC-2, AC-3 | Story-1 / W-1 | An accepted local-video request returns without waiting for media completion and produces ordered visible frames before EOF with a bounded handoff. | `videolib`; no screen change | — |
| FR-1 | SC-1 | AC-4, AC-5 | Story-1 / W-2 | Cancellation, surface loss, release, completion, failure, and retry expose no stale frame/event and produce at most one permitted terminal result. | `videolib`; no screen change | W-1 fixture contract |
| FR-2 | SC-2 | AC-6 | W-3 | The test-only change remains inside `videolib`, while the native artifact and unchanged `app` consumer retain public/JNI/ABI/build compatibility. | `videolib`, unchanged `app` consumer | W-1, W-2 |

## 4. Task backlog

| Task-ID | Work-ID/Story-ID | Owning module | Owner stage | Objective | Change-IDs / exact path-symbol scope | Design-Ref(s) | Preconditions/inputs | Invariants | Done condition | Verification/Test-ID/Check-ID | Depends on | Collision key |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| T-01 | Story-1 / W-1 | `videolib` | android-dev | Record source conformance of asynchronous acceptance and terminal delivery across the public/JNI boundary; make no edit unless the approved contract is contradicted. | C-01 `VideoPreview.play` and callback/lifecycle symbols; C-02 `PlaybackJniBridge` and playback JNI exports | none — requirement/code-driven | Approved D-3, D-6, D-8; current Kotlin/JNI source | Public signatures, JNI names/descriptors, Boolean acceptance semantics, callback main-thread delivery, error values, and stale-attempt filtering remain unchanged. | Inspection confirms `nativePlay` owns a copied path, starts through the native owner, returns an attempt ID, and callbacks cross through a thread-valid JNI environment into attempt-filtered main-thread delivery; any contradiction is a planning blocker rather than an improvised edit. | source-only Check S-01; linked TEST-01, TEST-04, TEST-05, TEST-06 | — | `public-playback-contract`, `jni-playback-contract` |
| T-02 | Story-1 / W-1 | `videolib` | android-dev | Record source conformance of incremental FFmpeg decode, pacing, bounded render handoff, and lifecycle ordering; make no edit unless the approved contract is contradicted. | C-03 `VideoPlayback` state/fields; C-04 decode/lifecycle symbols; C-05/C-06 `PreviewRenderer` presentation/teardown; C-07 `RenderThreadExecutor::runSync`/destructor | none — requirement/code-driven | Approved D-2 through D-9; current C++ source | No full-file frame cache; at most one decode-produced RGBA frame crosses synchronously; `AVPacket`/`AVFrame`/`SwsContext` stay attempt-owned; EGL work remains on one render thread; cancellation invalidates and joins playback before renderer/window destruction. | Inspection traces first and subsequent decoded frames through pacing, swscale, synchronous swap, frame unref, EOF/terminal arbitration, and stop/detach/release order with no missing ownership edge; any contradiction is a planning blocker rather than an improvised redesign. | source-only Check S-02; linked TEST-01 through TEST-05 | — | `native-playback-state`, `egl-renderer-lifecycle` |
| T-03 | Story-1 / W-1 | `videolib` | testing | Create the reusable device-test surface probe and deterministic local video fixture. | C-08 planned `PlaybackSurfaceProbe`; C-09 test asset | none — requirement/code-driven | A supported Android device/emulator with EGL/GLES 3.0; bundled decoder supports fixture codec | Test surface consumes buffers so playback cannot stall; observations are ordered/thread-safe; cleanup releases surface resources; fixture is local, video-only, multi-frame, deterministic, and not a latency benchmark. | The probe can attach to `VideoPreview`, observe/consume multiple visible frames in order, expose whether a terminal event has occurred, and clean up repeatedly; the fixture copies to a readable local file for `play`. | fixture self-check used by TEST-01 through TEST-05 | T-01, T-02 | `playback-test-fixture` |
| T-04 | Story-1 / W-1 | `videolib` | testing | Add progressive-playback instrumentation coverage without production-only observability hooks. | C-10 `ProgressivePlaybackInstrumentedTest`; read C-08/C-09 | none — requirement/code-driven | T-03 fixture; public API and listener contract from T-01; native flow from T-02 | Assert ordering, not an invented millisecond target; keep surface consumption active; always stop/release in cleanup; no sleeps used as the sole synchronization mechanism. | Tests prove acceptance returns before terminal completion, the first visible fixture frame precedes terminal completion/EOF, successive visible frames follow the fixture's presentation order, a second concurrent `play` is rejected, and natural completion is delivered once on main. | TEST-01, TEST-02 | T-03 | `progressive-playback-tests` |
| T-05 | Story-1 / W-2 | `videolib` | testing | Add lifecycle, cancellation, failure, and recovery instrumentation coverage. | C-11 `PlaybackLifecycleInstrumentedTest`; read C-08/C-09 | none — requirement/code-driven | T-03 fixture; lifecycle/state contract from T-01/T-02 | Invalidate before teardown; after each cancelling call returns, admit no prior-attempt frame or listener event; cancellation is not a terminal result; release/detach are idempotent; retry requires restored preconditions. | Tests cover stop during playback plus retry, detach/surface loss plus render failure and reattach retry, release during playback plus repeated release/stop, input-open and unsupported-video categories, exactly-one main-thread terminal result for non-cancelled attempts, and stale-event suppression across attempts. | TEST-03, TEST-04, TEST-05, TEST-06 | T-03 | `playback-lifecycle-tests` |
| T-06 | Story-1 / W-1, W-2 | `videolib` | integration-testing | Compile and execute the complete instrumented contract suite on a supported ARM device. | C-08 through C-13; no production edits | none — requirement/code-driven | T-04 and T-05 authored; attached supported-ABI device with EGL/GLES 3.0 | A compile/link result is not reported as proof of visible frames; device failures are recorded against the exact AC/risk; existing JNI/linkage regressions remain enabled. | `./gradlew :videolib:connectedDebugAndroidTest` passes and reports TEST-01 through TEST-07 on the attached device; execution record identifies ABI/API/device. | Check I-01; TEST-01 through TEST-07 | T-04, T-05 | `videolib-device-suite` |
| T-07 | W-3 | `videolib` | integration-testing | Build and inspect the native library package for unchanged JNI, linkage, ABI, and 16 KB contracts. | C-02, C-12 through C-16 | none — requirement/code-driven | T-01/T-02 source conformance complete; NDK 29 readelf tooling available | Do not edit generated AAR contents or vendored FFmpeg archives; both Kotlin/native sides are built together; both configured ARM ABIs must be present; every LOAD segment alignment is at least `0x4000`. | `:videolib:assembleDebug` passes; AAR contains both `libvideolib.so` payloads; required `VideoPreview` JNI exports exist; the shared object has no unresolved FFmpeg symbols and meets 16 KB LOAD alignment for both ABIs. | Check I-02, I-03 | T-01, T-02 | `native-package-contract` |
| T-08 | W-3 | `app` (consumer check only) | integration-testing | Verify the unchanged direct consumer still compiles and packages the retained `videolib` contract. | C-17 `app` dependency edge; C-18 `MainActivity2.tryStartPlayback`; read generated C-16 | none — requirement/code-driven | T-07 producer artifact succeeds | No `app` source or Gradle edit; no new public API, permission, resource, or migration requirement. | `./gradlew :app:assembleDebug` passes with the existing `tryStartPlayback` call and packaged native library. | Check I-04 | T-07 | `app-videolib-consumer` |
| T-09 | W-3 | `videolib` | integration-testing | Close runtime behavior on each supported ARM ABI and record the scope that automation cannot generalize across devices. | C-01 through C-18, read-only runtime exercise | none — requirement/code-driven | T-06 and T-08 pass; one `arm64-v8a` device and one `armeabi-v7a` device are available in aggregate | Use the approved local video-only scenario; do not claim a numeric startup target; verify first-visible-frame-before-completion, progressive frames, cancellation, surface loss/recovery, completion/failure, retry, and no stale callbacks. | The automated suite or equivalent supported-device observation passes on both ABI families, with device/API/ABI and each covered TEST-ID recorded; unavailable hardware is reported as pending environment-dependent evidence, not as a pass. | Check I-05; TEST-01 through TEST-07 | T-06, T-07, T-08 | `supported-abi-runtime` |

## 5. Dependency map (DAG)

### Typed edges

| From | To | Edge type | Reason |
| --- | --- | --- | --- |
| T-01 | T-03 | contract | Test fixture must exercise the confirmed public/JNI attempt and callback contract. |
| T-02 | T-03 | contract | Surface probe and fixture observations must match the confirmed native/render ownership boundary. |
| T-03 | T-04 | test | Progressive tests require the reusable surface and video fixture. |
| T-03 | T-05 | test | Lifecycle tests require the same surface and video fixture. |
| T-04 | T-06 | test | Device suite execution follows progressive test authoring. |
| T-05 | T-06 | test | Device suite execution follows lifecycle test authoring. |
| T-01 | T-07 | contract | JNI/package inspection uses the confirmed export/callback set. |
| T-02 | T-07 | contract | Native/package inspection uses the confirmed C++/FFmpeg/EGL ownership and link boundary. |
| T-07 | T-08 | wiring | The producer native artifact must build/package before the direct app consumer gate. |
| T-06 | T-09 | test | Cross-ABI runtime closure uses the completed device suite. |
| T-07 | T-09 | contract | Cross-ABI runtime closure requires validated packages for both ABIs. |
| T-08 | T-09 | wiring | Host observation follows direct-consumer compile/package compatibility. |

Ownership serialization applies inside T-01 to the coupled Kotlin/JNI callback contract and inside T-02 to the coupled decode/renderer lifetime trace. T-04 and T-05 may run concurrently because they create disjoint test files and only read the shared fixture. T-06 and T-07 may run concurrently after their respective predecessors because one executes test/runtime behavior while the other inspects generated build artifacts; they do not edit a shared source file.

Cycle check: **PASS**. A topological order is `T-01/T-02 → T-03 and T-07 → T-04/T-05 → T-06 and T-08 → T-09`; every edge points forward and no task depends on its descendant.

## 6. Execution waves

| Wave | Tasks | Concurrency/serialization | Entry condition |
| --- | --- | --- | --- |
| 1 | T-01, T-02 | Concurrent: T-01 owns Kotlin/JNI conformance; T-02 owns native decode/render conformance. Their exact source scopes do not overlap. | Approved solution design and current source available. |
| 2 | T-03, T-07 | Concurrent: T-03 writes only new androidTest fixture files; T-07 reads build/native/package surfaces and generated output. | T-01 and T-02 complete without contradiction. |
| 3 | T-04, T-05, T-08 | T-04 and T-05 are concurrent because their new test files are disjoint and the fixture is read-only. T-08 is also read/build-only and may run once T-07 succeeds. | T-03 completes for T-04/T-05; T-07 completes for T-08. |
| 4 | T-06 | Serialized after both test-authoring tasks so one device suite contains the complete coverage. | T-04 and T-05 complete. |
| 5 | T-09 | Serialized final runtime closure because it consumes device-test, native-package, and app-consumer evidence. | T-06, T-07, and T-08 complete. |

## 7. Test scope and verification matrix

### Test scope

| Test-ID | AC-ID/risk | Level | Target component/contract | Behavior/transition/error scope | Fake/fixture boundary | Production Task-ID | Depends on | Execution expectation |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| TEST-01 | AC-1, AC-2, AC-3; poor-startup and unbounded-queue risks | Android instrumentation + real EGL surface | `VideoPreview.play` → JNI → `VideoPlayback::decodeAttempt` → `PreviewRenderer::pushFrame` | Acceptance does not wait for terminal completion; first visible frame occurs before completion/EOF; distinguishable subsequent frames are observed in presentation order; second active request is rejected. | C-08 consumable surface probe and C-09 local video; no mock decoder/renderer and no production test hook | T-01, T-02 | T-03, T-04 | Runnable on a supported EGL/GLES ARM device; no numeric latency claim. |
| TEST-02 | AC-5 | Android instrumentation | Natural EOF and Kotlin listener dispatch | Exactly one completion after at least one presented frame; callback executes on main; a later valid attempt is accepted. | C-08/C-09 | T-01, T-02 | T-03, T-04 | Runnable on supported device. |
| TEST-03 | AC-4; stop/release race | Android instrumentation | `VideoPreview.stop` and native `VideoPlayback::stop` | Stop during pacing/presentation returns only after quiescence; no later prior-attempt frame/callback; repeated stop is safe; retry succeeds on ready surface. | C-08/C-09 with ordered frame/event recorder | T-01, T-02 | T-03, T-05 | Runnable on supported device; synchronization must be event/latch based. |
| TEST-04 | AC-4, AC-5; EGL/window lifetime risk | Android instrumentation | `detachSurface`/`releaseSurface`, render-failure delivery, reattach | Surface loss invalidates presentation, quiesces decode before EGL/window teardown, emits at most one render failure when not explicitly cancelled, rejects play until reattach, then permits retry. | C-08 surface lifecycle control and C-09 video | T-01, T-02 | T-03, T-05 | Runnable on supported device; requires real EGL/window behavior. |
| TEST-05 | AC-4; stale JNI/global-reference risk | Android instrumentation | `VideoPreview.release`, native destruction, callback bridge | Release during active work suppresses prior-attempt listener/frame visibility after return; repeated release/stop/detach are safe and the released instance rejects play. | C-08/C-09; strong references retained only for the test lifetime | T-01, T-02 | T-03, T-05 | Runnable on supported device. |
| TEST-06 | AC-5 | Android instrumentation | Existing public `PlaybackError` mapping and terminal arbitration | Missing/unreadable local input reports `INPUT_OPEN`; readable non-video input reports `UNSUPPORTED_VIDEO`; each accepted non-cancelled attempt reports exactly one error on main; cancellation wins over a racing error. | Temporary app-cache files plus C-08; no external storage/network | T-01, T-02 | T-03, T-05 | Runnable on supported device. |
| TEST-07 | AC-6; JNI/FFmpeg/ABI regression | Existing Android instrumentation + artifact inspection | `PreviewBindingTest`, `FfmpegLinkageTest`, packaged `libvideolib.so` | Library load, all preview JNI exports reachable without a surface, idempotent no-surface lifecycle, linked FFmpeg version, both ARM payloads, and 16 KB alignment remain valid. | Existing tests; generated AAR/native ELF | T-01, T-02 | T-06, T-07 | Device tests runnable per attached ABI; package inspection runnable locally after assemble. |

### Source-only checks

| Check | Scope | Evidence required | Owner |
| --- | --- | --- | --- |
| S-01 | C-01/C-02 public/JNI path | Exact path/symbol trace showing copied JNI path, worker acceptance token, stable descriptors/error values, thread-valid callback bridge, attempt filtering, and main-handler delivery. | T-01 |
| S-02 | C-03 through C-07 native decode/render path | Exact path/symbol trace showing per-attempt FFmpeg resource cleanup, packet-loop frame emission before EOF, monotonic/fallback pacing, synchronous one-frame handoff, cancellation checks, terminal claim, worker join, and EGL teardown order. | T-02 |

### Module integration matrix

| Check-ID | Changed module | Affected consumer/external contract | Boundary | Command or device/manual check | Why required | Owner integration-testing task |
| --- | --- | --- | --- | --- | --- | --- |
| I-01 | `videolib` tests | Public/JNI/native runtime behavior | Kotlin → JNI → FFmpeg worker → EGL surface → Kotlin callback | `./gradlew :videolib:connectedDebugAndroidTest` on an attached supported ARM device | Compilation cannot prove visible progressive presentation, callback threading, or lifecycle races. | T-06 |
| I-02 | `videolib` tests only; production unchanged | Native build/link contract | CMake, FFmpeg static archives, EGL/GLES, `libvideolib.so` | `./gradlew :videolib:assembleDebug` | Proves configured Kotlin/C++ compilation and native linkage for both filtered ABIs; does not prove runtime rendering. | T-07 |
| I-03 | `videolib` tests only; production unchanged | Unknown external AAR consumers and supported ABI devices | Generated AAR/JNI/ELF package | Inspect `videolib/build/outputs/aar/videolib-debug.aar`: both `jni/arm64-v8a/libvideolib.so` and `jni/armeabi-v7a/libvideolib.so`; use NDK `llvm-readelf`/`llvm-nm` on each extracted `.so` to verify required `Java_com_cii_videolib_VideoPreview_*` exports, no unresolved FFmpeg references, and every ELF LOAD alignment `>= 0x4000`. | Public native libraries can fail only on one ABI or at load time; build success alone does not establish packaged payload/alignment. | T-07 |
| I-04 | `videolib` tests only; `app` unchanged | Direct in-repository `app` consumer | Gradle project dependency, Kotlin source compatibility, APK native packaging | `./gradlew :app:assembleDebug` | Confirms the unchanged consumer and packaged native dependency remain compatible without app edits. | T-08 |
| I-05 | `videolib` tests only; production unchanged | Runtime behavior on both supported ABI families | Local playback, FFmpeg decode, EGL presentation, lifecycle/callback ownership | Run TEST-01 through TEST-07 on `arm64-v8a` and `armeabi-v7a` supported devices, recording device/API/ABI and outcomes; use the unchanged app flow for an additional visible playback observation. | Only per-ABI device execution proves FFmpeg/EGL/JNI thread and lifecycle behavior; no result may be generalized from compile-only evidence. | T-09 |

## 8. Shared infrastructure and risk constraints

| Risk/constraint | Affected Change/Task/AC IDs | Required serialization or verification | Owning response task |
| --- | --- | --- | --- |
| Producer RGBA reuse before GL upload completes could corrupt frames or crash. | C-04 through C-07; T-02/T-04; AC-2/AC-3 | Source-confirm `runSync` completion-bounded borrowing and exercise progressive visible frames; any future async handoff would require independent ownership and renewed design. | T-02, T-04 |
| A decode-ahead queue could become unbounded and increase stale-frame latency/memory. | C-03/C-04; T-02/T-04; AC-3 | Confirm the packet-loop performs one synchronous render handoff before continuing and that no queue/cache is introduced. | T-02 |
| Stop/detach/release can race pacing, swscale, rendering, callback delivery, and EGL/window destruction. | C-01 through C-07, C-11; T-01/T-02/T-05; AC-4/AC-5 | Serialize cancellation claim and worker quiescence before EGL/window/native destruction; execute TEST-03 through TEST-05 with event-based synchronization. | T-02, T-05 |
| JNI export, callback descriptor, JavaVM attachment, global-ref, or error-value regression affects all consumers. | C-01/C-02/C-12/C-16; T-01/T-06/T-07; AC-5/AC-6 | Keep Kotlin/JNI sides coupled and unchanged; run binding tests and inspect exported symbols. | T-01, T-06, T-07 |
| Progressive source structure may exist while device-visible output is delayed until completion. | C-08 through C-10; T-03/T-04/T-06/T-09; AC-1 through AC-3 | Require an actually consumed surface frame before terminal completion/EOF; do not substitute logging, decoded-frame count, or a numeric latency claim. | T-04, T-06, T-09 |
| FFmpeg decode/conversion resource misuse could leak or double-release attempt data. | C-04; T-02/T-04/T-05; AC-2 through AC-5 | Preserve `AttemptResources` ownership, per-attempt `SwsContext`, packet/frame unref/free, and synchronous RGBA borrowing; run completion/cancellation/failure paths. | T-02, T-04, T-05 |
| One ABI, JNI symbol, static FFmpeg link, or 16 KB alignment can regress independently. | C-13 through C-16; T-07/T-09; AC-6 | Keep build/CMake/vendored inputs read-only; inspect both generated ABI payloads and execute on both supported ABI families. | T-07, T-09 |
| The shared test fixture is read by two suites. | C-08/C-09; T-03/T-04/T-05 | T-03 owns fixture creation. T-04 and T-05 may only read it and must not mutate shared fixture files; fixture changes serialize through T-03. | T-03 |

No DI, database, socket, navigation, resource, schema, publication, or shared application-state work is planned. No generated output, vendored FFmpeg header/archive, Gradle file, CMake file, JNI signature, production source, or app source is to be edited under this plan.
