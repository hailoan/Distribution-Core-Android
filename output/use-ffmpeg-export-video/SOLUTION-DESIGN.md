AUTOMATION: CONTINUE

# SOLUTION-DESIGN — use-ffmpeg-export-video

Design for an FFmpeg + MediaCodec **video export** in `:videolib` that renders the already-applied
preview effects (speed, appearance adjustments, GLSL filter) to an MP4 file. Grounded in
`DEV-SPEC.md` and repository source. Design only — no code, tasks, or estimates.

---

## 1. Decision ledger

**Investigation depth:** deep · ~18 lookups (DEV-SPEC + preview pipeline sources already read in
feature-analysis, reused here; new this stage: `preview_renderer.cpp`, `gl_program.cpp` prefix,
`render_thread_executor.h`, camera `video_encoder.h`, `videolib` manifest, `nm` audio-codec probe,
routed guideline skills). Cap honored; stopped when two consecutive lookups added no material design
evidence.

### Sources / evidence

| Ref | Evidence | Status |
|---|---|---|
| E-1 | Preview pipeline exists: FFmpeg demux/decode → `sws_scale`→RGBA → `GlProgram` appearance+filter → present to `Surface` (`video_playback.cpp:528-1095`, `preview_renderer.cpp`, `gl_program.cpp`) | observed |
| E-2 | Effect parameters are a shared `AppearanceSnapshot` (16 adjustments + versioned GLSL filter+textures), validated identically Kotlin/native (`appearance.h`, `VideoPreview.kt:407-453`, `video_playback.cpp:72-124`) | observed |
| E-3 | Preview presents via **window** EGL surface + `eglSwapBuffers`; no FBO/pbuffer/readback path exists in `:videolib` (`preview_renderer.cpp:32-48,152`) | observed |
| E-4 | Preview pacing is **wall-clock**, dropping late frames at speed>1 (`video_playback.cpp:725-797`) — unsuitable for a file target | observed |
| E-5 | Prebuilt LGPL FFmpeg has **no usable software H.264 encoder**; has `ff_mp4_muxer`, `ff_aac_encoder/decoder`, `ff_af_atempo`, `ff_af_aresample`, and video-frame decoders (`nm` probe; `config.sh` LGPL) | observed |
| E-6 | Camera module has a **proven** `AVFrame(YUV420P)→AMediaCodec(H.264)→AMediaMuxer(MP4)` encoder on a worker thread with NV12 conversion + MP4 rotation hint (`camera/src/main/cpp/video/video_encoder.h`) — different module, pattern reference only | observed |
| E-7 | `:videolib` **minSdk 21**; ARM-only ABIs; 16 KB page alignment; C++17; single `libvideolib.so` target; JNI is hand-mangled by name (`build.gradle.kts`, `CMakeLists.txt`, `videolib.cpp`) | observed |
| E-8 | `AMediaCodec_createInputSurface` (GPU input-surface encode) requires **API 26**; byte-buffer codec input + `AMediaMuxer` are API 21 (NDK media docs) | observed |
| E-9 | Existing input contract is **local readable file only** (`isReadableLocalFile`, `video_playback.cpp:55-61`); host owns output path/permissions (context §6) | observed |

### Decisions

| ID | Decision | Rationale / evidence | Status |
|---|---|---|---|
| D-1 | Export is a **new sibling public component** (`VideoExporter`, semantic role) — not new methods on `VideoPreview`. | Export is headless (no host `Surface`), PTS-paced (not wall-clock), one-shot file producer. Overloading `VideoPreview`'s single-attempt, surface-bound model (E-3,E-4) would break its invariants. Additive → source/binary compatible. | proposed |
| D-2 | Reuse the **decode/demux/`sws_scale` primitives** and the **`GlProgram` effect program** (E-1,E-2); do **not** reuse the wall-clock scheduler (E-4) or the window-surface renderer (E-3). | Effect parity (SC-2) requires the same GLSL program; a file target requires PTS-passthrough framing, not real-time drop. | proposed |
| D-3 | Video encode = **Android MediaCodec H.264**, fed by **CPU byte-buffer input** (offscreen GL render → `glReadPixels` RGBA → `sws_scale` RGBA→NV12 → codec input buffer). | DEV-SPEC FR-6. Byte-buffer input works on the full **minSdk 21** range; the GPU input-surface path needs API 26 (E-8) and would silently raise export's floor. Mirrors proven camera pattern (E-6) and reuses already-linked libswscale (E-5). | proposed |
| D-4 | Effect rendering runs on an **offscreen EGL target** (pbuffer or FBO), not a window surface. | Export has no host `Surface`; current renderer is window-bound (E-3). Same `GlProgram`, different EGL setup + readback. | proposed |
| D-5 | **Muxing = FFmpeg `avformat` MP4 muxer.** MediaCodec H.264 access units are wrapped as `AVPacket`s (video stream: `codec_id=H264`, `extradata`=SPS/PPS from the codec-config buffer, rescaled PTS/DTS); FFmpeg-encoded AAC packets form the audio stream. | Honors the clarified "FFmpeg for MP4 muxing"; naturally consumes the FFmpeg AAC packets (D-6). MP4 muxer + AAC present (E-5). `AMediaMuxer` (camera, E-6) is a viable alternative — noted, not chosen. | proposed |
| D-6 | **Audio** = FFmpeg decode → speed re-time (`atempo`, chained for the 0.1+ speed range) → AAC encode → muxed (FR-7). Speed==1.0 stream-copy is an allowed optimization, left unspecified. | Preview is video-only; export must add audio and keep it in sync with the video speed transform (E-5 has atempo/aresample/AAC). | proposed |
| D-7 | Encoder configured to **source dimensions (even-adjusted `& ~1`) and source frame rate**; bitrate by a resolution-derived heuristic (value left implementation-local). | DEV-SPEC Q-C assumption (match source); H.264/NV12 require even dimensions (native-boundary-guideline). | proposed |
| D-8 | Output = **caller-supplied local writable file path**; host owns creation, permissions, and any MediaStore/scoped-storage placement. | Matches existing local-file-only I/O contract (E-9) and library/host split (context §6). | proposed |
| D-9 | Export covers both a **single clip** and an **ordered timeline** (mirror preview, DEV-SPEC Q-A), applying each segment's own speed/appearance/filter and concatenating them into one output. | The ticket exports "these applying before" — i.e. whatever the user built in preview, which includes the timeline (`VideoSegment`/`playTimeline`, E-1). | proposed |
| D-10 | Terminal outcome via a **listener** (completed / typed error), reusing the existing `PlaybackError` taxonomy; export is **cancellable**. Incremental progress is an optional addition. | Mirrors `PlaybackListener`; cancellation + no-partial-success are correctness requirements (§2 AC-8). | proposed |

### Explicitly unspecified (implementation-local)

Bitrate heuristic value; keyframe/GOP interval; exact NV12 stride handling and codec color-format
selection; atempo chaining factorization; speed==1.0 audio stream-copy optimization; output
container brand/faststart; progress-callback cadence; final Kotlin/JNI symbol names and signatures.

### Blockers

**None.** The two DEV-SPEC blocking gaps were resolved by clarification (FR-6, FR-7). The minSdk-21
vs API-26 tension is resolved by D-3 (byte-buffer path). Timeline scope is resolved by D-9. →
`AUTOMATION: CONTINUE`.

---

## 2. Behavior and state transitions

### Behavior contract

| FR | SC | AC | Story | Rule / trigger | Observable outcome | Failure / recovery |
|---|---|---|---|---|---|---|
| FR-1 | SC-1 | **AC-1** | Story-1 | Caller starts an export for a valid local source + output path (no on-screen surface required) | An MP4 file is produced at the output path | Invalid source/path → typed error, no file claimed successful (AC-7) |
| FR-2 | SC-2 | **AC-2** | Story-1 | Each source frame is emitted at its media PTS scaled by the segment/clip speed | Exported motion matches the sped preview; **no** wall-clock frame drops | Decode error → terminal error (AC-7) |
| FR-3 | SC-2 | **AC-3** | Story-1 | The same `AppearanceSnapshot` adjustments drive the export GL program | Exported frames show identical adjustments to preview | Invalid appearance rejected before start (reuses E-2 validation) |
| FR-4 | SC-2 | **AC-4** | Story-1 | The same versioned GLSL filter + textures drive the export GL program | Exported frames show identical filter to preview | Filter compile/link failure → terminal render error |
| FR-6 | SC-3 | **AC-5** | Story-2 | Filtered frames are H.264-encoded (MediaCodec) and MP4-muxed (FFmpeg) | Output is a playable MP4 whose video track is H.264 | Encoder/muxer init failure → terminal error, partial file removed |
| FR-7 | SC-4 | **AC-6** | Story-2 | Source audio is decoded, speed-re-timed, AAC-encoded, and muxed | Output contains an audio track in sync with the sped video | No audio track in source → video-only output (proposed sub-case, see §7) |
| FR-1 | — | **AC-7** | Story-1 | Unreadable/non-video/unsupported-codec source, or write failure | Export ends with a typed `PlaybackError`; no partial file reported as success | Terminal error; partial output deleted |
| FR-1 | — | **AC-8** | Story-1 | Caller cancels / releases mid-export | Export stops promptly; the in-progress file is not reported as a completed export | Cancelled; partial output deleted |
| FR-1 | SC-2 | **AC-9** *(proposed, D-9)* | Story-1 | Source is an ordered `VideoSegment` timeline | Segments are concatenated in order, each with its own speed/appearance/filter | Any segment failure → whole export fails with the failing segment index |

### State model

| State | Meaning / invariants | Permitted events | Prohibited / ignored |
|---|---|---|---|
| `Idle` | No export active; exporter usable | `start` | `cancel` (no-op), terminal events |
| `Preparing` | Source opened, decoders/encoders/muxer being initialized; no frame written yet | `cancel`, init-fail, init-ok | second `start` (rejected) |
| `Exporting` | Frames being decoded→filtered→encoded→muxed; output file open | `cancel`, frame-error, source-EOF | second `start` (rejected) |
| `Completed` | Muxer finalized; output file valid and complete | — (terminal; new export needs a fresh attempt) | frame events |
| `Failed` | Terminal error; partial output removed | — | frame events |
| `Cancelled` | Caller-terminated; partial output removed | — | frame events |

### Transition contract

| From | Event / precondition | To | Side effect | Failure / cancellation / recovery |
|---|---|---|---|---|
| `Idle` | `start` accepted (valid params, no active attempt) | `Preparing` | Open source, allocate decode/encode/mux resources | Param/validation reject → stay `Idle`, return rejection (no listener fired) |
| `Preparing` | init success | `Exporting` | Begin decode→filter→encode→mux loop; write moov/header | init failure → `Failed`, release resources, delete partial, fire error |
| `Exporting` | source EOF / last segment complete | `Completed` | Flush encoders, finalize muxer (write trailer), close file | flush/finalize failure → `Failed`, delete partial, fire error |
| `Exporting` | decode/render/encode/mux error (or segment error) | `Failed` | Stop workers, release, delete partial | fire typed error (+ segment index for timeline) |
| `Preparing`/`Exporting` | `cancel` / `release` | `Cancelled` | Signal cancel (reuse `cancelRequested_`+FFmpeg interrupt pattern), join workers, release, delete partial | idempotent; late frames suppressed |

---

## 3. Components and responsibilities

### Module Contract Matrix

| Module | Owner/consumer | Responsibility | Depends on | Crossed contract | Compatibility obligation | Verification obligation |
|---|---|---|---|---|---|---|
| `videolib` | **owner** | New public export API + native export pipeline (decode reuse, offscreen GL effect, H.264 encode, audio transcode, MP4 mux) | FFmpeg archives (linked), NDK `mediandk` (**new link**), EGL/GLESv3 (linked) | Public Kotlin API (additive); JNI (new hand-mangled exports); CMake (add `mediandk` + new `.cpp` to the one `libvideolib.so` target); ABI/16 KB packaging | Additive only → source & binary compatible; keep ARM-only ABIs + 16 KB alignment + C++17 | `:videolib:assembleDebug` proves compile/link/packaging; **device/ABI run** proves encode/mux/decode/GL/A-V-sync |
| `app` | consumer | May add an export trigger in the demo (**out of scope** unless requested) | `:videolib` | — | Source-compatible; no forced change | `:app:assembleDebug` if touched |
| external consumers | consumer (unknown) | — | `:videolib` | Public API additive | No break expected; keep additions additive | n/a in-repo |

### Components

| Component (role) | Observed / proposed | Responsibility / owned state | Delegates to | Dependency direction | Must not own / know | Evidence / decision |
|---|---|---|---|---|---|---|
| `VideoExporter` (Kotlin public facade) | proposed | Accept export request (source/segments + appearance + speed + output path + listener); validate params (reuse E-2 rules); own the export handle + attempt/cancel lifecycle; marshal terminal callback to main thread | native export session (JNI) | Kotlin → JNI | Encoder/muxer/GL mechanics; the host `Surface` | D-1, E-2, mirrors `VideoPreview.kt` callback model |
| Native export session (owner) | proposed | Coordinate one export attempt across decode, GL effect, video encode, audio transcode, mux workers; own cancellation + stop-before-release ordering; emit exactly one terminal event | decode stage, effect renderer, video encoder sink, audio stage, muxer | JNI → native | Real-time pacing; window surfaces | D-1..D-6, pattern from `VideoPlayback` |
| Decode / demux stage | proposed (reuse of E-1 primitives) | Demux, decode video (and audio), `sws_scale` video→RGBA, honor trim `[startMs,endMs)` and per-segment ordering, **PTS passthrough** (no wall-clock wait) | — | native | Wall-clock scheduling, drop-when-late (E-4) | D-2 |
| Effect renderer (offscreen) | proposed (reuse `GlProgram`) | Apply `AppearanceSnapshot` (adjustments+filter) to each RGBA frame on an offscreen EGL target; read back filtered RGBA | `GlProgram` | native | Encoding, muxing, source `Surface` | D-2, D-4, E-3 |
| Video encoder sink (H.264) | proposed (pattern from E-6) | RGBA→NV12 (`sws_scale`), feed MediaCodec byte-buffer input, drain encoded access units with correct PTS + codec-config (SPS/PPS) | MediaCodec | native → `mediandk` | Muxing policy, audio | D-3, D-5, E-6, E-8 |
| Audio transcode stage | proposed | Decode source audio → `atempo` re-time to speed → AAC encode → hand packets to muxer | FFmpeg (avcodec/avfilter/swresample) | native | Video/GL, speed authority (reads segment speed) | D-6, E-5 |
| MP4 muxer | proposed | Own the `avformat` MP4 output: create H.264 + AAC streams, write header, interleave video/audio `AVPacket`s, write trailer; delete file on failure/cancel | FFmpeg avformat | native → FFmpeg | Encoding/decoding internals | D-5, E-5 |
| `GlProgram` / `AppearanceSnapshot` | observed (reused verbatim) | Effect shader program + validated effect parameters | — | — | — | E-1, E-2 |
| camera `VideoEncoder` | observed (reference only) | Proven MediaCodec H.264 + muxer pattern; **not** linked cross-module | — | — | — | E-6, ground rule (no cross-module link) |

---

## 4. End-to-end data flow

### Flow A — single-clip export (normative)

| # | Participant | Input / source | Decision / transformation | Output / side effect | Error propagation |
|---|---|---|---|---|---|
| 1 | `VideoExporter` | source path, `VideoAppearance`, speed, output path, listener | Validate params (E-2 rules; local-readable source E-9; writable output D-8) | Accept → start native session; else reject synchronously | Reject returns without firing listener |
| 2 | Native session | accepted request | `Idle→Preparing`: open demux, init video+audio decoders, offscreen EGL+`GlProgram`, MediaCodec H.264, AAC encoder, `avformat` MP4 muxer | Resources allocated; output file opened | Any init fail → `Failed`, delete file, fire error (AC-7) |
| 3 | Decode stage | demuxed packets | Decode video → `sws_scale`→RGBA; carry PTS; apply speed as PTS scale (AC-2); no wall-clock wait | RGBA frame + scaled PTS | Decode error → `Failed` (AC-7) |
| 4 | Effect renderer | RGBA frame + `AppearanceSnapshot` | Draw through `GlProgram` on offscreen target (AC-3, AC-4); `glReadPixels` filtered RGBA | Filtered RGBA frame | GL compile/render fail → `Failed` (render error) |
| 5 | Video encoder sink | filtered RGBA + PTS | `sws_scale` RGBA→NV12; queue to MediaCodec; drain access units | H.264 `AVPacket`s (with SPS/PPS extradata, rescaled PTS/DTS) | Codec error → `Failed` |
| 6 | Audio stage | demuxed audio packets | Decode → `atempo`(speed) → AAC encode (AC-6) | AAC `AVPacket`s with re-timed PTS | Audio error → `Failed`; source-has-no-audio → video-only (§7) |
| 7 | MP4 muxer | H.264 + AAC packets | Interleave by PTS; write MP4 | Frames/samples written to output file | Write/interleave error → `Failed`, delete file |
| 8 | Native session | EOF on all streams | `Exporting→Completed`: flush encoders, write trailer, close, release | Valid MP4 at output path (SC-1,3,4) | Finalize fail → `Failed`, delete file |
| 9 | `VideoExporter` | terminal event (JNI, worker thread) | Marshal to main thread (mirror `VideoPreview` handler) | `onExportCompleted()` / `onExportError(error)` | — |

Source of truth: the output file is authoritative only after step 8 (`Completed`); before that a
partial file is never surfaced as success (AC-7, AC-8). Ordering: video PTS and audio PTS derive
from the same speed transform so the muxed streams stay in sync (D-6).

### Flow B — timeline export (proposed, D-9 / AC-9)

Same as Flow A per segment, sequenced in list order into **one** output (reuses the `runTimeline`
orchestration shape, E-1). Each segment contributes its `[startMs,endMs)` window with its own speed,
appearance, and filter; video PTS is made continuous across segment boundaries, and audio is
concatenated with per-segment `atempo`. Any segment failure ends the whole export with that
segment's index (AC-9). Added risk: heterogeneous per-segment audio (sample rate/codec) concat — see
§7 / §9.

---

## 5. Boundary contracts

| Contract / boundary | Observed/proposed | Semantic input | Output / result | Invariants | Errors | Compatibility / versioning | Owner |
|---|---|---|---|---|---|---|---|
| Public export API (Kotlin) | proposed | source (path or ordered `VideoSegment`s) + `VideoAppearance` + speed + output path + listener | accept/reject; async terminal `completed`/`error(+segmentIndex)` | One active export per exporter; params validated by the **same** rules as `setAppearance`/`playTimeline`; effect parity with preview | rejects invalid params synchronously; typed `PlaybackError` on async failure | Additive to `:videolib` public API; source & binary compatible | `videolib` |
| JNI export boundary | proposed | export params marshalled to native; callbacks to Kotlin | native handle; terminal callback on worker thread | Hand-mangled names in exact sync with Kotlin; direct `ByteBuffer`s for any zero-copy; global-ref/attach-detach discipline | `UnsatisfiedLinkError` if names drift; cleared JNI exceptions on marshalling failure | New exports additive; must not rename existing `VideoPreview` JNI symbols | `videolib` |
| Effect parameter contract | observed (reused) | `AppearanceSnapshot` (16 adjustments + versioned filter+textures) | GL program state | Identical validation and GLSL to preview (SC-2) | filter version/source/opacity/texture errors reuse `AppearanceError` | Unchanged; shared with preview | `videolib` |
| Video encode boundary | proposed | filtered NV12 frames + PTS | H.264 elementary stream | Even dimensions (`& ~1`); PTS monotonic; SPS/PPS captured from codec-config buffer | codec init/queue/drain failure → render/encode error | MediaCodec H.264 baseline/main; device-dependent | `videolib` (→ `mediandk`) |
| Audio transcode boundary | proposed | source audio packets + speed | AAC elementary stream, re-timed | Audio PTS timeline consistent with video speed (A/V sync) | decode/filter/encode failure → error | AAC-LC in MP4 | `videolib` (→ FFmpeg) |
| MP4 mux boundary | proposed | H.264 + AAC `AVPacket`s | MP4 file at output path | Streams interleaved by PTS; trailer written before success; partial file deleted on failure | I/O/mux error → error, file removed | MP4 (`mov`/`mp4` muxer) | `videolib` (→ FFmpeg avformat) |
| CMake / packaging | proposed | build config | `libvideolib.so` with `mediandk` linked | ARM-only ABIs; 16 KB alignment; C++17; one shared target | link failure surfaces at `assembleDebug` | Add `mediandk` link + new `.cpp` to existing target; no ABI/alignment change | `videolib` |
| Output file / storage | proposed (host-owned) | caller-supplied local writable path | written MP4 | Host owns creation, permissions, MediaStore placement | unwritable path → early reject/error | Local filesystem path only (matches input contract) | host |

---

## 6. Conditional cross-cutting design

- **Async / concurrency (correctness-critical):** Export runs on a native worker (like the preview
  decode worker), with GL/EGL work marshalled onto a single render thread (`RenderThreadExecutor`
  pattern) and MediaCodec drained on its own thread (camera pattern). Cancellation reuses the
  `cancelRequested_` atomic + FFmpeg `interrupt_callback` and enforces **stop-before-release**
  ordering across decode → GL → codec → muxer (native-boundary-guideline; `video_playback.cpp`).
  Exactly one terminal event per attempt. `JNIEnv` re-derived per worker thread via the cached
  `JavaVM`; callback objects promoted to global refs (ndk-cpp-guideline).
- **Platform / SDK behavior:** Byte-buffer MediaCodec input + `AMediaMuxer`/`avformat` keep export on
  the full **minSdk 21** range (D-3, E-8); the API-26 input-surface path is deliberately **not**
  used. `mediandk` is a new NDK link but is available from API 21.
- **Native packaging (build-logic):** Add `mediandk` to `target_link_libraries` and the new export
  `.cpp` to the single `libvideolib.so` source list; preserve ARM `abiFilters`, `CMAKE_ANDROID_PAGE_SIZE
  16384`, `-Wl,-z,max-page-size=16384`, arm64 `-Bsymbolic`, and C++17 (E-7, gradle/ndk guidelines).
- **Performance:** Export is offline/faster-or-slower-than-real-time; the CPU readback + `sws_scale`
  RGBA→NV12 path (D-3) is heavier than a GPU surface path but is the minSdk-safe, proven choice.
  Even dimensions required for H.264/NV12.
- **Security / privacy:** No new network or credential surface. Reads a caller-owned local file,
  writes a caller-owned local file; no logging of file contents.
- **Risk mitigation:** See §9. Top risk is the MediaCodec-H.264-into-`avformat` integration
  (extradata/PTS handoff) and A/V sync under speed changes.

No UI design evidence exists (no Figma/`.tsx`/image); Design Conformance Contract omitted.

---

## 7. Coverage audit

| Input | Resolved by | Status |
|---|---|---|
| FR-1 (export operation) | §3 components, §4 Flow A, AC-1 | designed |
| FR-2 (speed) | D-2 (PTS passthrough), AC-2 | designed |
| FR-3 (adjustments) | D-2 (`GlProgram` reuse), AC-3 | designed |
| FR-4 (filter) | D-2 (`GlProgram` reuse), AC-4 | designed |
| FR-5 (`:videolib`-only) | §3 Module Contract Matrix (owner=`videolib`; camera reference not linked) | designed |
| FR-6 (H.264/MP4 via MediaCodec) | D-3, D-5, AC-5, §5 encode/mux boundaries | designed |
| FR-7 (source audio) | D-6, AC-6, §5 audio boundary | designed |
| SC-1..SC-4 | AC-1/AC-2..AC-4/AC-5/AC-6 | mapped |
| Q-A (timeline?) | D-9 / AC-9 (mirror preview; single-clip normative, timeline proposed) | resolved (assumption) |
| Q-B (output destination) | D-8 (caller-supplied local path; host-owned) | resolved (assumption) |
| Q-C (resolution/bitrate/fps) | D-7 (match source; bitrate heuristic unspecified) | resolved (assumption) |
| Edge: invalid/unsupported source | AC-7 | designed |
| Edge: cancellation | AC-8, §2 transitions | designed |
| Edge: source has no audio | §5 audio boundary → video-only sub-case (proposed) | proposed |

**Unresolved inputs needed to complete the design:** none blocking. Deferrable confirmations for
implementation-plan/coding: (a) whether timeline export (AC-9) is in the first delivery or a
follow-up; (b) behavior when the source has no audio track (video-only vs error); (c) bitrate policy
value. None change the architecture.

**Traceability:** `FR → SC → AC → Story` preserved in §2; every SC-ID retains an AC-ID for
downstream test mapping. Design is complete with no blocker → `AUTOMATION: CONTINUE`.

---

## 8. Notes for downstream (non-prescriptive)

- The stage packet still lists `:videolib` as a "scaffold"; source contradicts this (full preview
  pipeline). Design is against source (DEV-SPEC Conflict-1).
- The camera `VideoEncoder` is a **pattern reference only** — do not add a cross-module dependency;
  `:videolib` gets its own encoder against `mediandk` (FR-5).
- Verification levels: `:videolib:assembleDebug` proves JNI symbol/link/packaging (incl. new
  `mediandk` link, ABI, 16 KB); only a **supported-device/ABI run** proves encode/mux/decode/GL
  correctness and audio-video sync. Never claim runtime behavior from compile alone.
