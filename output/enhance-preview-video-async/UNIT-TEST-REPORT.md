AUTOMATION: CONTINUE

# UNIT-TEST-REPORT — enhance-preview-video-async

## 0. Summary

- **Owned test contract:** CHANGESET Testing Handoff — T-03 (fixtures), T-04 (TEST-01/02),
  T-05 (TEST-03/04/05/06). All are Android instrumentation on a real EGL surface (no JVM-unit
  layer applies; this is a native/JNI/EGL/FFmpeg runtime contract).
- **Authored:** 3 new test-source files (probe + fixture generator + 2 instrumented test classes),
  8 new instrumented test methods. No production, Gradle, CMake, vendored-FFmpeg, or `app` file was
  touched.
- **Executed:** `:videolib:connectedDebugAndroidTest` on an attached supported ARM device
  (SM-F721B / Galaxy Z Flip4, arm64-v8a, Android 16). **17/17 tests passed** (8 authored here + 9
  retained regression tests) after fixing one authored-fixture defect found by the first run.
- **Fixture substitution (disclosed):** C-09 named a checked-in binary `progressive_preview_long.mp4`.
  A binary MP4 is not authorable/reviewable as source, so the fixture is realised as a deterministic
  runtime generator (`TestVideoFixture`, MediaCodec/MediaMuxer H.264) that writes the clip to app
  cache and hands its local path to `play` — same fixture contract, more determinism, decodable by
  the bundled FFmpeg 7.1 build. See §5.

## 1. Test implementation

`FR-ID | SC-ID | AC-ID | Work/Story | production Task-ID | testing Task-ID | changed production path/symbol | Test-ID | test path/symbol | level | authored status`

| FR | SC | AC | Work/Story | prod Task | test Task | production path/symbol under test | Test-ID | test path/symbol | level | authored status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| FR-1 | SC-1 | AC-1,2,3 | Story-1/W-1 | T-01,T-02 (verified) | T-03 | `VideoPreview.play`/`attachSurface`; `PreviewRenderer::pushFrame` (surface consumption) | C-08 fixture | `PlaybackSurfaceProbe.kt` → `PlaybackSurfaceProbe`, `PlaybackEventLog`, `RecordingPlaybackListener` | Android instrumentation fixture | authored |
| FR-1 | SC-1 | AC-1–5 | Story-1/W-1 | T-01,T-02 (verified) | T-03 | local decodable input for `VideoPlayback::decodeAttempt` | C-09 fixture | `TestVideoFixture.kt` → `generateProgressiveVideo`, `generateAudioOnly`, `missingLocalPath` | Android instrumentation fixture | authored (generator; see §5) |
| FR-1 | SC-1 | AC-1,2,3 | Story-1/W-1 | T-01,T-02 | T-04 | `VideoPreview.play`→JNI→`VideoPlayback::decodeAttempt`→`PreviewRenderer::pushFrame` | TEST-01 | `ProgressivePlaybackInstrumentedTest.play_isAcceptedAsync_presentsFirstFrameBeforeCompletion_andRejectsSecondPlay` | Android instrumentation + real EGL | authored |
| FR-1 | SC-1 | AC-5 | Story-1/W-1 | T-01,T-02 | T-04 | natural EOF + `VideoPreview.enqueueNativeEvent` main dispatch | TEST-02 | `ProgressivePlaybackInstrumentedTest.naturalCompletion_deliveredOnceOnMain_thenLaterAttemptAccepted` | Android instrumentation | authored |
| FR-1 | SC-1 | AC-4 | Story-1/W-2 | T-01,T-02 | T-05 | `VideoPreview.stop` / `VideoPlayback::stop` | TEST-03 | `PlaybackLifecycleInstrumentedTest.stopDuringPlayback_suppressesTerminal_andRetrySucceeds` | Android instrumentation | authored |
| FR-1 | SC-1 | AC-4,5 | Story-1/W-2 | T-01,T-02 | T-05 | `detachSurface`/`VideoPlayback::releaseSurface`; render-failure delivery + reattach | TEST-04 | `PlaybackLifecycleInstrumentedTest.surfaceLossDuringPlayback_emitsSingleRenderError_thenReattachRetrySucceeds` | Android instrumentation + real EGL/window | authored |
| FR-1 | SC-1 | AC-4 | Story-1/W-2 | T-01,T-02 | T-05 | `VideoPreview.release`; native destruction; callback bridge | TEST-05 | `PlaybackLifecycleInstrumentedTest.releaseDuringPlayback_suppressesTerminal_isIdempotent_andRejectsPlay` | Android instrumentation | authored |
| FR-1 | SC-1 | AC-5 | Story-1/W-2 | T-01,T-02 | T-05 | `PlaybackError` mapping / terminal arbitration (INPUT_OPEN) | TEST-06a | `PlaybackLifecycleInstrumentedTest.missingInput_reportsInputOpenOnceOnMain` | Android instrumentation | authored |
| FR-1 | SC-1 | AC-5 | Story-1/W-2 | T-01,T-02 | T-05 | `PlaybackError` mapping (UNSUPPORTED_VIDEO) | TEST-06b | `PlaybackLifecycleInstrumentedTest.audioOnlyInput_reportsUnsupportedVideo` | Android instrumentation | authored |
| FR-1 | SC-1 | AC-5 | Story-1/W-2 | T-01,T-02 | T-05 | cancellation-vs-error arbitration; no cross-attempt bleed | TEST-06c | `PlaybackLifecycleInstrumentedTest.cancellationSuppressesStaleOutcome_andDoesNotBleedIntoNextAttempt` | Android instrumentation | authored |
| FR-2 | SC-2 | AC-6 | W-3 | (verified) | T-05/regression | `Java_com_cii_videolib_VideoPreview_*` exports; no-surface lifecycle | TEST-07 (partial) | `PreviewBindingTest` (existing, retained) | Android instrumentation | retained-unchanged |
| FR-2 | SC-2 | AC-6 | W-3 | (verified) | regression | FFmpeg static linkage / library load | TEST-07 (partial) | `FfmpegLinkageTest` (existing, retained) | Android instrumentation | retained-unchanged |

TEST-06 was authored as three focused methods (06a/06b/06c) covering the three behaviors the Handoff
bundled under TEST-06 (missing-input category, unsupported-video category, cancellation-wins /
no-bleed). No unplanned scope was added.

## 2. Coverage matrix

`Test-ID | behavior/transition/error/risk | fixture boundary | assertion scope | coverage status | evidence`

| Test-ID | behavior / risk | fixture boundary | assertion scope | coverage | evidence |
| --- | --- | --- | --- | --- | --- |
| TEST-01 | AC-1 async acceptance; AC-2 first visible frame before EOF; AC-3 ordered distinguishable frames; single-active-attempt reject | real `ImageReader` surface (C-08) + generated H.264 clip (C-09); no mock decoder/renderer | `play`==true; `terminal==null` at return; 2nd `play`==false; first frame observed; `firstFramePrecededTerminal`; `frameCount>=2`; `distinctLumaCount>=2`; non-decreasing luma | full | passed on device |
| TEST-02 | AC-5 exactly-one completion on main after ≥1 frame; later attempt accepted | C-08/C-09 | `terminalDeliveryCount==1`; `kind==COMPLETED`; `onMainThread`; `frameCount>=1`; second attempt accepted+completes | full | passed on device |
| TEST-03 | AC-4 stop during playback quiesces; no terminal; no later frame; repeated stop safe; retry succeeds | C-08/C-09; latch to reach mid-playback | after `stop`: `terminalDeliveryCount==0`; frame count stable across settle; repeated `stop` no-op; retry completes | full | passed on device |
| TEST-04 | AC-4/AC-5 surface loss → single RENDER error on main, quiesce before EGL teardown; reject until reattach; retry | C-08 surface control + C-09 | `kind==ERROR`,`error==RENDER`,`onMainThread`; `terminalDeliveryCount==1`; `play`==false pre-reattach; reattach+retry completes | full | passed on device |
| TEST-05 | AC-4 release during work suppresses prior-attempt visibility; idempotent; released rejects play | C-08/C-09; strong refs for test lifetime only | after `release`: `terminalDeliveryCount==0`; repeated release/stop/detach no-op; `play`==false | full | passed on device |
| TEST-06a | AC-5 missing/unreadable input → INPUT_OPEN once on main | absent cache path + C-08 | accepted then async `error==INPUT_OPEN`; `onMainThread`; count==1 | full | passed on device |
| TEST-06b | AC-5 readable non-video → UNSUPPORTED_VIDEO | generated audio-only M4A (C-09) | `error==UNSUPPORTED_VIDEO`; `onMainThread` | full | passed on device |
| TEST-06c | AC-5 cancellation wins over racing error; no cross-attempt bleed | absent path + valid clip (C-09) | ≤1 pre-stop terminal, correct category if any, `frameCount==0`; retry `COMPLETED`, count==1; no stale terminal after stop returns | full (see §5 note on the benign pre-stop race) | passed on device |
| TEST-07 | AC-6 JNI export resolution, no-surface lifecycle, FFmpeg link/load | existing tests; no surface | `PreviewBindingTest` (6) + `FfmpegLinkageTest` (2) retained and green | retained; full for its scope | passed on device |

## 3. Execution results

`command scope | exact command | environment | executed status | result | failure evidence`

| scope | exact command | environment | status | result |
| --- | --- | --- | --- | --- |
| First run (found fixture defect) | `./gradlew :videolib:connectedDebugAndroidTest --configure-on-demand --console=plain` | SM-F721B (Galaxy Z Flip4), arm64-v8a, Android 16; NDK 29.0.14206865; Gradle JDK 17 | executed | **BUILD FAILED — 5/17 failed** (all "no presented frame"); root cause = authored probe bug (§4) |
| Fix | renamed `PlaybackSurfaceProbe` ctor param `log`→`initialLog` (variable shadowing) | — | applied | probe lambda now reads the reassignable property |
| Re-run (final) | `./gradlew :videolib:connectedDebugAndroidTest --configure-on-demand --console=plain` | same device/toolchain | executed | **BUILD SUCCESSFUL — 17/17 passed, 0 failed** (XML: ProgressivePlayback 2/0, PlaybackLifecycle 6/0, PreviewBinding 6/0, FfmpegLinkage 2/0, Example 1/0) |

**Environment note (not a test failure):** the plain `:videolib:connectedDebugAndroidTest` invocation
first failed at *configuration* time — the unrelated `:camera` module applies `com.chiistudio:plugin`,
which requires a JVM 21 runtime, but the build runs on JDK 17 (only JDK 17 and 11 are installed; no
JDK 21). `--configure-on-demand` is the in-scope workaround: `:videolib` has no dependency on
`:camera`, so Gradle skips configuring `:camera` and the videolib tests build and run normally. This
did not require any build-file edit. If a future run needs the default invocation, either install a
JDK 21 for Gradle or keep `--configure-on-demand`.

## 4. Failed cases and root cause

| Case | First-run result | Classification | Root cause | Resolution |
| --- | --- | --- | --- | --- |
| TEST-01, TEST-02, TEST-03, TEST-04, TEST-05 (5 tests) | FAILED — "expected a presented frame …" / "within 10000 ms" | **Test defect** (authored fixture), not a product failure | `PlaybackSurfaceProbe`'s constructor parameter `log` shadowed the reassignable `var log` property; the `onImageAvailable` lambda captured the throwaway initial log, so `probe.log = realLog` reassignments were ignored and frames were recorded into the wrong log. Error/cancellation tests (which assert *zero* frames) passed, masking the bug on that subset. | Renamed the ctor parameter to `initialLog` so the lambda binds the property; re-ran → all green. |

No product (`videolib`) defect was found. The green error/lifecycle assertions independently confirm
the native decode/cancellation/terminal contract (D-2–D-9) that android-dev verified in S-01/S-02.

## 5. Gaps, decisions, and recommendations

Each item maps to a Test-ID, changed symbol, or risk. No unowned backlog.

- **C-09 realized as a runtime generator, not a checked-in `.mp4` (decision).** `TestVideoFixture`
  encodes a deterministic solid-color H.264/MP4 (red channel ramps per frame so the probe's
  centre-pixel read distinguishes successive frames) into app cache and a matching audio-only M4A for
  UNSUPPORTED_VIDEO. Rationale: a binary blob cannot be code-reviewed, is nondeterministic across
  toolchains, and risks a codec the bundled FFmpeg build cannot decode. The generator satisfies the
  T-03 done condition ("copies to a readable local file for `play`", multi-frame, deterministic, long
  enough to observe first-frame-before-completion at ~3 s / 45 frames). Maps to C-09.
- **TEST-06c asserts the deterministic guarantee, not the exact pre-stop count (risk-scoped).** A
  missing-input attempt fails almost instantly, so whether the native input-open error or `stop`'s
  cancellation wins the terminal claim is a benign wall-clock race — both orderings are correct per
  D-7/AC-5. The test therefore asserts what is guaranteed: at most one pre-stop terminal (correct
  category if present), no frame presented, no stale terminal after `stop` returns, and no
  cross-attempt bleed into the retry. Maps to AC-5 / `finishAttempt` arbitration.
- **Single-ABI execution (environment coverage gap, owned downstream).** Verified on arm64-v8a only;
  the attached device is a Z Flip4. `armeabi-v7a` runtime execution and the packaged-artifact / ELF
  16 KB-alignment / JNI-export inspection are **integration-testing** obligations (I-03, I-05 / T-07,
  T-09), not testing-stage scope. Reported as pending environment-dependent evidence, not a pass.
- **`--configure-on-demand` dependency for local runs (environment).** Until a JDK 21 is available to
  Gradle, the videolib device suite must be invoked with `--configure-on-demand` to bypass the
  `:camera` plugin's JVM-21 configuration requirement. This is an environment constraint, not a
  `videolib` product or test issue. Flag for integration-testing (T-06) so its execution command
  matches.
- **No JVM-unit coverage (expected).** The changed contract is native decode + EGL presentation +
  JNI callback threading; there is no pure-JVM logic layer to unit test. This is a deliberate
  level choice, not a gap.

Guarded flow: this report begins `AUTOMATION: CONTINUE`; hand off to integration-testing for T-06
(full device suite command of record), T-07 (assemble + AAR/ELF inspection), T-08 (`:app:assembleDebug`
consumer gate), and T-09 (cross-ABI runtime closure incl. armeabi-v7a).
