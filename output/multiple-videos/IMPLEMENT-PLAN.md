AUTOMATION: CONTINUE

# IMPLEMENT-PLAN — multiple-videos

Execution-ready plan for the sequential video timeline. Design source: `SOLUTION-DESIGN.md`
(rev: first line `AUTOMATION: CONTINUE`). Strategy: **extend the existing single-attempt native
playback seam** (`VideoPlayback`) into an ordered-segment engine; add an **additive** Kotlin public
API (`VideoPreview.playTimeline`); upgrade the `app` demo. No public-API break, no vendored-FFmpeg
change, no build/ABI change.

Chain preserved: `FR → SC → AC → Work-ID → Task-ID`.

---

## 1. Planning control

| Field | Value |
|---|---|
| Outcome | **CONTINUE** — DAG valid, no planning blocker |
| Design revision | `SOLUTION-DESIGN.md` (AUTOMATION: CONTINUE) |
| Owning module | `videolib` (capability) + `app` (demo consumer) |
| Investigation | ~6 planning lookups: `videolib.cpp` (JNI), `CMakeLists.txt`, `appearance.h`, `app/strings.xml`, `activity_main.xml`, `videolib` test tree. Graph empty → source inspection. |
| Guidelines applied | native-boundary, ndk-cpp, opengles, ffmpeg (principles only — `videolib` is FFmpeg 7.1 **static**, not `camera`'s 3.2.12 shared) → converted to invariants + IT tasks |

**Assumptions (from design, not re-decided):** ms A/B units, `[A,B)` exclusive B, fail-fast with
segment index, video-only, export out of scope, selection order, loop→segment 0.

**Blockers:** none. **Unresolved planning inputs:** none — every task resolves to an existing
path/symbol or a design-sanctioned bounded new symbol.

**Native-symbol note:** SOLUTION-DESIGN intentionally leaves native/JNI *symbol shapes*
implementation-local (§1 D-2, §6). Tasks name the exact **files** and the **semantic** new
symbols; the precise C++ struct fields and the JNI array-marshalling shape are owned inside their
task, not re-architected by android-dev.

---

## 2. Change-surface inventory

| Change-ID | existing/new | action | exact path | symbol / resource / config key | Design-Ref | responsibility | evidence / design decision | shared/collision key |
|---|---|---|---|---|---|---|---|---|
| C1 | existing | extend | `videolib/src/main/cpp/video_playback.h` | `VideoPlayback`; `PlaybackTerminalCallback` typedef; new segment struct (e.g. `TimelineSegment`) | none — code-driven | Declare ordered-segment playback API + terminal callback carrying a segment index | `video_playback.h:40-48,107-129` single-attempt owner (D-1,D-2) | native-core |
| C2 | existing | extend | `videolib/src/main/cpp/video_playback.cpp` | `VideoPlayback::runAttempt`/`decodeAttempt` + new segment-sequencing; A-skip, B-boundary, per-segment appearance+speed apply, single finish | none — code-driven | Timeline engine: sequence [A,B) segments back-to-back on one worker/clock | decode loop `:646-978` owns clock/seek/skip/loop/speed | native-core |
| C3 | existing | extend | `videolib/src/main/cpp/videolib.cpp` | new `Java_com_cii_videolib_VideoPreview_nativePlayTimeline`; extend `PlaybackJniBridge` to resolve+call timeline callbacks (with index) | none — code-driven | Marshal ordered segment descriptors JNI→native; route one terminal event | JNI hand-mangled 1:1 `:283-459`; bridge `:24-111` fixed sigs | native-jni |
| C4 | new | create | `videolib/src/main/java/com/cii/videolib/VideoSegment.kt` | `VideoSegment` (path, startMs, endMs, speed, appearance) | none — code-driven | Immutable per-segment descriptor composing `VideoAppearance` | design §3 "Segment descriptor" (D-5); reuses `VideoAppearance` | — |
| C5 | new | create | `videolib/src/main/java/com/cii/videolib/TimelineListener.kt` | `TimelineListener.onTimelineCompleted()` / `onTimelineError(error: PlaybackError, segmentIndex: Int)` | none — code-driven | One terminal outcome for the whole timeline | design §5 terminal contract (D-6); reuses `PlaybackError` | — |
| C6 | existing | extend | `videolib/src/main/java/com/cii/videolib/VideoPreview.kt` | `playTimeline(...)`; `external fun nativePlayTimeline(...)`; `@Keep onNativeTimelineCompleted`/`onNativeTimelineError`; per-segment validation reusing `validate()` | none — code-driven | Public additive timeline entry + JNI decls + callback marshalling to main thread | `VideoPreview.kt:62-88` play seam; `:236-259` @Keep callbacks; `:296-343` validate | videopreview |
| C7 | existing | verify | `videolib/src/main/cpp/CMakeLists.txt` | `add_library(videolib SHARED ...)` source list; `abiFilters`; page settings | none — code-driven | Confirm NO new `.cpp`/ABI/page change (engine extends existing sources) | `CMakeLists.txt:30-37` explicit source list | native-build |
| C8 | existing | modify | `app/src/main/java/com/chiistudio/library/MainActivity2.kt` | picker → `OpenMultipleDocuments`; segment assembly; `playTimeline`; timeline callbacks; per-clip controls; cross-segment progress | none — requirement/code-driven | Demo host wiring for multi-video timeline | `MainActivity2.kt:76` single `OpenDocument()`; `:269-321` play path | app-demo |
| C9 | existing | modify | `app/src/main/res/layout/activity_main.xml` | new controls (per-clip trim A/B, filter, speed; segment list/progress) | none — requirement/code-driven | Demo UI surface | current ids `:12-94` | app-demo |
| C10 | existing | modify | `app/src/main/res/values/strings.xml` | new `video_*` status/label strings | none — requirement/code-driven | Demo user-visible text (context §6 keep text in resources) | existing `video_*` keys | app-demo |
| C11 | new | create | `videolib/src/test/java/com/cii/videolib/VideoSegmentTest.kt` | JVM unit test for `VideoSegment` value semantics + pure interval rule | none — code-driven | Fast validation of the pure value type | `src/test` JVM seam (`ExampleUnitTest.kt`) | test-jvm |
| C12 | new | create | `videolib/src/androidTest/java/com/cii/videolib/TimelinePlaybackInstrumentedTest.kt` | Instrumented timeline behavior suite | none — code-driven | Device proof of trim/filter/speed/sequence/terminal | existing `androidTest` suite + `TestVideoFixture`/`PlaybackSurfaceProbe` | test-android |

---

## 3. Work-item backlog

| FR | SC | AC | Work-ID | outcome | module/screen | depends on |
|---|---|---|---|---|---|---|
| FR-1..5 | SC-1..5 | AC-1..8 | **W1** | `videolib` plays an ordered list of trimmed (A→B) segments, each with its own filter+speed, back-to-back on one surface, with one terminal outcome; existing single-clip API unchanged | `videolib` | — |
| FR-1..5 | SC-1..5 | AC-1..7 | **W2** | `app` demo lets a user multi-pick videos, set per-clip A/B/filter/speed, and preview them as one sequential timeline | `app` | W1 |
| FR-1..5 | — | AC-1..8 | **W3** | Cross-module/native closure: native link+package, host compile, device runtime on both ARM ABIs | `videolib`, `app` | W1, W2 |

Delivery-backlog metadata (points/MoSCoW/sprint) not requested → omitted.

---

## 4. Task backlog

Owner stages: **A**=android-dev, **T**=testing, **I**=integration-testing. Design-Ref for every
task: `none — requirement/code-driven` (no design source). Native invariants below are the
guideline checks mapped to task invariants.

| Task-ID | Work | module | stage | objective | Change-IDs / scope | preconditions | invariants | done condition | verification | depends on | collision |
|---|---|---|---|---|---|---|---|---|---|---|---|
| **P1** | W1 | videolib | A | Native segment model + timeline engine: generalize `VideoPlayback` to sequence segments (seek-to-A + skip-until-A, present `[A,B)`, advance at B, apply appearance+speed per segment, one terminal outcome); keep single-clip path as degenerate 1-segment | C1, C2 | — | one worker/one clock; cancel-then-join before renderer release; single terminal claim (`terminalClaimed_`); appearance applied **before** first frame of each segment; re-anchor clock + reset `mediaUs`/speed at each boundary (mirror loop reset `:967-973`); `SwsContext` confined to worker thread; B clamped to playable duration; **no** libavcodec muxing; no `abiFilters`/page change | `video_playback.{h,cpp}` compile; state machine covers §2 states; single-clip behavior byte-for-byte preserved (AC-8) | IT-BUILD (compile) + IT-DEVICE (runtime) | — | native-core |
| **P2** | W1 | videolib | A | JNI timeline export + terminal callback with segment index: add `nativePlayTimeline` marshalling ordered descriptors; extend `PlaybackTerminalCallback`/`PlaybackJniBridge` to route completion and error(+index); segment index `-1` preserves the existing single-clip callbacks | C3 (+C1 typedef) | P1 | JNI export name matches Kotlin `external fun` exactly (hand-mangled, no RegisterNatives); direct-buffer/`GetStringUTFChars` release discipline; `NewGlobalRef`/`DeleteGlobalRef` + attach/detach on worker thread; existing single-clip exports unchanged | new export compiles/links into `libvideolib.so`; bridge resolves both callback method IDs | IT-BUILD + IT-DEVICE | P1 | native-jni, native-core |
| **P3** | W1 | videolib | A | Public value types: `VideoSegment` (path, startMs, endMs, speed, appearance) and `TimelineListener` (completed / error+segmentIndex) | C4, C5 | — | immutable; compose existing `VideoAppearance`/`PlaybackError`; Kotlin style + `com.cii.videolib` namespace | both files compile; public, additive | UT-SEG (JVM) | — | — |
| **P4** | W1 | videolib | A | `VideoPreview.playTimeline(segments, listener)`: add `external fun nativePlayTimeline` + `@Keep onNativeTimelineCompleted`/`onNativeTimelineError`; validate each segment (reuse `validate()` for appearance/filter; add `0 ≤ A < B` + finite; speed ≥ `MIN_PLAYBACK_SPEED`); reject on empty list / no surface; marshal terminal event to main thread via existing handler | C6 | P2, P3 | additive only — `play`/`stop`/`pause`/`resume`/`seekTo`/`setFilter`/`setPlaybackSpeed` untouched; one active attempt; `external fun` signature identical to P2 export; callback names match `@Keep` methods; B-vs-duration clamp deferred to native (Kotlin validates structure only) | `VideoPreview.kt` compiles; `playTimeline` returns accept/reject per §5 contract; single-clip API unchanged (AC-8) | UT-SEG (partial), IT-BUILD, IT-DEVICE, ANDT-TL | P2, P3 | videopreview |
| **P5** | W2 | app | A | Host walking skeleton: `OpenMultipleDocuments` multi-pick, copy each to cache, assemble `List<VideoSegment>` (default full interval/speed/filter), drive `playTimeline`, handle `TimelineListener`, add status strings | C8, C9, C10 | P4 | keep text in `strings.xml`; host owns cache files/permissions/lifecycle; single-owner-thread use of `VideoPreview`; preserve existing single-video controls or replace coherently | app builds; multi-pick → sequential preview of ≥2 clips; one completion (AC-1,6,7 demoable) | IT-APP, manual run | P4 | app-demo |
| **P6** | W2 | app | A | Host per-clip controls: UI to set A/B (trim), filter, and speed per picked clip; cross-segment progress reflecting the active segment | C8, C9, C10 | P5 | same file set as P5 → serialized; view-pixel/ms units; no library-internal knowledge | app builds; user can set A/B/filter/speed per clip and see them applied (AC-2,3,4 demoable) | IT-APP, manual run | P5 | app-demo |
| **UT-SEG** | W1 | videolib | T | JVM unit tests for `VideoSegment` value semantics (construction, equality, immutability) and the pure interval rule (`A<B`, non-negative) | C11 | P3 | pure JVM; no native lib load | test file compiles; asserts value + interval rules | runnable — `:videolib:testDebugUnitTest` | P3 | test-jvm |
| **ANDT-TL** | W1 | videolib | T | Instrumented timeline suite: multi-retain (AC-1), trim `[A,B)` (AC-2,3), per-segment filter (AC-4), per-segment speed (AC-5), advance-next (AC-6), back-to-back + single completion (AC-7), fail-fast with segment index (D-6), single-clip regression (AC-8) | C12 | P4 | reuse `TestVideoFixture`/`PlaybackSurfaceProbe`; attach real surface; device-required | test file compiles; scopes all listed AC/behaviors | authored now; executed device-side by ANDT/IT-DEVICE | P4 | test-android |
| **IT-BUILD** | W3 | videolib | I | `:videolib:assembleDebug` — native compile/link, JNI symbol resolution, soname/packaging, ABI/page settings intact | C1,C2,C3,C7 | P4, ANDT-TL authored | ARM `abiFilters` + 16 KB alignment preserved; no vendored change | build succeeds; `libvideolib.so` present for both ABIs in AAR | runnable | P4 | native-build |
| **IT-APP** | W3 | app | I | `:app:assembleDebug` — host compiles against additive API | C8,C9,C10 | P6 | additive API only; no host contract exported | build succeeds | runnable | P6 | app-demo |
| **IT-DEVICE** | W3 | videolib | I | Device smoke on `arm64-v8a` AND `armeabi-v7a`: native load, trim accuracy, per-segment filter/speed, back-to-back transition continuity, single terminal callback, fail-fast index | C1,C2,C3,C6 | IT-BUILD | run on each supported ABI; runtime proof only (compile ≠ runtime) | timeline plays correctly on both ABIs; behaviors match ACs | device-required | IT-BUILD, ANDT-TL | native-core |

---

## 5. Dependency map (DAG)

```
P1 ──contract──▶ P2 ──contract──▶ P4 ──behavior──▶ P5 ──behavior──▶ P6
P3 ──contract──▶ P4                         │
P1 ──behavior──▶ IT-DEVICE                  │
P3 ──test──────▶ UT-SEG                     │
P4 ──test──────▶ ANDT-TL                    │
P4 ──wiring────▶ IT-BUILD ──behavior──▶ IT-DEVICE
P6 ──wiring────▶ IT-APP
ANDT-TL ──test─▶ IT-DEVICE
```

Edge types: P1→P2 `contract` (native API), P3→P4 `contract` (types), P2→P4 `contract` (JNI
signature), P4→P5→P6 `behavior`, P4→{UT-SEG,ANDT-TL} `test`, P4→IT-BUILD→IT-DEVICE `wiring/behavior`,
P6→IT-APP `wiring`. **Cycle check:** none — strictly forward from P1/P3 to integration.

Serialization keys: `native-core` (P1,P2,IT-DEVICE — shared `video_playback.*`), `videopreview`
(P4), `app-demo` (P5,P6,IT-APP — shared `MainActivity2.kt`+layout+strings).

---

## 6. Execution waves

| Wave | Tasks | Concurrency / serialization | Prerequisites satisfied |
|---|---|---|---|
| 0 | **P1**, **P3** | Concurrent — disjoint files (native `video_playback.*` vs new Kotlin value files) | none |
| 1 | **P2**, **UT-SEG** | Concurrent — P2 native (`videolib.cpp`+header), UT-SEG test authoring; disjoint | P1 (P2), P3 (UT-SEG) |
| 2 | **P4** | Serial — sole `VideoPreview.kt` owner; needs JNI (P2) + types (P3) | P2, P3 |
| 3 | **ANDT-TL**, **P5** | Concurrent — androidTest authoring vs `app` sources; disjoint | P4 |
| 4 | **P6** | Serial after P5 — same `app-demo` file set | P5 |
| 5 | **IT-BUILD** | Serial — videolib build gate | P4, ANDT-TL authored |
| 6 | **IT-APP**, **IT-DEVICE** | Concurrent — IT-APP builds `app`; IT-DEVICE runs videolib on both ABIs | IT-APP←P6; IT-DEVICE←IT-BUILD+ANDT-TL |

---

## 7. Test scope and verification matrix

| Test-ID | AC-ID / risk | level | target component/contract | behavior/transition/error scope | fake/fixture boundary | production Task-ID | depends on | execution expectation |
|---|---|---|---|---|---|---|---|---|
| UT-SEG | AC-1; interval invariant | JVM unit | `VideoSegment` value type | construction, equality, immutability, `A<B` non-negative | none (pure value) | P3, P4 | P3 | **runnable** `:videolib:testDebugUnitTest` |
| ANDT-TL-1 | AC-1, AC-7 | instrumented | `VideoPreview.playTimeline` | ≥2 segments retained, play back-to-back, exactly one completion | `TestVideoFixture`, `PlaybackSurfaceProbe` | P4 | P4 | authored now; **device** at IT-DEVICE |
| ANDT-TL-2 | AC-2, AC-3 | instrumented | trim `[A,B)` | frames before A / at-or-after B not presented; advance at B | fixture | P1, P4 | P4 | authored; **device** |
| ANDT-TL-3 | AC-4 | instrumented | per-segment filter | filter applied only to its segment | fixture | P1, P4 | P4 | authored; **device** |
| ANDT-TL-4 | AC-5 | instrumented | per-segment speed | segment rate governed by its speed | fixture | P1, P4 | P4 | authored; **device** |
| ANDT-TL-5 | AC-6 | instrumented | segment advance | next segment begins at its A with its appearance/speed | fixture | P1, P4 | P4 | authored; **device** |
| ANDT-TL-6 | D-6 (fail-fast) | instrumented | terminal error contract | mid-timeline bad segment → one error carrying failing segment index | fixture (unreadable/short clip) | P2, P4 | P4 | authored; **device** |
| ANDT-TL-7 | AC-8 (regression) | instrumented | single-clip `play` | existing single-video path unchanged | fixture | P1, P4 | P4 | authored; **device** |

### Module integration matrix

| Check-ID | changed module | affected consumer / external contract | boundary | command or device/manual check | why required | owner task |
|---|---|---|---|---|---|---|
| MI-1 | `videolib` | JNI ↔ native; ABI packaging | native/build | `:videolib:assembleDebug` + AAR `.so` inspection (both ABIs) | prove link, JNI symbol resolution, soname, 16 KB/ABI packaging (compile ≠ runtime) | IT-BUILD |
| MI-2 | `app` | `app`→`videolib` additive public API | consumer-wiring | `:app:assembleDebug` | host compiles against new additive API; no break | IT-APP |
| MI-3 | `videolib` | native runtime on supported ABIs | native-lifecycle/runtime | run on `arm64-v8a` **and** `armeabi-v7a` | only device proves trim/filter/speed/transition/single-callback + JNI load | IT-DEVICE |
| MI-4 | `videolib` | unknown external consumers of public API | public-contract | source review: additive-only diff (no removed/changed existing symbols) | public lib, external consumers unknown; additive by construction (D-3) | IT-BUILD (review gate) |

---

## 8. Sprint plan

Not requested; omitted (execution waves in §6 are sufficient).

---

## 9. Shared infrastructure and risk constraints

| Risk (source) | affected IDs | required serialization / verification | owning task |
|---|---|---|---|
| Native "one attempt"→ordered sequence (DEV-SPEC §9 top; SOLUTION §6) | P1, P2, IT-DEVICE | Serialize all `native-core` edits (P1→P2); cancel-then-join + single-terminal-claim + appearance-before-frame as P1 invariants; runtime proof MI-3 | P1 |
| JNI both-sides drift → `UnsatisfiedLinkError` at call time (ndk-cpp) | P2, P4 | Export name/signature must match `external fun` and `@Keep` callback names exactly; proven only by device load MI-3 | P2, P4 |
| Trim seek/PTS accuracy (FFmpeg 7.1 static; ffmpeg principles) | P1 | A via keyframe-backward seek + skip-until-target; B exclusive PTS clamp to duration; device check | P1, IT-DEVICE |
| ABI/16 KB packaging regression | C7, IT-BUILD | P1/P2 must not alter `abiFilters`/page settings; MI-1 build gate both ABIs | IT-BUILD |
| Public-API compatibility (unknown external consumers) | P3, P4, P6 | Additive-only; existing symbols untouched; MI-4 diff review | IT-BUILD |
| Per-segment appearance apply needs attached surface / GL-thread (opengles) | P1, P4 | Appearance applied on render thread with context current; reuse existing `applyAppearance` path; `SURFACE_UNAVAILABLE` reject preserved | P1 |
| Host resource pressure with N cached clips (DEV-SPEC §9) | P5 | Host-side concern; per-clip copy reused; max-count guard left implementation-local (not a library contract) | P5 |

No new design decisions introduced. DAG valid, no blocker → **AUTOMATION: CONTINUE**. Ready for
android-dev starting at Wave 0 (P1, P3).
