AUTOMATION: CONTINUE

# CHANGESET — multiple-videos (Android Dev)

Feature: sequential video timeline in `videolib` (ordered trimmed A→B segments, each with its own
filter + speed, back-to-back on one preview surface, one terminal outcome) + `app` demo host.
Strategy per `IMPLEMENT-PLAN.md`: **extend** the single-attempt native `VideoPlayback` seam into an
ordered-segment engine; **additive** Kotlin API `VideoPreview.playTimeline`. No public-API break, no
vendored-FFmpeg change, no CMake/ABI/16 KB change.

Owning modules: `videolib` (capability), `app` (demo consumer). Guidance loaded: module-impact,
native-boundary, ndk-cpp. Graph unavailable → source inspection throughout (validated against
Gradle/source per ground rules).

---

## 1. Implementation outcome

**Completed** — all android-dev tasks P1–P6 (Waves 0–4). Deviations: none. Scope held to the
approved Change-IDs; no unplanned symbols, dependencies, or refactors.

Native-symbol note (IMPLEMENT-PLAN §1): the C++ struct fields and the JNI array-marshalling shape
were owned inside P1/P2 as designed. Decision recorded in §Task notes below.

Testing (UT-SEG, ANDT-TL) and integration (IT-BUILD, IT-APP, IT-DEVICE) tasks are **not executed
here** — handed off in §5/§6. `video_playback.h` (Change-ID C1) was already committed by the session
auto-commit before this stage resumed; its committed content carries the full C1 contract and is
consistent with the C2 implementation (verified via `git show HEAD:…`).

---

## 2. Actual change manifest

Design-Ref for every row: `none — requirement/code-driven` (no design source; SOLUTION-DESIGN §6
"Design conformance N/A").

| FR | SC | AC | Work | Task | Change-ID | module | consumers/contracts | planned action | actual path | actual symbol / key | diff | purpose | Test/Check | verification |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| FR1-5 | SC1-5 | AC1-8 | W1 | P1 | C1 | videolib | JNI (P2); public via P4 | extend | `videolib/src/main/cpp/video_playback.h` | `PlaybackKind`, `TimelineSegment`, `PlaybackTerminalCallback` (4-arg), `playTimeline`, `runTimeline`, `finishAttempt(+kind,+idx)`, `currentKind_` | committed (HEAD) | ordered-segment API + kind-tagged terminal callback | IT-BUILD, IT-DEVICE | source-inspected; committed content verified |
| FR1-5 | SC1-5 | AC1-8 | W1 | P1 | C2 | videolib | native-core | extend | `videolib/src/main/cpp/video_playback.cpp` | segment window locals; `EndOfSegment` flow; seek-to-A; B-exclusive end; A-skip; `honorLooping` gate; `runAttempt` (single-clip defaults); `runTimeline`; `finishAttempt` body | modified | timeline engine: sequence [A,B) back-to-back, one worker/clock/terminal | IT-BUILD, IT-DEVICE | source-inspected; not compiled (auth required) |
| FR1-5 | SC1-5 | AC1-7 | W1 | P2 | C3 | videolib | native-jni; Kotlin @Keep (P4) | extend | `videolib/src/main/cpp/videolib.cpp` | `Java_com_cii_videolib_VideoPreview_nativePlayTimeline`; `readTimelineSegments`; `PlaybackJniBridge` +`timelineCompletedMethod_`/`timelineErrorMethod_`, kind-routing `notify` | modified | marshal ordered descriptors JNI→native; route one terminal event(+index) | IT-BUILD, IT-DEVICE | JNI 1:1 parity checked (§4); not compiled |
| FR1 | SC1 | AC1 | W1 | P3 | C4 | videolib | public (unknown ext) | create | `videolib/src/main/java/com/cii/videolib/VideoSegment.kt` | `VideoSegment(path,startMs,endMs,speed,appearance)`, `hasValidInterval` | added | immutable per-segment descriptor | UT-SEG | source-inspected |
| FR5 | SC5 | AC6,7 | W1 | P3 | C5 | videolib | public (unknown ext) | create | `videolib/src/main/java/com/cii/videolib/TimelineListener.kt` | `TimelineListener.onTimelineCompleted()`/`onTimelineError(error,segmentIndex)` | added | one terminal outcome for the whole timeline | ANDT-TL | source-inspected |
| FR1-5 | SC1-5 | AC1-8 | W1 | P4 | C6 | videolib | app (P5/P6); unknown ext | extend | `videolib/src/main/java/com/cii/videolib/VideoPreview.kt` | `playTimeline(segments,listener)`; `external fun nativePlayTimeline`; `@Keep onNativeTimelineCompleted`/`onNativeTimelineError`; `TimelineCompleted`/`TimelineError` events; per-segment validation | modified | public additive timeline entry + JNI decls + main-thread marshalling | UT-SEG, ANDT-TL, IT-BUILD | additive-only diff (§4); not compiled |
| — | — | AC1-8 | W1 | P1/P2 | C7 | videolib | native-build | verify | `videolib/src/main/cpp/CMakeLists.txt` | source list, `abiFilters`, page settings | unchanged | confirm no new .cpp/ABI/page change | IT-BUILD (MI-1) | verified unchanged (§4) |
| FR1-5 | SC1-5 | AC1-7 | W2 | P5,P6 | C8 | app | app→videolib additive API | modify | `app/src/main/java/com/chiistudio/library/MainActivity2.kt` | `OpenMultipleDocuments`; per-URI cache copy; `Clip` model; `List<VideoSegment>` assembly; `playTimeline`; `TimelineListener`; per-clip A/B/speed/filter rows; cross-segment progress estimate | modified | demo host for multi-video timeline | IT-APP, manual | source-inspected; not compiled |
| FR1-5 | — | AC1-7 | W2 | P5,P6 | C9 | app | — | modify | `app/src/main/res/layout/activity_main.xml` | NestedScrollView; `clip_list` container; removed brightness control | modified | scrollable timeline demo surface | IT-APP | id/string xrefs resolved (§4) |
| FR1-5 | — | AC2-5 | W2 | P6 | C9 | app | — | create | `app/src/main/res/layout/clip_control_row.xml` | per-clip trim A/B + speed + filter row | added | inflatable per-clip control row (bounded, C9 responsibility) | IT-APP | id xrefs resolved (§4) |
| FR1-5 | — | AC1-7 | W2 | P5,P6 | C10 | app | — | modify | `app/src/main/res/values/strings.xml` | new `video_*`/`pick_videos` timeline+clip strings; removed brightness/single-pick strings | modified | demo user-visible text (keep text in resources) | IT-APP | string xrefs resolved (§4) |

Every planned Change-ID C1–C10 appears as completed (C7 = verified-unchanged with reason). C11/C12
(test files) are testing-owned (UT-SEG, ANDT-TL) → §5 handoff, not implemented here. Every changed
production path maps back to an approved Change-ID/task.

---

## 3. Task completion

| Task | preconditions | invariants checked | done condition | result | evidence |
|---|---|---|---|---|---|
| **P1** | C1 header committed | one worker/one clock; single terminal claim (`terminalClaimed_`); appearance applied before first segment frame (via `decodeAttempt` renderer lock, unchanged path); B clamped to playable duration (reuses `resetDecoder` last-playable clamp); `honorLooping` gates per-segment loop; single-clip defaults `(0,-1,0.0,nullopt,true)` leave every new branch inert | `.cpp` state machine covers §2 states; single-clip path byte-for-byte preserved (AC-8) | done | `runAttempt` calls 7-arg `decodeAttempt` with degenerate defaults; `runTimeline` iterates segments, `honorLooping=false`, one `finishAttempt`; all call sites re-checked via grep |
| **P3** | — | immutable `data class`; composes `VideoAppearance`/`PlaybackError`; `com.cii.videolib` namespace; pure `hasValidInterval` for JVM UT | both files compile-shaped; public, additive | done | files created; interval rule pure (no native load) for UT-SEG |
| **P2** | P1 | JNI export name = Kotlin `external fun` exactly; `GetStringUTFChars`/`ReleaseStringUTFChars` paired; per-element `DeleteLocalRef` in marshalling loop; attach/detach + global ref unchanged; single-clip exports untouched; fail-fast reject (no partial timeline) | new export compiles-shaped; bridge resolves both new method IDs | done | 15↔15 export parity; 4↔4 callback parity (§4); `readTimelineSegments` releases every local ref on all paths |
| **P4** | P2, P3 | additive — `play`/`stop`/`pause`/`resume`/`seekTo`/`setFilter`/`setPlaybackSpeed` untouched; one active attempt (shared `activeAttemptId`/`startPending`); `external fun` signature identical to P2 export; callback names match `@Keep`; B-vs-duration deferred to native; terminal marshalled to main thread via existing `callbackHandler` | `VideoPreview.kt` compile-shaped; accept/reject per §5; single-clip API unchanged | done | validation reuses `validate()` + `0≤A<B` (`hasValidInterval`) + `speed≥MIN_PLAYBACK_SPEED`; `enqueueNativeEvent` `when` exhaustive over 4 event subtypes |
| **P5** | P4 | text in `strings.xml`; host owns cache files/permissions/lifecycle; single-owner-thread `VideoPreview`; `fileExecutor` shut down in `onDestroy` (instance-scoped, no static leak) | app builds-shaped; multi-pick → sequential preview of ≥2 clips; one completion | done | `OpenMultipleDocuments`; per-URI copy on single-thread executor; default full-interval/speed/filter segments; `TimelineListener` wired |
| **P6** | P5 | same `app-demo` file set → serialized; view-px/ms units; no library-internal knowledge; drag commits on `onStopTrackingTouch` (no per-tick decoder thrash); A<B enforced in UI | app builds-shaped; per-clip A/B/filter/speed settable + applied | done | inflated `clip_control_row`; per-clip SeekBars/switch mutate `Clip`, restart timeline on commit; cross-segment active-clip status estimate |

---

## 4. Authorized command results

No build/test/static command was executed. An authorized implementation stage *may* run the smallest
relevant compile, but the environment here has no confirmed NDK/Gradle toolchain invocation
authorization for this session, so all are recorded `not run — authorization required`.

| Check | scope | command | outcome |
|---|---|---|---|
| videolib native+Kotlin compile/link | `videolib` | `./gradlew :videolib:assembleDebug` | not run — authorization required |
| videolib JVM unit tests (UT-SEG) | `videolib` | `./gradlew :videolib:testDebugUnitTest` | not run — authorization required |
| app host compile | `app` | `./gradlew :app:assembleDebug` | not run — authorization required |

Static evidence gathered by source inspection (not command execution):

- **JNI name parity (drift = runtime `UnsatisfiedLinkError`):** 15 Kotlin `external fun native*` ↔ 15
  `Java_com_cii_videolib_VideoPreview_native*` exports — exact 1:1, incl. new `nativePlayTimeline`.
- **Callback parity:** 4 `@Keep onNative*` ↔ 4 `GetMethodID` names/sigs —
  `onNativeTimelineCompleted "(J)V"`, `onNativeTimelineError "(JII)V"` match Kotlin
  `(Long)`/`(Long,Int,Int)`. `nativePlayTimeline` param types map 1:1 (Array<String>→jobjectArray,
  LongArray→jlongArray, DoubleArray→jdoubleArray, Array<FloatArray>/Array<IntArray>/
  Array<Array<ByteArray>>→jobjectArray, IntArray→jintArray, FloatArray→jfloatArray).
- **Additive-only (MI-4 review gate):** no existing public symbol removed/changed in `VideoPreview.kt`;
  single-clip `onNativePlayback*` callbacks and events untouched; `PlaybackListener`/`PlaybackError`
  unchanged.
- **C7 / ABI (MI-1):** `CMakeLists.txt` source list (`videolib.cpp`, `video_playback.cpp`,
  `preview_renderer.cpp`, `gl_program.cpp`) unchanged — no new `.cpp`; `abiFilters` = `arm64-v8a`,
  `armeabi-v7a`; 16 KB page settings intact.
- **app→videolib dep:** `app/build.gradle.kts:55 implementation(project(":videolib"))` present →
  `com.cii.videolib.*` imports resolve. (Registry topology listed only `:network`; corrected against
  source per ground rules.)
- **Resource xrefs:** every `R.string.*`/`R.id.*` in `MainActivity2.kt` + both layouts resolves to a
  definition; removed dead `video_status_playing`; no dangling references.

UI design conformance evidence: `Design-Ref: none — requirement/code-driven` for all UI tasks; no
Figma/style capture exists, so no cached digest/screenshot comparison applies (SOLUTION-DESIGN §6).

---

## 5. Testing Handoff

Preserves the approved test contract (IMPLEMENT-PLAN §4, §7). Change-IDs C11/C12 are authored by
testing, not android-dev.

| testing Task | Work | AC/risk | Test-ID | level | target contract | behavior/error scope | fake/fixture boundary | changed paths/symbols | depends on | execution expectation |
|---|---|---|---|---|---|---|---|---|---|---|
| UT-SEG | W1 | AC-1; interval invariant | UT-SEG | JVM unit | `VideoSegment` value type | construction, equality, immutability, `hasValidInterval` (`0≤A<B`) | none (pure value) | `VideoSegment.kt` | P3 | **runnable** `:videolib:testDebugUnitTest` |
| ANDT-TL | W1 | AC-1,7 | ANDT-TL-1 | instrumented | `VideoPreview.playTimeline` | ≥2 segments retained, back-to-back, exactly one completion | `TestVideoFixture`, `PlaybackSurfaceProbe` | `VideoPreview.kt`, `video_playback.cpp` | P4 | authored by testing; device at IT-DEVICE |
| ANDT-TL | W1 | AC-2,3 | ANDT-TL-2 | instrumented | trim `[A,B)` | frames before A / at-or-after B not presented; advance at B | fixture | `video_playback.cpp` (seek-to-A, B-exclusive, A-skip) | P4 | authored; device |
| ANDT-TL | W1 | AC-4 | ANDT-TL-3 | instrumented | per-segment filter | filter applied only to its segment | fixture | `video_playback.cpp` (`decodeAttempt` appearance) | P4 | authored; device |
| ANDT-TL | W1 | AC-5 | ANDT-TL-4 | instrumented | per-segment speed | segment rate governed by its speed | fixture | `video_playback.cpp` (speed apply) | P4 | authored; device |
| ANDT-TL | W1 | AC-6 | ANDT-TL-5 | instrumented | segment advance | next segment begins at its A with its appearance/speed | fixture | `video_playback.cpp` (`runTimeline`) | P4 | authored; device |
| ANDT-TL | W1 | D-6 fail-fast | ANDT-TL-6 | instrumented | terminal error contract | mid-timeline bad segment → one `onTimelineError` carrying failing index | fixture (unreadable/short clip) | `videolib.cpp` (index route), `runTimeline` | P4 | authored; device |
| ANDT-TL | W1 | AC-8 regression | ANDT-TL-7 | instrumented | single-clip `play` | existing single-video path unchanged | fixture | `video_playback.cpp` (`runAttempt` defaults) | P4 | authored; device |

---

## 6. Integration Handoff

Preserves every planned Check-ID (IMPLEMENT-PLAN §7 module integration matrix). Guarded flow →
handoff to testing/integration on CONTINUE.

| integration Task | Check-ID | changed module | affected consumer/external contract | boundary | exact command / device check | required environment | blocking policy |
|---|---|---|---|---|---|---|---|
| IT-BUILD | MI-1 | videolib | JNI↔native; ABI packaging | native/build | `./gradlew :videolib:assembleDebug` + AAR `.so` inspection (both ABIs) | NDK 29.x, CMake 3.22.1 | blocking — link/JNI symbol/soname/16 KB must pass |
| IT-APP | MI-2 | app | app→videolib additive API | consumer-wiring | `./gradlew :app:assembleDebug` | AGP 8.9.1 toolchain | blocking — host must compile against additive API |
| IT-DEVICE | MI-3 | videolib | native runtime on supported ABIs | native-lifecycle/runtime | run on `arm64-v8a` **and** `armeabi-v7a`: trim accuracy, per-segment filter/speed, back-to-back continuity, single terminal callback, fail-fast index, single-clip regression | 2 supported devices/emulators | blocking — only device proves runtime (compile ≠ runtime) |
| IT-BUILD | MI-4 | videolib | unknown external consumers of public API | public-contract | source review: additive-only diff (no removed/changed existing symbol) | — | blocking — review gate (done here §4; re-confirm on final diff) |

---

## Task notes (decisions owned within tasks)

- **P1 B-boundary:** ends a segment before the first frame with `mediaUs ≥ endMs` (`[A,B)` exclusive);
  `endMs < 0` = natural EOF. New `EndOfSegment` decode-flow returns segment success (→ next segment)
  at both `receiveFrames` call sites.
- **P1 A-boundary:** `av_seek_frame(AVSEEK_FLAG_BACKWARD)` to A (reusing `resetDecoder`) + skip frames
  with `mediaUs < segmentStartUs`. Single-clip (`startMs 0`) never seeks/skips — path preserved.
- **P2 marshalling shape (design left impl-local):** parallel per-segment arrays; per-segment
  appearance read through the existing `readAppearance` helper so filter/adjustment validation is
  identical to single-clip apply. Any structural/appearance error rejects the whole request before an
  attempt starts (fail-fast, no partial timeline).
- **P5/P6 host:** the previous global brightness control is coherently **replaced** by per-clip filter
  toggles; the progress bar is a read-only whole-timeline wall-clock estimate (cross-segment scrubbing
  is unspecified by design). A clip edit restarts the timeline from segment 0.

CONTINUE — android-dev complete; ready for testing (UT-SEG, ANDT-TL) and integration
(IT-BUILD, IT-APP, IT-DEVICE).
