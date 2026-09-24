AUTOMATION: CONTINUE

# IMPLEMENT-PLAN — use-ffmpeg-export-video

Execution-ready plan to add FFmpeg + MediaCodec **video export** to `:videolib`, derived from
`SOLUTION-DESIGN.md` (rev: first line `AUTOMATION: CONTINUE`). Planning only — no code.

---

## 1. Planning control

| Field | Value |
|---|---|
| Source design | `SOLUTION-DESIGN.md` (AC-1..AC-9; D-1..D-10) |
| Outcome | **CONTINUE** — valid DAG, no planning blocker |
| Owning module | `videolib` only (FR-5); no other module edited |
| New NDK link | `mediandk` (H.264 encode); FFmpeg archives + EGL/GLESv3 already linked |
| Investigation ledger | Change-surface anchored to confirmed files: `VideoPreview.kt`, `videolib.cpp`, `CMakeLists.txt`, `gl_program.{h,cpp}`, `appearance.h`, `preview_renderer.cpp`, `video_playback.{h,cpp}`, camera `video_encoder.h` (reference), existing test scaffolds (`VideoSegmentTest.kt`, `FfmpegLinkageTest.kt`, `TestVideoFixture.kt`). |

**Assumptions carried from design (non-blocking):**
- A-1: Timeline export (AC-9) is **in scope** for this plan (D-9); delivered as the last increment (W4) so the single-clip slice ships first.
- A-2: When the source has **no audio track**, export produces a **video-only** MP4 (not an error) — invariant on the audio wiring task.
- A-3: Encoder targets **source dimensions (even-adjusted) + source fps**; bitrate = resolution-derived heuristic (implementation-local, D-7).
- A-4: Export error taxonomy **extends `PlaybackError`** (D-10) with `ENCODE`, `MUX`, `OUTPUT`; native export codes live in a **new** `video_export.h` enum (not in the shared `video_playback.h`) to avoid touching the preview error contract.

**Unresolved planning inputs:** none blocking. A-1/A-2/A-3 are confirmable at review; none change the DAG.

---

## 2. Change-surface inventory

| Change-ID | existing/new | action | exact path | symbol / key | Design-Ref | responsibility | evidence / decision | shared/collision key |
|---|---|---|---|---|---|---|---|---|
| C1 | new | create | `videolib/src/main/java/com/cii/videolib/VideoExporter.kt` | `VideoExporter` | none — code-driven | Public export facade: accept request, validate, own handle/lifecycle, marshal terminal callback | D-1 | — |
| C2 | new | create | `videolib/src/main/java/com/cii/videolib/ExportListener.kt` | `ExportListener` | none — code-driven | Terminal export callbacks (completed/error[, segmentIndex]) | D-10; mirrors `PlaybackListener.kt` | — |
| C3 | existing | extend | `videolib/src/main/java/com/cii/videolib/PlaybackError.kt` | `PlaybackError` (+`ENCODE`,`MUX`,`OUTPUT`) | — | Reuse taxonomy for export failures | D-10, §5 | `K-PLAYBACKERROR` (public enum; see §9) |
| C4 | new | create | `videolib/src/main/cpp/frame_source.h` / `.cpp` | `FrameSource` | — | Demux + decode video → RGBA, trim `[startMs,endMs)`, **PTS passthrough** (no wall-clock) | D-2, E-1; modeled on `decodeAttempt`, standalone (preview path untouched) | — |
| C5 | new | create | `videolib/src/main/cpp/offscreen_renderer.h` / `.cpp` | `OffscreenRenderer` | — | Offscreen EGL (pbuffer) + reuse `GlProgram` effect + `glReadPixels` filtered RGBA | D-2, D-4, E-3 | — |
| C6 | new | create | `videolib/src/main/cpp/h264_encoder.h` / `.cpp` | `H264Encoder` | — | RGBA→NV12 (`sws_scale`), MediaCodec **byte-buffer** H.264, drain access units + capture SPS/PPS | D-3, E-6, E-8 | — |
| C7 | new | create | `videolib/src/main/cpp/mp4_muxer.h` / `.cpp` | `Mp4Muxer` | — | `avformat` MP4: create H.264(+AAC) streams w/ extradata, rescale PTS/DTS, interleave, trailer, delete-on-fail | D-5, E-5 | — |
| C8 | new | create | `videolib/src/main/cpp/audio_transcoder.h` / `.cpp` | `AudioTranscoder` | — | Decode source audio → `atempo`(speed) → AAC encode → packets to muxer | D-6, E-5 | — |
| C9 | new | create | `videolib/src/main/cpp/video_export.h` / `.cpp` | `VideoExport`, `ExportErrorCode` | — | Session owner: orchestrate frame_source→offscreen→encoder→muxer(+audio); cancel/stop-before-release; one terminal event | D-1..D-6 | `N-VIDEOEXPORT` |
| C10 | existing | extend | `videolib/src/main/cpp/videolib.cpp` | `ExportJniBridge`, `Java_com_cii_videolib_VideoExporter_native*` | — | New hand-mangled JNI exports + terminal-callback bridge (shares cached `gJvm`) | E-7, ndk-cpp-guideline | `N-JNI` (file shared w/ preview JNI) |
| C11 | existing | extend | `videolib/src/main/cpp/CMakeLists.txt` | source list + `target_link_libraries` (+`mediandk`) | — | Register new `.cpp` in the one `libvideolib.so` target; link `mediandk` | E-7, gradle/ndk guidelines | `N-CMAKE` |
| C12 | new | create | `videolib/src/test/java/com/cii/videolib/VideoExporterTest.kt` | `VideoExporterTest` | — | JVM validation/rejection coverage | mirrors `VideoSegmentTest.kt` | — |
| C13 | new | create | `videolib/src/androidTest/java/com/cii/videolib/VideoExportInstrumentedTest.kt` | `VideoExportInstrumentedTest` | — | Device export coverage (video/audio/timeline/cancel/error) | mirrors `TimelinePlaybackInstrumentedTest.kt`, uses `TestVideoFixture` | — |

Reused **unchanged** (referenced, not edited): `gl_program.{h,cpp}` (`GlProgram`), `appearance.h`
(`AppearanceSnapshot` + native `validateAppearance` via `readAppearance`), `render_thread_executor.h`
(`RenderThreadExecutor`), and the FFmpeg decode/`sws_scale` primitives. The camera
`video_encoder.h` is a **pattern reference only — not linked** (FR-5).

---

## 3. Work-item backlog

| FR | SC | AC | Work-ID | outcome | module | depends on |
|---|---|---|---|---|---|---|
| FR-1,6 | SC-1,3 | AC-1,5,7,8 | **W1** | Build/link enablement + single-clip **video-only** export produces a playable H.264/MP4; cancel + error paths correct | videolib | — |
| FR-2,3,4 | SC-2 | AC-2,3,4 | **W2** | Exported frames carry the same speed (PTS scale) + adjustments + filter as preview | videolib | W1 |
| FR-7 | SC-4 | AC-6 | **W3** | Source audio decoded, speed-re-timed, AAC-encoded, muxed in sync | videolib | W1, W2 |
| FR-1 | SC-2 | AC-9 | **W4** | Ordered `VideoSegment` timeline exported as one concatenated MP4 | videolib | W2, W3 |

> W1 and W2 share the same core pipeline files; W2 is the effect-parity guarantee inside the pipeline
> W1 stands up. They are ordered but their tasks overlap by file (see waves).

---

## 4. Task backlog

Owner stage legend: **AD** = android-dev, **T** = testing, **IT** = integration-testing.

| Task-ID | Work | Module | Owner | Objective | Change-IDs / exact scope | Design-Ref | Preconditions/inputs | Invariants | Done condition | Verification | Depends on | Collision key |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| **T1** | W1 | videolib | AD | Enable build: add `mediandk` link + register new native units; create compile-ready skeletons (declarations only, no logic) for C4–C9 | C11; skeleton of C4,C5,C6,C7,C8,C9 | none | — | Keep ARM `abiFilters`, `CMAKE_ANDROID_PAGE_SIZE 16384`, `-Bsymbolic` (arm64), C++17, one shared `libvideolib.so`; do not touch FFmpeg IMPORTED targets | `:videolib:assembleDebug` links; new units compile | Check-1 | — | N-CMAKE, N-VIDEOEXPORT, all new native files (created here) |
| **T2** | W1 | videolib | AD | Extend `PlaybackError` with `ENCODE`,`MUX`,`OUTPUT`; define native `ExportErrorCode` in `video_export.h` | C3; C9 (`ExportErrorCode`) | none | — | Additive enum values only; native codes distinct from `PlaybackErrorCode`; no edit to `video_playback.h` | Enum extended; native codes declared | Source-only (compiles) | T1 | K-PLAYBACKERROR, N-VIDEOEXPORT |
| **T3** | W1/W2 | videolib | AD | Implement `FrameSource`: demux+decode video→RGBA, honor trim window + PTS passthrough (no wall-clock wait); FFmpeg interrupt-based cancel | C4 | — | T1 | Reuse `sws_scale` RGBA path; **must not** import the preview wall-clock scheduler; even-dim awareness deferred to encoder | Decodes a fixture frame to RGBA with correct PTS in a harness | via T-EXPORT-VIDEO (device) | T1 | — |
| **T4** | W1/W2 | videolib | AD | Implement `OffscreenRenderer`: offscreen EGL (pbuffer) on `RenderThreadExecutor`, reuse `GlProgram` to apply `AppearanceSnapshot`, `glReadPixels` filtered RGBA | C5 | — | T1 | All GL/EGL on one render thread with context current; GL resource create/destroy while current; no window surface | Applies adjustments+filter to an RGBA frame and reads it back in a harness | via T-EXPORT-VIDEO (device) | T1 | — |
| **T5** | W1 | videolib | AD | Implement `H264Encoder`: MediaCodec H.264 **byte-buffer** input, RGBA→NV12 (`sws_scale`), drain access units, capture codec-config SPS/PPS | C6 | — | T1 | **No** `AMediaCodec_createInputSurface` (API-26); byte-buffer path only (minSdk 21, E-8); even dimensions `& ~1`; monotonic PTS | Encodes NV12 frames to an H.264 elementary stream + exposes SPS/PPS in a harness | via T-EXPORT-VIDEO (device) | T1 | — |
| **T6** | W1 | videolib | AD | Implement `Mp4Muxer`: `avformat` MP4 output, create H.264 stream with SPS/PPS extradata, rescale PTS/DTS, interleave, write header/trailer, delete partial on failure/cancel | C7 | — | T1, T5 (SPS/PPS shape) | Trailer written before success; partial file removed on any failure/cancel (AC-7,8); no libavcodec H.264 encode (E-5) | Muxes a supplied H.264 stream into a playable MP4 in a harness | via T-EXPORT-VIDEO (device); Risk-1 guard | T1, T5 | — |
| **T7** | W1/W2 | videolib | AD | Implement `VideoExport` session (single-clip, video-only): orchestrate FrameSource→OffscreenRenderer→H264Encoder→Mp4Muxer; state machine (Idle→Preparing→Exporting→Completed/Failed/Cancelled); cancel + stop-before-release; exactly one terminal event | C9 | — | T3,T4,T5,T6 | Effect parity (AC-2 PTS-speed, AC-3 adjustments, AC-4 filter); stop-before-release order across decode→GL→codec→muxer; one terminal event; partial-file delete | End-to-end single-clip video-only export succeeds in a harness | T-EXPORT-VIDEO, T-CANCEL, T-ERR (device) | T3,T4,T5,T6 | N-VIDEOEXPORT |
| **T8** | W1 | videolib | AD | Add export JNI: `ExportJniBridge` + `Java_com_cii_videolib_VideoExporter_nativeCreate/StartExport/CancelExport/Destroy`; marshal terminal callback (worker thread → Kotlin); map `ExportErrorCode`→Kotlin int | C10 | — | T7, T2 | Hand-mangled names in exact sync with Kotlin `external fun`s; reuse cached `gJvm`; global-ref + attach/detach discipline; clear JNI exceptions | JNI exports resolve; native session reachable from Kotlin | via T-EXPORT-VIDEO (device link) | T7, T2 | N-JNI |
| **T9** | W1 | videolib | AD | Implement `VideoExporter.kt` + `ExportListener.kt`: accept source+appearance+speed+output path+listener; Kotlin structural validation (blank path, writable output, speed≥0.1) mirroring `playTimeline`; single active export; main-thread callback marshalling; map native codes→`PlaybackError` | C1, C2 | none | T8, T2 | One active export per instance; reject invalid params synchronously without firing listener; rely on **native** appearance validation (no edit to `VideoPreview.validate`) | Public API compiles; validation rejects bad params | T-VAL (JVM), T-EXPORT-VIDEO (device) | T8, T2 | — |
| **T10** | W3 | videolib | AD | Implement `AudioTranscoder`: decode source audio → `atempo`(speed, chained) → AAC encode → AAC packets; no-audio source → inert (video-only) | C8 | — | T1 | Audio PTS timeline consistent with video speed (A/V sync); A-2 video-only fallback when no audio stream | Produces AAC packets from a fixture in a harness | via T-AUDIO (device) | T1 | — |
| **T11** | W3 | videolib | AD | Wire audio into the session + muxer: add AAC stream to `Mp4Muxer`, interleave audio/video by PTS; feed `AudioTranscoder` from `VideoExport` | C7 (extend), C9 (extend) | — | T7, T10 | Interleave by PTS; A/V sync under speed (AC-6); video-only when source has no audio (A-2) | Export produces MP4 with in-sync audio track in a harness | T-AUDIO (device) | T7, T10 | N-VIDEOEXPORT, C7 (Mp4Muxer) |
| **T12** | W4 | videolib | AD | Timeline export: sequence ordered `VideoSegment`s into one output (continuous video PTS across boundaries, per-segment speed/appearance/filter, concatenated audio); add JNI `nativeStartExportTimeline` + `VideoExporter` timeline entry; fail-fast with segment index | C9 (extend), C10 (extend), C1 (extend) | none | T9, T11 | Segment order preserved; any segment failure → terminal error with segment index (AC-9); reuse `runTimeline` orchestration shape | Timeline export produces one concatenated MP4 in a harness | T-TIMELINE (device) | T9, T11 | N-VIDEOEXPORT, N-JNI, C1 |
| **T-VAL** | W1 | videolib | T | JVM unit test: `VideoExporter` param validation/rejection | C12 | — | T9 | Host-JVM only; no native load | Authored + runs on `:videolib:testDebugUnitTest` | see §7 | T9 | — |
| **T-DEV** | W1–W4 | videolib | T | Instrumented tests: video-only, cancel/cleanup, invalid-source error, audio+sync, timeline concat | C13 | — | T9 (video), T11 (audio), T12 (timeline) | Device/ABI; uses `TestVideoFixture` | Authored; device-run in testing stage | see §7 | T9, T11, T12 | — |
| **T-IT** | W1–W4 | videolib | IT | Execute module verification: `:videolib:assembleDebug` (link/ABI/16 KB) + supported-device export run | — | — | T1 (link), T9/T11/T12 (runtime) | Distinguish compile-proof vs device-proof | Checks recorded | §7 matrix | T1, T9, T11, T12 | — |

---

## 5. Dependency map (DAG)

Typed edges (`contract`, `behavior`, `wiring`, `test`, `ownership`):

```
T1 ──contract──▶ T2,T3,T4,T5,T6,T10        (build scaffold + link before any logic)
T5 ──contract──▶ T6                          (SPS/PPS shape feeds muxer extradata)
T3,T4,T5,T6 ──behavior──▶ T7                 (pipeline stages before orchestrator)
T7 ──contract──▶ T8                          (session API before JNI)
T2 ──contract──▶ T8, T9                      (error codes before mapping)
T8 ──wiring──▶ T9                            (JNI before Kotlin facade)
T7,T10 ──behavior──▶ T11                     (session + audio stage before audio wiring)
T9,T11 ──behavior──▶ T12                     (single-clip + audio before timeline)
T9 ──test──▶ T-VAL ; T9/T11/T12 ──test──▶ T-DEV ; T1/T9/T11/T12 ──test──▶ T-IT
T11 ──ownership──▶ serialized after T6,T7    (edits Mp4Muxer + VideoExport)
T12 ──ownership──▶ serialized after T7,T8,T9 (edits VideoExport, JNI, VideoExporter)
```

**Cycle check:** none. All edges point forward T1→…→T12→tests; `N-CMAKE`/all-file creation is
isolated to T1; later native logic tasks own disjoint files except the explicitly serialized
`N-VIDEOEXPORT`/`C7` edits (T7→T11→T12).

---

## 6. Execution waves

| Wave | Tasks | Concurrency / serialization | Reason |
|---|---|---|---|
| 0 | **T1** | solo | Creates all new native files + CMake link; every other native task depends on it |
| 1 | **T2**, **T3**, **T4**, **T5** | concurrent | Disjoint files (PlaybackError/video_export enum, frame_source, offscreen_renderer, h264_encoder); T5 before T6 |
| 2 | **T6** | after T5 | Muxer needs the encoder's SPS/PPS contract |
| 3 | **T7** | after T3,T4,T5,T6 | Orchestrator integrates all stages (owns `N-VIDEOEXPORT`) |
| 4 | **T8** | after T7,T2 | JNI against the session API (owns `N-JNI`) |
| 5 | **T9**, **T-VAL** | T9 after T8,T2; T-VAL after T9 | Kotlin facade; JVM validation test can be authored immediately after |
| 6 | **T10** | concurrent-eligible with waves 1–5 (only depends T1), scheduled here to keep W3 grouped | Audio stage is a disjoint new file |
| 7 | **T11** | after T7,T10 | Serialized: edits `VideoExport` + `Mp4Muxer` |
| 8 | **T12** | after T9,T11 | Serialized: edits `VideoExport`, JNI, `VideoExporter` |
| 9 | **T-DEV**, **T-IT** | after production tasks land | Device/ABI verification (testing + integration stages) |

---

## 7. Test scope and verification matrix

| Test-ID | AC / risk | level | target component/contract | behavior/error scope | fake/fixture boundary | production Task | depends on | execution expectation |
|---|---|---|---|---|---|---|---|---|
| **T-VAL** | AC-7 | JVM unit | `VideoExporter` public API | Reject blank path / bad speed / bad interval / non-writable output without firing listener; single-active-export | none (pure Kotlin; no native load) | T9 | T9 | **Runnable** `:videolib:testDebugUnitTest` |
| **T-EXPORT-VIDEO** | AC-1,2,3,4,5 | instrumented | end-to-end single-clip video-only export | Produces a playable MP4; H.264 video track; frames reflect speed(PTS)+adjustments+filter | `TestVideoFixture` sample video | T7,T9 | T9 | Device/ABI only (encode/GL/mux) — testing stage |
| **T-CANCEL** | AC-8 | instrumented | `VideoExport` lifecycle | Cancel mid-export stops promptly; partial file deleted; no completed callback | `TestVideoFixture` | T7,T9 | T9 | Device only |
| **T-ERR** | AC-7 | instrumented | error path | Unreadable/non-video source → typed `PlaybackError`; no partial success | invalid/short fixture | T7,T9 | T9 | Device only |
| **T-AUDIO** | AC-6 | instrumented | audio + mux | Output has AAC audio track in sync with sped video; no-audio source → video-only (A-2) | `TestVideoFixture` (with/without audio) | T11 | T11 | Device only |
| **T-TIMELINE** | AC-9 | instrumented | timeline orchestration | Ordered segments concatenated into one MP4; per-segment effects; segment failure → error+index | multi-clip fixtures | T12 | T12 | Device only |
| **T-LINK** *(extend existing `FfmpegLinkageTest` style)* | Risk build | instrumented | `libvideolib.so` load | `mediandk` symbols + new exports resolve at runtime | none | T1,T8 | T8 | Device only |

### Module integration matrix

| Check-ID | changed module | affected consumer/external contract | boundary | command / device check | why required | owner |
|---|---|---|---|---|---|---|
| **Check-1** | videolib | build/packaging | CMake link, ABI, 16 KB alignment | `:videolib:assembleDebug` + AAR/`.so` inspection (`nm` mediandk resolvable) | New `mediandk` link + new sources must not break link/packaging | T-IT |
| **Check-2** | videolib | `app` (project dep) + external (unknown) | public Kotlin API additive | `:app:assembleDebug` | Confirm additive API does not break the in-repo consumer | T-IT |
| **Check-3** | videolib | native runtime | encode/mux/GL/audio/timeline | supported-device/ABI export run (T-DEV) | Compile cannot prove encode/mux/GL/A-V-sync correctness (native-boundary-guideline) | T-IT |

---

## 8. Sprint plan

Omitted — not requested (no capacity supplied).

---

## 9. Shared infrastructure and risk constraints

| Risk / shared surface | Affected IDs | Required serialization / verification | Owning task |
|---|---|---|---|
| **Risk-1 — MediaCodec-H.264 → `avformat` handoff** (SPS/PPS extradata + PTS/DTS rescale). Top design risk. | T5, T6, AC-5 | Muxer extradata built from the encoder's captured codec-config; PTS/DTS rescaled to stream time_base; guarded by T-EXPORT-VIDEO playability assertion | T6 |
| **Risk-2 — A/V sync under speed change** | T10, T11, AC-6, AC-2 | Audio `atempo` factor derived from the same speed as the video PTS scale; verified by T-AUDIO | T11 |
| **Risk-3 — minSdk-21 vs API-26 encode surface** | T5, D-3, E-8 | Invariant: byte-buffer MediaCodec input only; **no** `createInputSurface`; enforced at review + T-LINK on a 21-floor device | T5 |
| **Risk-4 — native lifecycle / stop-before-release** across decode→GL→codec→muxer threads | T7, AC-8 | Cancellation via `cancelRequested_`+FFmpeg interrupt; ordered teardown; one terminal event; verified by T-CANCEL | T7 |
| **Risk-5 — public enum extension** (`PlaybackError` +3 values) | C3, T2, K-PLAYBACKERROR | Additive values only; downstream exhaustive `when` must add branches. Acceptable for a pre-1.0 scaffold with nascent consumers; recorded as a source-compat note (alternative: separate `ExportError` if the team rejects the extension) | T2 |
| **Shared file — `N-VIDEOEXPORT` / `C7` (Mp4Muxer)** | T7, T11, T12 | Serialize edits: T7 (create) → T11 (audio wiring) → T12 (timeline); never concurrent | T7 |
| **Shared file — `N-JNI` (`videolib.cpp`)** | T8, T12 | Serialize: T8 adds export exports → T12 adds timeline export; additive only, no rename of existing `VideoPreview` JNI symbols | T8 |
| **Shared file — `N-CMAKE`** | T1 | All CMake edits done once in T1; later tasks do not touch it | T1 |

**Traceability preserved:** `FR → SC → AC → Work-ID → Task-ID` intact (§3/§4); every AC maps to a
production task and a Test-ID; every Test-ID maps to an AC/risk and a production task; every task
path/symbol resolves to §2. DAG valid, no cycle, no planning blocker → `AUTOMATION: CONTINUE`.
Publishing/signing/upload remain separately authorized and are **not** part of this plan.
