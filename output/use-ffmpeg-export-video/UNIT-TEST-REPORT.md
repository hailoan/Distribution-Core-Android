AUTOMATION: CONTINUE

# UNIT-TEST-REPORT — use-ffmpeg-export-video

Test coverage for the FFmpeg + MediaCodec export feature in `:videolib`, against the CHANGESET
Testing Handoff. Scope: `:videolib` test roots only.

> **Addendum (post-integration fix).** After the integration stage replaced the shared-enum extension
> with a separate `ExportError` type (see CHANGESET addendum), the JVM enum test was updated
> accordingly: `PlaybackErrorTest` → **`ExportErrorTest`** (pins the 7-value `ExportError` ordinal
> contract the native mapping depends on, plus a guard that `PlaybackError` stays the original 4).
> The instrumented tests' error assertions/listener signatures now use `ExportError`. Re-executed
> JVM suite: **PASS — 13 tests, 0 failures** (ExportErrorTest 4, VideoSegmentTest 8, ExampleUnitTest
> 1); instrumented suite recompiles clean. §1/§3 below describe the pre-fix authoring; the Test-ID
> coverage is unchanged (same behaviors, `ExportError` instead of the extended `PlaybackError`).

## 0. Handoff reconciliation & one reclassification

The CHANGESET Testing Handoff named 7 Test-IDs. One planned classification is corrected here with
evidence, per the testing contract (report ownership/trace issues rather than silently comply):

- **T-VAL was planned as a host-JVM unit test ("pure Kotlin, no native load").** This is **infeasible**:
  `VideoExporter` calls `System.loadLibrary("videolib")` in its companion initializer and
  `nativeCreate()` + `Handler(Looper.getMainLooper())` in its constructor (`VideoExporter.kt:28,344`),
  so it cannot be instantiated on the host JVM. The equivalent `VideoPreview` validation is likewise
  covered only by instrumented tests. **T-VAL is therefore authored as an instrumented test**
  (`VideoExporterValidationTest`), asserting the same pre-native rejection contract.
- **Added-by-testing (regression):** the genuinely JVM-runnable slice of C3 — the `PlaybackError` enum
  extension whose ordinal/name contract the native `ExportErrorCode`→`PlaybackError` mapping depends on
  — is covered by a new **executable** JVM test `PlaybackErrorTest` (evidence: `regression-analysis`
  on the additive public-enum change, risk Risk-5 in the plan). Mapped to C3 / AC-7.

No other scope was added. All 7 handoff Test-IDs are implemented; execution honesty is in §3.

## 1. Test implementation

| FR | SC | AC | Work | prod Task | testing Task | changed prod path/symbol | Test-ID | test path/symbol | level | authored |
|---|---|---|---|---|---|---|---|---|---|---|
| FR-1 | — | AC-7 | W1 | T2 | added-by-testing | `PlaybackError.kt` (C3, +ENCODE/MUX/OUTPUT) | UT-ERR | `src/test/.../PlaybackErrorTest.kt` | JVM unit | ✅ authored |
| FR-1..6 | SC-1..3 | AC-7 | W1 | T9 | T-DEV | `VideoExporter.kt` (validation) | T-VAL | `src/androidTest/.../VideoExporterValidationTest.kt` | instrumented | ✅ authored |
| FR-1,6 | SC-1,3 | AC-1,5 | W1 | T7,T9 | T-DEV | `video_export.*`,`frame_source.*`,`offscreen_renderer.*`,`h264_encoder.*`,`mp4_muxer.*` | T-EXPORT-VIDEO | `.../VideoExportInstrumentedTest.singleClipExport_*` | instrumented | ✅ authored |
| FR-2 | SC-2 | AC-2 | W2 | T3,T7 | T-DEV | `frame_source.cpp` (PTS speed scale) | T-EXPORT-VIDEO | `.../exportSpeed_governsExportedRate` | instrumented | ✅ authored |
| FR-1 | — | AC-8 | W1 | T7 | T-DEV | `video_export.cpp` (cancel/unlink) | T-CANCEL | `.../cancelDuringExport_*` | instrumented | ✅ authored |
| FR-1 | — | AC-7 | W1 | T7 | T-DEV | `frame_source.cpp`,`video_export.cpp` | T-ERR | `.../unreadableSource_failsWithTypedError_*` | instrumented | ✅ authored |
| FR-7 | SC-4 | AC-6 | W3 | T10,T11 | T-DEV | `audio_transcoder.*`,`mp4_muxer.*` | T-AUDIO | `.../exportWithAudio_*`, `.../exportWithAudioRequested_onVideoOnlySource_*` | instrumented | ✅ authored |
| FR-1 | SC-2 | AC-9 | W4 | T12 | T-DEV | `video_export.cpp`,`videolib.cpp`,`VideoExporter.kt` | T-TIMELINE | `.../timelineExport_concatenatesSegmentsIntoOneFile`, `.../timelineExport_failsFastWithFailingSegmentIndex` | instrumented | ✅ authored |
| — | — | Risk-3 (build) | build | T1,T8 | T-DEV | `CMakeLists.txt`,`videolib.cpp` | T-LINK | `.../exporterConstruction_loadsNativeLibrary` | instrumented | ✅ authored |

Supporting test fixture (test source only): `TestVideoFixture.generateProgressiveVideoWithAudio()`
added for T-AUDIO (remuxes the existing deterministic video clip + AAC silence into one MP4; no new
encode path).

## 2. Coverage matrix

| Test-ID | behavior / error / risk | fixture boundary | assertion scope | coverage | evidence |
|---|---|---|---|---|---|
| UT-ERR | `PlaybackError` additive taxonomy (C3) | none (pure enum) | 4 original ordinals unchanged; ENCODE/MUX/OUTPUT appended at 4/5/6; exact member set; `valueOf` by name | full (JVM) | executed pass (§3) |
| T-VAL | pre-native rejection (AC-7): blank/absent path, bad speed (sub-0.1, NaN), out-of-range/inverted/malformed appearance & filter, empty timeline, malformed interval, released exporter | none (no real export; `FailIfCalledListener`) | returns `false` AND never notifies listener | full (device) | authored; device-pending |
| T-EXPORT-VIDEO | single-clip E2E (AC-1,5) + speed (AC-2) | `TestVideoFixture` video clip; container read via platform `MediaExtractor` | playable MP4, H.264 track, 1 terminal on main; 2x duration < 1x | behavioral (device) | authored; device-pending |
| T-CANCEL | cancel mid-export (AC-8) | video clip | no completion delivered; partial file deleted | behavioral (device) | authored; device-pending |
| T-ERR | unreadable source (AC-7) | missing local path | typed `INPUT_OPEN`; no output file | full (device) | authored; device-pending |
| T-AUDIO | source audio (AC-6) + no-audio fallback (A-2) | video+audio fixture; video-only fixture | output has AAC+H.264; video-only source → no phantom audio track | behavioral (device) | authored; device-pending |
| T-TIMELINE | concat + fail-fast (AC-9) | 2× trimmed segments; missing-segment list | one MP4, duration > one segment; fail-fast with segment index 1; no partial file | behavioral (device) | authored; device-pending |
| T-LINK | native load / JNI resolve (Risk-3) | none | `VideoExporter()` construction + a JNI call do not throw `UnsatisfiedLinkError` | smoke (device) | authored; device-pending |

## 3. Execution results

| scope | exact command | environment | executed | result | evidence |
|---|---|---|---|---|---|
| UT-ERR (JVM) | `./gradlew :videolib:testDebugUnitTest --tests com.cii.videolib.PlaybackErrorTest` | JBR 21 (Android Studio), host JVM | ✅ executed | **PASS** — 4 tests, 0 failures, 0 errors | `build/test-results/.../PlaybackErrorTest.xml` (tests="4" failures="0") |
| Full JVM suite (regression) | `./gradlew :videolib:testDebugUnitTest` | JBR 21, host JVM | ✅ executed | **PASS** — ExampleUnitTest 1, PlaybackErrorTest 4, VideoSegmentTest 8 = 13, 0 failures | test-results XMLs |
| Instrumented compile | `./gradlew :videolib:compileDebugAndroidTestKotlin` | JBR 21 + NDK 29 | ✅ executed | **BUILD SUCCESSFUL** — T-VAL + export suite compile | task log |
| T-VAL, T-EXPORT-VIDEO, T-CANCEL, T-ERR, T-AUDIO, T-TIMELINE, T-LINK | `./gradlew :videolib:connectedDebugAndroidTest` | **requires** supported EGL/GLES 3.0 ARM device | ❌ not executed | — | see §4 blocker |

**Environment note:** the system `JAVA_HOME` (JDK 17) cannot configure the project (`:camera` →
published `com.chiistudio:plugin:1.0.0` requires JVM 21). The Android Studio bundled JBR 21 was used
for all runs. Pre-existing constraint, unrelated to this change.

## 4. Failed cases & blockers

- **No failed cases.** All executed tests passed.
- **Instrumented suite not executed — environment blocker (not a product or test defect):** the export
  pipeline performs real EGL init, MediaCodec H.264 encoding, and FFmpeg muxing, and the fixtures use
  the platform encoder — all device-only. No connected ARM device/emulator is available in this
  environment. The tests are authored and compile; they are runnable at integration-testing.
  - Suggested command: `./gradlew :videolib:connectedDebugAndroidTest` on a supported arm64-v8a or
    armeabi-v7a device (a hardware H.264 MediaCodec encoder must be present).

## 5. Gaps & recommendations

| Gap | mapped to | recommendation |
|---|---|---|
| Native pipeline runtime behavior (encode/mux/GL/audio-sync) is unverified by execution here. | T-EXPORT-VIDEO, T-AUDIO, T-TIMELINE, T-CANCEL / AC-1..6,8,9 | Run `:videolib:connectedDebugAndroidTest` at integration-testing (Check-3) on a device. |
| A/V sync **tightness** under speed (Risk-2) is asserted only as "audio track present + duration reflects speed", not sample-accurate drift. | AC-6 / Risk-2 | If drift regressions appear, add a device test comparing audio vs video end-PTS within a tolerance; deferred (no evidence of a current defect, and precise drift is device-codec dependent). |
| SPS/PPS→avcC extradata correctness (Risk-1) is verified indirectly (MP4 is decodable/`MediaExtractor` reports `video/avc`), not by parsing the avcC box. | AC-5 / Risk-1 | Indirect coverage is sufficient (a bad avcC fails `MediaExtractor`/playback); no dedicated box-parse test added — would test FFmpeg internals, not our contract. |
| Effect *pixel* parity (exported frame == preview frame for the same params) is asserted structurally (filter/adjust applied, playable H.264), not by luma-signature like the preview suite. | SC-2 / AC-3,4 | Optional future enhancement: mirror the preview suite's centre-luma-ramp assertion by decoding an exported constant-filter clip; deferred as behavioral-plus, not required for the AC. |

All gaps map to an owned Test-ID/AC/risk; no unowned backlog created. Production behavior was not
modified by this stage (only test sources + the `TestVideoFixture` helper were added/edited).

Handoff to integration-testing on `AUTOMATION: CONTINUE`. Distribution/signing/publish remain
separately authorized and were not performed.
