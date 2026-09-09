AUTOMATION: CONTINUE

# IMPLEMENT-PLAN — control-preview-video

## 1. Planning control

| Item | Status / value |
| --- | --- |
| Source design | `output/control-preview-video/SOLUTION-DESIGN.md`, guarded outcome `AUTOMATION: CONTINUE`, working-tree revision read on 2026-09-09 |
| Planning outcome | `CONTINUE` — all AC-01–AC-12 map to production, testing, and integration tasks; no unresolved planning input remains |
| Primary owner | `:videolib` |
| Changed modules | `:videolib` only |
| Consumer closure | `:videolib` → direct `:app` consumer → transitive `:benchmark` target/package consumer; external library consumers unknown |
| Crossed contracts | Public Kotlin API; hand-mangled JNI names/signatures; native playback state/worker lifetime; FFmpeg demux/decode/seek/timestamps; synchronous RGBA-to-EGL presentation; ARM ABI packaging |
| Verification closure | `:videolib` build, `:app` consumer build, native symbol/ABI artifact inspection, supported ARM EGL/GLES device suite, per-packaged-ABI load/control smoke coverage |
| Investigation depth | Deep; approximately 8 focused planning lookups within the 20-lookup cap; no escalation |
| Knowledge graph | No registered repository graph was available in the immediately preceding analysis; focused source/symbol inspection was used and absence of graph edges was not treated as absence of consumers |
| Delivery-backlog mode | Not requested; no estimates, priority, points, sprint packing, or G/W/T criteria are included |

Assumptions fixed by the approved design:

- Planned public semantic surface: `pause(): Boolean`, `resume(): Boolean`, `setLooping(enabled: Boolean): Boolean`, `setPlaybackSpeed(speed: Double): Boolean`, and `seekTo(positionMs: Long): Boolean`. Names follow the existing `VideoPreview` command style and Android playback terminology; Boolean is the approved synchronous accepted/rejected result.
- Matching private JNI declarations and literal exported names use `nativePause`, `nativeResume`, `nativeSetLooping`, `nativeSetPlaybackSpeed`, and `nativeSeekTo` on `VideoPreview`.
- Loop defaults to off and speed to `1.0`; both settings persist per non-released preview instance. Pause/resume/seek require ready active media. Seek positions are clamped; valid speeds are finite and `≥0.1`.
- FFmpeg control/seek/reset stays worker-confined. The existing renderer remains the only visible-frame path and retains synchronous pixel-lifetime and single EGL-thread behavior.

Blockers: none. UI, audio, remote APIs, storage, DI, Gradle configuration, dependency upgrades, publication, and vendored FFmpeg changes remain out of scope.

### Bounded investigation ledger

| Lookup | Planning evidence |
| --- | --- |
| Approved solution design | Complete AC/state/component/boundary/verification contracts |
| `VideoPreview.kt` | Exact public facade and private native declaration ownership |
| `videolib.cpp` | Exact hand-mangled export/callback bridge and opaque native owner |
| `video_playback.h/.cpp` | Shared state, control collision surface, worker, clock, FFmpeg decode, EOF, stop/release behavior |
| `preview_renderer.h/.cpp` and `render_thread_executor.h` | Renderer is reusable without modification; synchronous RGBA lifetime and EGL-thread ordering |
| `PlaybackSurfaceProbe.kt` | Existing visible-frame/luma/terminal test seam; timing/count waiting needs extension |
| `TestVideoFixture.kt` and current playback tests | Existing deterministic 3-second ramp clip and lifecycle/error coverage are reusable |
| Gradle/CMake/app/benchmark edges from approved analysis | Owner, consumer, ABI, linkage, and minimal integration closure |

## 2. Change-surface inventory

| Change-ID | Existing/new | Action | Exact path | Symbol/resource/config key | Design-Ref(s) | Responsibility | Evidence/design decision | Shared/collision key |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| C-01 | Existing | Extend | `videolib/src/main/java/com/cii/videolib/VideoPreview.kt` | `VideoPreview`; planned public controls and matching private native declarations | none — requirement/code-driven | Validate facade state/value preconditions, expose accepted/rejected controls, and retain existing lifecycle/listener behavior | Solution §§1, 3, 5; AC-01–AC-10 | `public-playback-facade` |
| C-02 | Existing | Extend | `videolib/src/main/cpp/video_playback.h` | `PlaybackState`; `VideoPlayback`; planned private control/timeline state and helpers | none — requirement/code-driven | Declare authoritative pause/seek/configuration state and coordinator contracts | Solution §§2–5; AC-01–AC-12 | `native-playback-owner` |
| C-03 | Existing | Extend | `videolib/src/main/cpp/video_playback.cpp` | `VideoPlayback::play`, `waitUntil`, `decodeAttempt`, `runAttempt`, `finishAttempt`; planned control methods/private helpers | none — requirement/code-driven | Implement ordered pause/resume, rate clock, latest seek, EOF loop, cancellation, and terminal behavior while keeping FFmpeg worker-confined | Solution §§2, 4, 6; AC-01–AC-12 | `native-playback-owner` |
| C-04 | Existing | Extend | `videolib/src/main/cpp/videolib.cpp` | Planned exports `Java_com_cii_videolib_VideoPreview_nativePause`, `nativeResume`, `nativeSetLooping`, `nativeSetPlaybackSpeed`, `nativeSeekTo` | none — requirement/code-driven | Pair Kotlin declarations with exact JNI exports and scalar accepted/rejected forwarding | Solution §§3, 5, 6; JNI contract | `jni-playback-contract` |
| C-05 | Existing | Extend | `videolib/src/androidTest/java/com/cii/videolib/PlaybackSurfaceProbe.kt` | `FrameObservation`, `PlaybackEventLog.recordFrame`; planned monotonic presentation-time observation and bounded frame-count wait | none — requirement/code-driven | Add test-only observability for quiescence, pacing, looping, and seek-visible frames without production hooks | Existing probe contract; solution verification obligations | `playback-test-probe` |
| C-06 | Existing | Extend | `videolib/src/androidTest/java/com/cii/videolib/PreviewBindingTest.kt` | `previewNativeMethods_resolve_andNoOpSafely_withoutSurface`; planned control binding/default-state coverage | none — requirement/code-driven | Prove all new public-to-JNI calls resolve and invalid pre-attach/released states reject safely | JNI risk; AC-01, AC-02, AC-04, AC-05, AC-09 | `binding-test` |
| C-07 | New | Create | `videolib/src/androidTest/java/com/cii/videolib/PlaybackPauseSpeedInstrumentedTest.kt` | `PlaybackPauseSpeedInstrumentedTest` | none — requirement/code-driven | Cover pause/resume, configuration persistence/defaults, speed changes, invalid speed, and high-speed bounded behavior | AC-01, AC-02, AC-04, AC-11 | `pause-speed-test` |
| C-08 | New | Create | `videolib/src/androidTest/java/com/cii/videolib/PlaybackSeekInstrumentedTest.kt` | `PlaybackSeekInstrumentedTest` | none — requirement/code-driven | Cover playing/paused seek, clamping, visible target, latest-seek-wins, and continuation disposition | AC-05–AC-07, AC-09, AC-10 | `seek-test` |
| C-09 | New | Create | `videolib/src/androidTest/java/com/cii/videolib/PlaybackLoopInstrumentedTest.kt` | `PlaybackLoopInstrumentedTest` | none — requirement/code-driven | Cover same-attempt looping, no intermediate completion, retained speed, disable-at-EOF ordering, and final completion | AC-03, AC-08, AC-12 | `loop-test` |
| C-10 | Existing | Extend | `videolib/src/androidTest/java/com/cii/videolib/PlaybackLifecycleInstrumentedTest.kt` | `PlaybackLifecycleInstrumentedTest`; planned control/lifecycle race regression coverage | none — requirement/code-driven | Preserve stop/detach/release quiescence, error, retry, stale-frame, and callback guarantees while controls are pending | AC-09; high-risk lifecycle contract | `lifecycle-test` |

Generated outputs under `build/`, `.cxx/`, and `.externalNativeBuild/` are generated — do not edit. `videolib/src/main/cpp/ffmpeg/**`, `videolib/src/main/cpp/CMakeLists.txt`, `videolib/build.gradle.kts`, renderer/GL sources, `app/**`, and `benchmark/**` are verification inputs only and are not planned change surfaces.

## 3. Work-item backlog

| FR-ID | SC-ID | AC-ID | Work-ID/Story-ID | Outcome | Module/screen | Depends on |
| --- | --- | --- | --- | --- | --- | --- |
| FR-01 | SC-01 | AC-01, AC-02, AC-09 | W-01 / Story-01 | An active preview pauses without later progress frames and resumes from retained media position under existing lifecycle precedence. | `:videolib`; no screen | — |
| FR-03 | SC-03 | AC-04, AC-11 | W-02 / Story-03 | A preview retains valid `≥0.1×` speed configuration, re-anchors active pacing without position jumps, rejects invalid values, and remains bounded at high rates. | `:videolib`; no screen | W-01 clock/control arbitration |
| FR-04, FR-05 | SC-04, SC-05, SC-06 | AC-05, AC-06, AC-07, AC-09, AC-10 | W-03 / Story-04 | Playing and paused attempts seek to a clamped millisecond position, render the latest target, and retain their pre-seek disposition. | `:videolib`; no screen | W-01 control arbitration; W-02 playback-clock contract |
| FR-02, FR-06 | SC-02, SC-07 | AC-03, AC-08, AC-12 | W-04 / Story-02 | Enabled looping restarts the same attempt without intermediate completion; disabling makes the current pass final and completes once. | `:videolib`; no screen | W-01 control arbitration; serialization after W-03 on shared native owner |
| FR-01–FR-06 | SC-01–SC-07 | AC-01–AC-12 | W-05 / Integration | Public/native linkage, direct consumer compatibility, lifecycle regression, packaged ABIs, and real device behavior are verified. | `:videolib`, consumer `:app`, transitive package target `:benchmark`; no screen | W-01–W-04 |

## 4. Task backlog

| Task-ID | Work-ID/Story-ID | Owning module | Owner stage | Objective | Change-IDs / exact path-symbol scope | Design-Ref(s) | Preconditions / inputs | Invariants | Done condition | Verification / Test-ID / Check-ID | Depends on | Collision key |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| TASK-01 | W-01 / Story-01 | `:videolib` | android-dev | Deliver pause/resume through the complete public → JNI → native coordinator → clock path. | C-01 `VideoPreview.pause/resume` + externals; C-02 `PlaybackState`, `VideoPlayback` control declarations/state; C-03 coordinator/wait/clock paths; C-04 exact pause/resume exports | none — requirement/code-driven | Approved solution AC-01, AC-02, AC-09; current one-attempt worker and synchronous renderer | Existing `play/stop/detach/release`, callback, single-owner-thread, worker quiescence, renderer lifetime, and default playback remain unchanged; accepted pause returns only after in-flight progress presentation is quiescent | Public calls accept/reject the designed states; paused media position is retained; resume re-anchors clock; JNI names/types pair exactly; diff contains no unrelated files | TEST-01, TEST-07, TEST-08; source check plus `./gradlew :videolib:assembleDebug` | — | `public-playback-facade` + `jni-playback-contract` + `native-playback-owner` |
| TASK-02 | W-02 / Story-03 | `:videolib` | android-dev | Add persistent speed configuration and rate-aware pacing/late-frame behavior. | C-01 `VideoPreview.setPlaybackSpeed` + external; C-02 speed/control-clock state; C-03 rate validation, position-preserving re-anchor, deadline/late-frame paths; C-04 exact speed export | none — requirement/code-driven | TASK-01 clock/control ordering; AC-04, AC-11 | Defaults `1.0×`; accept every finite `≥0.1×`; reject invalid without mutation/terminal event; paused state stays paused; no unbounded presentation queue; frame order remains monotonic | Speed works before/current/later attempts per design, active changes preserve position, invalid values are rejected, high rate stays bounded, JNI pairing is exact | TEST-02, TEST-03, TEST-08; source check plus `./gradlew :videolib:assembleDebug` | TASK-01 | `public-playback-facade` + `jni-playback-contract` + `native-playback-owner` |
| TASK-03 | W-03 / Story-04 | `:videolib` | android-dev | Add worker-confined millisecond seek with clamping, latest-target ordering, visible-frame settlement, and playing/paused recovery. | C-01 `VideoPreview.seekTo` + external; C-02 seeking state/request identity; C-03 FFmpeg reposition/flush/warm-up, timestamp reset, stale-result suppression, renderer handoff; C-04 exact seek export | none — requirement/code-driven | TASK-01 control ordering; TASK-02 clock re-anchor; AC-05–AC-07, AC-09, AC-10 | FFmpeg contexts stay on playback worker; negative/beyond-duration targets clamp; no stale target owns final state; existing RGBA ownership and synchronous render call remain; lifecycle cancellation wins | Playing seek renders target then progresses; paused seek renders target then stays paused; rapid seek settles latest target; accepted media/render failures use existing terminal categories exactly once | TEST-04, TEST-05, TEST-07, TEST-08; source check plus `./gradlew :videolib:assembleDebug` | TASK-02 | `public-playback-facade` + `jni-playback-contract` + `native-playback-owner` |
| TASK-04 | W-04 / Story-02 | `:videolib` | android-dev | Add persistent loop configuration and same-attempt EOF restart/final-completion arbitration. | C-01 `VideoPreview.setLooping` + external; C-02 loop configuration; C-03 EOF/drain/reset and terminal-claim paths; C-04 exact loop export | none — requirement/code-driven | TASK-01 control ordering; AC-03, AC-08, AC-12; serialize after TASK-03 due shared files | Default off; loop/speed retained across passes; one worker/attempt/listener; no intermediate callback; latest ordered loop value decides EOF; reset failure is one decode error | Enabled EOF visibly restarts without terminal delivery or resource growth; disabled current final pass completes once; JNI pairing exact; legacy non-looping EOF unchanged | TEST-06, TEST-07, TEST-08; source check plus `./gradlew :videolib:assembleDebug` | TASK-03 (`ownership serialization`; behavioral prerequisite is TASK-01) | `public-playback-facade` + `jni-playback-contract` + `native-playback-owner` |
| TASK-05 | W-05 / Integration | `:videolib` | testing | Extend the existing black-box surface probe with monotonic presentation timing and bounded wait-for-frame-count support. | C-05 `FrameObservation`, `PlaybackEventLog.recordFrame`, planned frame-count wait | none — requirement/code-driven | Existing consumable ImageReader surface and deterministic luma ramp | Test-only code; no production observability hook; thread-safe observations; bounded synchronization rather than sleeps for positive outcomes | Probe can compare presentation intervals and await additional frames while preserving current ordering/luma/terminal behavior | Source inspection; supports TEST-01–TEST-06 | TASK-04 | `playback-test-probe` |
| TASK-06 | W-01/W-02 / Story-01, Story-03 | `:videolib` | testing | Author device coverage for pause/resume and speed contracts. | C-07 `PlaybackPauseSpeedInstrumentedTest` | none — requirement/code-driven | TASK-02, TASK-05; deterministic progressive fixture and real surface probe | Assert public acceptance and observable frame/timing behavior; no production hooks; preserve teardown in every outcome | Coverage exists for quiescent pause, resume progression, idempotency, default/persistent/dynamic speed, invalid values, ordered high-speed behavior, and no terminal corruption | TEST-01, TEST-02, TEST-03; execution deferred to CHK-04/CHK-05 | TASK-05 | `pause-speed-test` |
| TASK-07 | W-03 / Story-04 | `:videolib` | testing | Author device coverage for seek target visibility, disposition, bounds, and latest-wins behavior. | C-08 `PlaybackSeekInstrumentedTest` | none — requirement/code-driven | TASK-03, TASK-05; deterministic luma/time fixture | Evaluate visible target through public API and surface output; use bounded synchronization; lifecycle always cleaned up | Coverage exists for playing continuation, paused settlement, negative/end clamping, rapid newer seek, no stale settled frame, and accepted seek error mapping | TEST-04, TEST-05; execution deferred to CHK-04/CHK-05 | TASK-05 | `seek-test` |
| TASK-08 | W-04 / Story-02 | `:videolib` | testing | Author device coverage for loop passes and final completion. | C-09 `PlaybackLoopInstrumentedTest` | none — requirement/code-driven | TASK-04, TASK-05; ramp fixture makes wrap visible | Same attempt/listener; no intermediate terminal; retained rate; bounded wait; cleanup disables/stops/releases | Coverage observes at least one end-to-start wrap without callback, then disablement and exactly one main-thread completion; EOF race does not duplicate/lose terminal | TEST-06; execution deferred to CHK-04/CHK-05 | TASK-05 | `loop-test` |
| TASK-09 | W-05 / Integration | `:videolib` | testing | Extend no-surface/released-state binding coverage across every new JNI export and public validation result. | C-06 `PreviewBindingTest.previewNativeMethods_resolve_andNoOpSafely_withoutSurface` plus released-state checks | none — requirement/code-driven | TASK-04 complete public/JNI surface | Every new symbol is invoked; invalid states reject safely; existing NativeLib/load and release idempotency regressions stay intact | Instrumentation source covers symbol resolution and default/invalid control calls without requiring EGL | TEST-08; execution deferred to CHK-04/CHK-05 | TASK-04 | `binding-test` |
| TASK-10 | W-05 / Integration | `:videolib` | testing | Extend lifecycle regressions for controls pending during stop, detach, release, and retry. | C-10 `PlaybackLifecycleInstrumentedTest` planned control-race cases | none — requirement/code-driven | TASK-04, TASK-05; current lifecycle suite | Stop/release suppress stale frame/terminal; detach emits one render error; no control resurrects attempt; retry keeps configured speed/loop contract | Coverage exists for pause/seek/control races with each lifecycle event and a clean later attempt/reattach recovery | TEST-07; execution deferred to CHK-04/CHK-05 | TASK-05 | `lifecycle-test` |
| TASK-11 | W-05 / Integration | `:videolib` | integration-testing | Compile/link the changed library and all Android-test source. | Verify C-01–C-10; no source edits | none — requirement/code-driven | TASK-01–TASK-10 | No generated/vendored edits; configured NDK/CMake/ABIs/linkage unchanged | `./gradlew :videolib:assembleDebug :videolib:compileDebugAndroidTestKotlin` completes successfully | CHK-01 | TASK-06, TASK-07, TASK-08, TASK-09, TASK-10 | `videolib-build-gate` |
| TASK-12 | W-05 / Integration | `:app` consumer, unchanged | integration-testing | Verify direct consumer source/package compatibility. | Verify `app/build.gradle.kts:55`, `MainActivity2` existing calls, and produced app package; no source edits | none — requirement/code-driven | TASK-04; public compatibility contract | App requires no new control calls; existing automatic playback/lifecycle compiles unchanged | `./gradlew :app:assembleDebug` succeeds and packages the changed `libvideolib.so` for configured ABIs | CHK-02 | TASK-04 | `app-consumer-gate` |
| TASK-13 | W-05 / Integration | `:videolib` | integration-testing | Inspect native exports, packaged ABIs, FFmpeg/EGL linkage, and 16 KB alignment. | Verify outputs generated from C-01–C-04 plus unchanged `CMakeLists.txt`/`build.gradle.kts`; no source edits | none — requirement/code-driven | TASK-11 artifacts | Exact new/old JNI symbols present; both configured ARM ABIs present; library name/linkage/alignment unchanged | Artifact evidence records symbols, ABI entries, dependency/link resolution, and 16 KB alignment for `arm64-v8a` and `armeabi-v7a` | CHK-03 | TASK-11 | `native-artifact-gate` |
| TASK-14 | W-05 / Integration | `:videolib` | integration-testing | Execute real device control/lifecycle behavior across supported packaged ABIs. | Execute C-06–C-10 tests against C-01–C-04 production behavior; no source edits | none — requirement/code-driven | TASK-11, TASK-12, test-capable supported ARM EGL/GLES devices | Runtime evidence only; compilation is not reported as proof of FFmpeg/JNI/EGL behavior; record unavailable ABI/device separately | `./gradlew :videolib:connectedDebugAndroidTest` passes on available supported ARM device(s), with explicit load/control smoke evidence for each packaged ABI or an honest environment gap | CHK-04, CHK-05; TEST-01–TEST-08 | TASK-11, TASK-12, TASK-13 | `device-runtime-gate` |

## 5. Dependency map (DAG)

### Typed edges

| From | To | Type | Reason |
| --- | --- | --- | --- |
| TASK-01 | TASK-02 | behavior + ownership serialization | Speed requires the playback-clock/control-arbitration seam and edits the same public/JNI/native owner files. |
| TASK-02 | TASK-03 | behavior + ownership serialization | Seek reuses rate-aware clock re-anchoring and edits C-01–C-04. |
| TASK-03 | TASK-04 | ownership serialization | Loop is behaviorally independent of seek but collides on C-01–C-04 and EOF/decode state. |
| TASK-04 | TASK-05 | stage/test | Test-support authoring begins after the complete public control contract is stable. |
| TASK-05 | TASK-06 | test | Pause/speed coverage uses enhanced observations. |
| TASK-05 | TASK-07 | test | Seek coverage uses enhanced observations. |
| TASK-05 | TASK-08 | test | Loop coverage uses enhanced observations. |
| TASK-04 | TASK-09 | contract + test | Binding coverage requires every final public/JNI control symbol. |
| TASK-05 | TASK-10 | test | Lifecycle race coverage uses bounded frame waits/timestamps. |
| TASK-06, TASK-07, TASK-08, TASK-09, TASK-10 | TASK-11 | test | Library gate must compile all authored instrumentation source. |
| TASK-04 | TASK-12 | contract | Consumer gate requires final public/native production behavior. |
| TASK-11 | TASK-13 | build | Artifact inspection requires built AAR/native outputs. |
| TASK-11, TASK-12, TASK-13 | TASK-14 | build + contract + test | Device execution follows compile, consumer package, and artifact verification. |

Cycle check: PASS. The graph is acyclic; production tasks form one serialized shared-boundary chain, test files fan out only after shared test support, and integration gates converge at TASK-14.

## 6. Execution waves

| Wave | Tasks | Concurrency / serialization reason | Prerequisites before wave |
| --- | --- | --- | --- |
| 1 | TASK-01 | Single task owns the initial public/JNI/native clock and state collision surface. | Approved SOLUTION-DESIGN |
| 2 | TASK-02 | Serialized with TASK-01 on C-01–C-04 and builds on its clock contract. | TASK-01 done and inspected |
| 3 | TASK-03 | Serialized on C-01–C-04; FFmpeg seek and clock reset must integrate with completed speed behavior. | TASK-02 done and inspected |
| 4 | TASK-04 | Serialized on C-01–C-04; EOF/reset changes integrate only after seek collision is closed. | TASK-03 done and inspected |
| 5 | TASK-05 and TASK-09 | May run concurrently: C-05 and C-06 are disjoint test files; TASK-09 needs final JNI contract but not probe changes. | TASK-04 done |
| 6 | TASK-06, TASK-07, TASK-08, TASK-10 | May run concurrently: C-07, C-08, C-09, and C-10 are disjoint; all consume C-05 read-only. | TASK-05 done; TASK-04 done |
| 7 | TASK-11 and TASK-12 | May run concurrently: library/test compilation gate and unchanged app consumer gate own separate Gradle task outputs and make no source edits. | All production/test-source tasks done for TASK-11; TASK-04 done for TASK-12 |
| 8 | TASK-13 | Artifact inspection requires TASK-11 outputs. | TASK-11 done |
| 9 | TASK-14 | Serialized final runtime gate; it consumes all tests/build/artifact evidence and device resources. | TASK-11, TASK-12, TASK-13 done |

## 7. Test scope and verification matrix

### Test scope

| Test-ID | AC-ID / risk | Level | Target component/contract | Behavior / transition / error scope | Fake/fixture boundary | Production Task-ID | Depends on | Execution expectation |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| TEST-01 | AC-01, AC-02, AC-09; pause/lifecycle race | Android instrumentation on real surface | Public facade → JNI → coordinator/clock → renderer | Playing→Paused quiescence after return, retained position, Paused→Playing progression, duplicate idempotency, invalid state rejection | Existing generated ramp MP4 + enhanced real `PlaybackSurfaceProbe`; no production fake | TASK-01 | TASK-05, TASK-06 | Device/environment-dependent; execute in TASK-14 |
| TEST-02 | AC-04; validation/default/persistence risk | Android instrumentation on real surface | Speed public/JNI/configuration/clock contract | Default `1.0×`, pre-play setting, active change without position jump, paused change stays paused, later-attempt persistence, rejection for `<0.1`, NaN, infinities | Ramp MP4 and frame presentation timestamps | TASK-02 | TASK-05, TASK-06 | Device/environment-dependent; execute in TASK-14 |
| TEST-03 | AC-11; unbounded-speed backlog risk | Android instrumentation on real surface | Rate clock and renderer queue behavior | Large finite rate accepted, ordered forward output, bounded completion/loop response, no ever-growing presentation backlog or crash | Ramp MP4; bounded frame/time observations, no mocked decoder | TASK-02 | TASK-05, TASK-06 | Device/environment-dependent; execute in TASK-14 |
| TEST-04 | AC-05, AC-06, AC-07 | Android instrumentation on real surface | Seek public/JNI/FFmpeg/renderer/state contract | Midpoint seek while playing continues; paused seek displays target then remains quiescent; accepted reposition/render failure uses existing category once | Deterministic luma ramp maps visible frame to approximate media region | TASK-03 | TASK-05, TASK-07 | Device/environment-dependent; execute in TASK-14 |
| TEST-05 | AC-09, AC-10; stale target/bounds risk | Android instrumentation on real surface | Seek arbitration and boundary behavior | Negative/end clamp, rapid targets settle latest, no stale settled overwrite, stop/detach/release precedence | Ramp MP4, luma observations, bounded frame waits | TASK-03 | TASK-05, TASK-07 | Device/environment-dependent; execute in TASK-14 |
| TEST-06 | AC-03, AC-08, AC-12; EOF/terminal/resource risk | Android instrumentation on real surface | Loop configuration, FFmpeg EOF reset, terminal bridge | Visible end→start wrap on same attempt, no intermediate terminal, speed retained, disable makes current pass final, exactly one main-thread completion, repeat remains bounded | Ramp MP4, luma wrap, existing recording listener | TASK-04 | TASK-05, TASK-08 | Device/environment-dependent; execute in TASK-14 |
| TEST-07 | AC-09, AC-12; native teardown/callback risk | Android instrumentation on real surface | Existing lifecycle suite with active controls | Stop/release suppress later frames/callbacks; detach produces one render error; pending pause/seek cannot resurrect; clean retry/reattach | Existing lifecycle fixture/probe/listener | TASK-01–TASK-04 | TASK-05, TASK-10 | Device/environment-dependent; execute in TASK-14 |
| TEST-08 | AC-01, AC-02, AC-04, AC-05, AC-09; JNI linkage risk | Android instrumentation, no surface for rejection path | Public facade and all literal JNI exports | New calls resolve, defaults/invalid states reject safely, release remains idempotent, legacy NativeLib exports still resolve | No media fixture for binding path | TASK-01–TASK-04 | TASK-09 | Device/environment-dependent native-load check; execute in TASK-14 |

### Module integration matrix

| Check-ID | Changed module | Affected consumer/external contract | Boundary | Command or device/manual check | Why required | Owner integration-testing task |
| --- | --- | --- | --- | --- | --- | --- |
| CHK-01 | `:videolib` | Public Kotlin/JNI/C++ contract | Kotlin/native compile and Android-test source | `./gradlew :videolib:assembleDebug :videolib:compileDebugAndroidTestKotlin` | Detect declaration/export/C++ errors and test-source drift before device execution | TASK-11 |
| CHK-02 | `:videolib` | Direct `:app` consumer; transitive benchmark target package | Source/runtime packaging | `./gradlew :app:assembleDebug` | Existing app must compile unchanged and package the revised native library | TASK-12 |
| CHK-03 | `:videolib` | External/consumer native ABI contract | JNI symbols, FFmpeg/EGL dependencies, ABIs, 16 KB alignment | Inspect built AAR/APK and `libvideolib.so` for old/new JNI exports, `arm64-v8a` and `armeabi-v7a` entries, native dependencies, and max-page-size alignment | Compilation alone does not prove packaged ABI/symbol/alignment contract | TASK-13 |
| CHK-04 | `:videolib` | Public runtime control contract | FFmpeg worker → RGBA → EGL surface → callback | `./gradlew :videolib:connectedDebugAndroidTest` on a supported ARM EGL/GLES 3.0 device | Executes TEST-01–TEST-08 against real native decode/render/lifecycle behavior | TASK-14 |
| CHK-05 | `:videolib` | Every packaged ABI / unknown external consumers | Native load and control smoke | Run native load plus representative pause/speed/seek/loop/lifecycle coverage on `arm64-v8a` and `armeabi-v7a`, recording any unavailable ABI environment explicitly | Runtime load/thread/FFmpeg/EGL behavior is ABI- and device-dependent | TASK-14 |

The `:benchmark:connectedBenchmarkAndroidTest` default is intentionally omitted: its confirmed code measures app startup and does not call playback controls; CHK-02 validates its affected target/package edge without spending an unrelated device benchmark run.

## 8. Shared infrastructure and risk constraints

| Risk / invariant | Affected Change/Task/AC IDs | Required serialization or verification | Owning response task |
| --- | --- | --- | --- |
| Four slices edit the same public/JNI/native playback owner | C-01–C-04; TASK-01–TASK-04; AC-01–AC-12 | Strict TASK-01 → TASK-02 → TASK-03 → TASK-04 ownership serialization; inspect diff and compile after each slice | TASK-01–TASK-04 |
| Hand-mangled JNI declaration/export mismatch causes runtime linkage failure | C-01, C-04; all production tasks | Pair each Kotlin declaration/export atomically; TEST-08, CHK-01, CHK-03, CHK-04 | TASK-01–TASK-04, TASK-09, TASK-11, TASK-13, TASK-14 |
| FFmpeg contexts/packet/frame/conversion state may be accessed or reset concurrently | C-02, C-03; TASK-03, TASK-04; AC-03, AC-05–AC-08, AC-10, AC-12 | Keep all FFmpeg operations worker-confined; latest control identity and lifecycle cancellation serialized; real device seek/loop coverage | TASK-03, TASK-04, TASK-07, TASK-08, TASK-10, TASK-14 |
| Pause/control command races an in-flight synchronous render | C-02, C-03; TASK-01, TASK-03; AC-01, AC-09, AC-10 | Preserve renderer lock and render-thread ownership; pause/lifecycle return only after required quiescence; TEST-01, TEST-05, TEST-07 | TASK-01, TASK-03, TASK-06, TASK-07, TASK-10, TASK-14 |
| Unbounded valid speed creates overflow, backlog, or resource growth | C-02, C-03; TASK-02; AC-04, AC-11 | Safe clock arithmetic for all finite accepted values; no unbounded frame queue; TEST-02/TEST-03 and device gate | TASK-02, TASK-06, TASK-14 |
| Loop restart duplicates workers/listeners/terminal callbacks or accumulates resources | C-02, C-03; TASK-04; AC-03, AC-08, AC-12 | Same attempt/worker identity, explicit reset and single terminal claim; TEST-06/TEST-07 | TASK-04, TASK-08, TASK-10, TASK-14 |
| Existing `play/stop/detach/release` behavior regresses for app or external consumers | C-01–C-04; TASK-01–TASK-04; AC-09, AC-12 | Preserve defaults and existing symbols/categories; lifecycle suite, app build, and device runtime gate | TASK-10, TASK-12, TASK-14 |
| ABI/linkage/alignment changes accidentally accompany native edits | Verification-only build files/artifacts; TASK-11–TASK-14 | No CMake/Gradle/vendored edits; CHK-01–CHK-05 inspect and run both configured ABI contracts | TASK-11, TASK-13, TASK-14 |

No shared DI, database, socket, navigation, resource, manifest, or publication surface is touched. No task authorizes commit, push, signing, publishing, upload, or distribution.
