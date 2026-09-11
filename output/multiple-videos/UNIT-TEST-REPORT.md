AUTOMATION: CONTINUE

# UNIT-TEST-REPORT — multiple-videos (Testing)

Owned test contract: CHANGESET.md §5 Testing Handoff — **UT-SEG** (JVM) and **ANDT-TL-1..7**
(instrumented). Both authored to the project's existing `videolib` test roots and idioms
(`ProgressivePlaybackInstrumentedTest`, `PlaybackLifecycleInstrumentedTest`, `TestVideoFixture`,
`PlaybackSurfaceProbe`, `PlaybackEventLog`). No production behavior redesigned; no unplanned scope.

Guidance loaded: module-impact (tests stay in owning module `videolib`; public API coverage retained),
regression-analysis (single-clip regression path selected — ANDT-TL-7 / AC-8). Graph unavailable →
source inspection.

---

## 1. Test implementation

`FR | SC | AC | Work | prod Task | testing Task | changed prod path/symbol | Test-ID | test path/symbol | level | authored status`

| FR | SC | AC | Work | prod Task | testing Task | changed prod path/symbol | Test-ID | test path/symbol | level | authored |
|---|---|---|---|---|---|---|---|---|---|---|
| FR1 | SC1 | AC1; interval invariant | W1 | P3 | UT-SEG | `VideoSegment.kt` (`VideoSegment`, `hasValidInterval`) | UT-SEG | `videolib/src/test/java/com/cii/videolib/VideoSegmentTest.kt` (8 `@Test`) | JVM unit | authored |
| FR1 | SC1 | AC1, AC7 | W1 | P4 | ANDT-TL | `VideoPreview.playTimeline`; `video_playback.cpp runTimeline` | ANDT-TL-1 | `…/androidTest/…/TimelinePlaybackInstrumentedTest.kt#multiSegmentTimeline_playsBackToBack_completesExactlyOnceOnMain` | instrumented | authored |
| FR2 | SC2 | AC2, AC3 | W1 | P1 | ANDT-TL | `video_playback.cpp` (seek-to-A, B-exclusive, A-skip) | ANDT-TL-2 | `…#trimmedSegment_presentsOnlyFramesInsideTheHalfOpenWindow` | instrumented | authored |
| FR3 | SC3 | AC4 | W1 | P1 | ANDT-TL | `video_playback.cpp decodeAttempt` (per-segment appearance) | ANDT-TL-3 | `…#perSegmentFilter_appliesOnlyToItsOwnSegment` | instrumented | authored |
| FR4 | SC4 | AC5 | W1 | P1 | ANDT-TL | `video_playback.cpp` (per-segment speed apply) | ANDT-TL-4 | `…#perSegmentSpeed_governsSegmentRate` | instrumented | authored |
| FR5 | SC5 | AC6 | W1 | P1 | ANDT-TL | `video_playback.cpp runTimeline` (advance-at-B, start-at-A) | ANDT-TL-5 | `…#segmentBoundary_advancesToNextSegmentAtItsStartWithItsAppearance` | instrumented | authored |
| FR5 | SC5 | D-6 fail-fast | W1 | P2 | ANDT-TL | `videolib.cpp` (index route); `runTimeline` fail-fast | ANDT-TL-6 | `…#failingSegment_endsTimelineOnceWithFailingSegmentIndex` | instrumented | authored |
| FR1-5 | — | AC8 regression | W1 | P1/P4 | ANDT-TL | `video_playback.cpp runAttempt` (single-clip defaults); shared attempt guard | ANDT-TL-7 | `…#singleClipPlay_isUnchanged_andSharesTheOneAttemptGuardWithTimeline` | instrumented | authored |

New test-only helper: `RecordingTimelineListener` (test-local, inside `TimelinePlaybackInstrumentedTest.kt`)
— records the single `TimelineListener` terminal (completed/error+index/main-thread), mirroring the
existing `RecordingPlaybackListener`. No production observability hook added; the shared fixture files
`TestVideoFixture.kt`/`PlaybackSurfaceProbe.kt` are reused unchanged.

---

## 2. Coverage matrix

`Test-ID | behavior/transition/error/risk | fixture boundary | assertion scope | coverage status | evidence`

| Test-ID | behavior / risk | fixture boundary | assertion scope | coverage | evidence |
|---|---|---|---|---|---|
| UT-SEG | `VideoSegment` value semantics + pure interval rule `0≤A<B` | none (pure value) | construction/defaults; `data class` equality per-field; `copy` immutability; `hasValidInterval` true/false across boundary, empty, inverted, negative-A | full (value type) | 8 `@Test`; B-vs-duration explicitly excluded (native, → ANDT-TL-2) |
| ANDT-TL-1 | ≥2 segments retained, back-to-back, exactly one completion on main; 2nd timeline rejected | `TestVideoFixture` clip, `PlaybackSurfaceProbe` | accept; reject-2nd; single main-thread completion; luma sawtooth reset proves 2nd segment | full (device) | asserts `deliveryCount==1`, `onMainThread`, `hasLumaReset` |
| ANDT-TL-2 | trim `[A,B)` — no frame before A / at-or-after B | fixture, probe | presented-luma min ≥ A-band, max ≤ B-band (±tol) | full (device) | window [1000,2000)ms → luma band ~[87,168] |
| ANDT-TL-3 | per-segment filter applied only to its segment | fixture, probe, constant-output `VideoFilter` | seg0 frames pinned to filter constant from first frame; seg1 raw low-luma only later | full (device) | constant filter `vec4(0.784,0,0,1)`→luma~200 (opacity=1 full replace, gl_program.cpp:215) |
| ANDT-TL-4 | per-segment speed governs rate | fixture, probe | 2x completion wall-time < 1x − margin | coarse (device) | ratio check, 500 ms margin; not a latency assertion |
| ANDT-TL-5 | advance-at-B; next segment starts at its A with its appearance | fixture, probe, constant filter | seg0 filtered block, then seg1 raw ramp starting at A-band, no sub-A frame | full (device) | seg1 trimmed A=1500ms → starts ~luma 133, reaches tail |
| ANDT-TL-6 | fail-fast: one error carrying failing segment index | fixture + `missingLocalPath` | `onTimelineError(INPUT_OPEN, 1)`, main thread, exactly one terminal, seg2 unreached | full (device) | missing file at index 1 after valid index 0 |
| ANDT-TL-7 | single-clip `play` unchanged (AC-8); shared one-attempt guard | fixture, probe, `RecordingPlaybackListener` | clip completes once on main; timeline rejected during clip; clip rejected during timeline | full (device) | regression-analysis: additive change must not disturb single-clip terminal or guard |

---

## 3. Execution results

`Test-ID/scope | exact command | environment | executed status | result | failure evidence`

| scope | exact command | environment | executed | result | evidence |
|---|---|---|---|---|---|
| UT-SEG | `./gradlew :videolib:testDebugUnitTest --tests "com.cii.videolib.VideoSegmentTest"` | macOS arm64; JDK 17.0.16 (only 17 + 11 installed); env vars set | **not executed** | build fails at **configuration** | `Could not resolve com.chiistudio:plugin:1.0.0 … Dependency requires at least JVM runtime version 21. This build uses a Java 17 JVM.` |
| ANDT-TL-1..7 | `./gradlew :videolib:connectedDebugAndroidTest` (or `assembleDebugAndroidTest` to compile) | same + requires supported EGL/GLES3 ARM device on both ABIs | **not executed** | blocked (same config failure; also device-required) | authored now; device execution owned by IT-DEVICE (CHANGESET §6, MI-3) |

**Blocker (environment, not product/test):** project `:camera` applies `com.chiistudio.plugin`
(`libs.plugins.publish`), whose plugin artifact `com.chiistudio:plugin:1.0.0` requires **JVM 21**;
Gradle configures all projects, so any task (even `:videolib`-only) fails to configure under the
active JDK 17. No JDK 21 is installed (`/usr/libexec/java_home -V` → 17, 11 only). Installing an SDK
exceeds the testing stage's authorized scope (smallest relevant test command only). `:videolib` itself
does not apply the publication plugin.

**Suggested commands once a JDK 21 is available** (point Gradle at it, then run the narrowest task):

- `./gradlew :videolib:testDebugUnitTest --tests "com.cii.videolib.VideoSegmentTest" -Dorg.gradle.java.home=<jdk21>` — UT-SEG (host JVM, no device).
- `./gradlew :videolib:connectedDebugAndroidTest -Dorg.gradle.java.home=<jdk21>` — ANDT-TL on a
  connected supported-ABI device (IT-DEVICE scope; run on `arm64-v8a` **and** `armeabi-v7a`).

Static pre-execution validation performed (source inspection, not command execution): every symbol
referenced by both suites exists with matching signatures (`VideoSegment(path,startMs,endMs,speed,
appearance)`, `hasValidInterval`, `playTimeline(List<VideoSegment>,TimelineListener)`,
`onTimelineCompleted`/`onTimelineError(PlaybackError,Int)`, `PlaybackEventLog`/`PlaybackSurfaceProbe`/
`TestVideoFixture`/`RecordingPlaybackListener`/`TerminalKind`); the constant filter passes
`VideoPreview.validate()` (entry-point regex match, no reserved tokens) and, at opacity 1.0, fully
replaces source color per `gl_program.cpp:215` while default neutral adjustments are all skipped by
their `!=`-neutral guards; fixture luma model `round(frameIndex*255/44)` at 15 fps underpins the
trim/advance luma bands. Compilation itself was **not** run (same JDK-21 config blocker), so
compile-clean is asserted from source inspection only, not from a compiler.

---

## 4. Failed cases and root cause

No test executed, so there is **no product failure and no test-defect failure** observed. The single
failure is an **environment failure**: Gradle configuration aborts because the applied publication
plugin requires JVM 21 and only JDK 17/11 are installed. It is upstream of `:videolib` compilation and
unrelated to the timeline change or the authored tests. Resolution is out of testing-stage scope
(install/point to JDK 21), handed to the environment/integration owner.

---

## 5. Gaps and recommendations

| gap | mapped to | recommendation | owner |
|---|---|---|---|
| UT-SEG + ANDT-TL not executed | all Test-IDs | run the §3 commands under JDK 21; UT-SEG is host-only and should run first as the cheapest gate | integration / IT-BUILD, IT-DEVICE |
| ANDT-TL luma thresholds (`FILTER_LUMA`, trim/advance bands, `LUMA_TOL=25`) are device-tunable | ANDT-TL-2,3,5 | if a specific device's H.264+RGBA-readback color error exceeds tolerance, widen `LUMA_TOL`/bands — assertions are structural (ordering, band membership, sawtooth), not exact pixels | IT-DEVICE |
| ANDT-TL-4 speed proof is a coarse wall-clock ratio | ANDT-TL-4 | acceptable per design (host holds only a timing estimate); keep the wide 500 ms margin to avoid device-jitter flakiness | IT-DEVICE |
| Per-segment **boundary/advance** has no production callback | ANDT-TL-1,5 | advance is proven indirectly via the luma signature (sawtooth reset, filtered→raw transition). A dedicated per-segment progress callback is out of scope (SOLUTION-DESIGN §2 note); no added coverage recommended | — (design) |
| Whole-timeline **loop → segment 0** (D-9) not covered | `runTimeline` loop; `setLooping` | not in the approved Testing Handoff; note as a candidate for a later iteration, not silently added here | backlog (unowned by this ticket) |
| Native concurrency (stop/release/detach mid-timeline join-before-terminal) | P1 cancel-then-join | single-clip lifecycle is already covered by `PlaybackLifecycleInstrumentedTest`; the timeline shares the same worker/claim machinery. IT-DEVICE should spot-check a mid-timeline `stop()`/`detachSurface()` if device time allows | IT-DEVICE (opportunistic) |

No unowned backlog beyond the two explicitly-labelled design/iteration notes above. Every authored
Test-ID maps to a Handoff Test-ID; the added `RecordingTimelineListener` is a test-local fixture, not
production scope.

CONTINUE — tests authored for the full Testing Handoff; execution blocked only by an environment
toolchain gap (JDK 21). Ready for integration-testing (IT-BUILD, IT-APP, IT-DEVICE).
