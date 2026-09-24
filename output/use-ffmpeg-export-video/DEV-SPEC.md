AUTOMATION: CONTINUE

# DEV-SPEC — use-ffmpeg-export-video

Add an FFmpeg-based **video export** to `:videolib` that writes the *already-applied* preview
effects (speed, appearance adjustments, filter) to an output video file, reusing the existing
preview decode/filter pipeline.

---

## 0. Analysis Control

| Field | Value |
|---|---|
| Outcome | **CONTINUE** — final validation PASS; both blocking gaps resolved by clarification |
| Scope classification | Existing-code · single-module (`:videolib`) · native/JNI · FFmpeg + MediaCodec boundary · **high-risk** (native lifecycle, ABI packaging, new public contract) |
| Depth | **deep** (native + FFmpeg + build boundary) |
| Kind | `feature` (ticket-reading) — not a bug |
| Lookup ledger | ~14 evidence reads: ticket; `VideoPreview.kt`; `video_playback.{h,cpp}`; `preview_renderer.h`; `gl_program.h`; `appearance.h`; `videolib.cpp` (JNI); `VideoAppearance.kt`; `VideoSegment.kt`; `CMakeLists.txt`; `build.gradle.kts`; `ffmpeg-build/config.sh`; `nm` symbol probe of `libavcodec.a`/`libavformat.a`/`libavfilter.a`; `MainActivity2.kt`; camera `video_encoder.{h,cpp}` (reuse ref). One escalation into the FFmpeg archive symbol table — justified: encoder availability is a blocking build/product constraint. |

**Focus-area applicability**

| # | Focus area | Applies? |
|---|---|---|
| 1 | Requirements | ✅ applicable (§2–§5) |
| 2 | Edge cases | ✅ applicable — best-effort proposals (§5a); ticket is thin |
| 3 | Feature impact | ✅ applicable — existing preview pipeline is reused (§6) |
| 4 | Risk | ✅ applicable (§9) |
| 5 | API docs | N/A — no remote/backend/network surface; the only "API" is the local FFmpeg/NDK **native** contract, covered in §6/§7 |
| 6 | Figma/design | N/A — no design source supplied; export has no visual UI in `:videolib` |

> **Packet staleness note [conflict]:** the generated stage packet describes `:videolib` as a bare
> "scaffold … Hello-from-C++ stub, no production video source yet". **Source contradicts this**: the
> module already contains a full FFmpeg-backed preview pipeline (playback, seek, speed, appearance
> adjustments, GLSL filter, and a multi-segment timeline). This spec is written against the **source**,
> per ground rule 0.

---

## 1. Sources

| Source | Type | Location | Read status |
|---|---|---|---|
| Ticket spec | markdown | `output/use-ffmpeg-export-video/input/use-ffmpeg-export-video.md` | read |
| Clarification (this run) | interactive `/study` answers | §8 | recorded `[fact:clarification]` |
| Preview implementation | first-party source | `videolib/src/main/{java,cpp}` | read |
| FFmpeg build config | first-party source | `videolib/ffmpeg-build/config.sh` | read |
| FFmpeg prebuilt archives | vendored binaries | `videolib/src/main/cpp/ffmpeg/<abi>/lib/*.a` | symbol-probed (`nm`) |

- Design link/source: **none.**
- API docs: **none** (no remote surface).
- Converted files: none (ticket was already `.md`; copied verbatim to `input/`).
- Design index: N/A.

---

## 2. Overview & Business Goal

The preview surface in `:videolib` already lets a caller apply **speed**, **appearance adjustments**,
and a **GLSL filter** to a local video (single clip or an ordered multi-segment timeline) and see the
result live. The goal is to **persist that same visual result to a file** — an export path that decodes
the source, applies the identical speed/adjustment/filter transforms, and writes an output video —
using the **FFmpeg API** already linked into the module. Scope is confined to the `:videolib` module.
`[fact:ticket]`

---

## 3. Functional Requirements

| FR-ID | Requirement | Status | Evidence / Source |
|---|---|---|---|
| FR-1 | Provide an export operation in `:videolib` that writes the previously-applied preview effects to an output video file, driven through the FFmpeg API. | fact | ticket ("use ffmpeg api, export video from these applying before") |
| FR-2 | Export applies the same **speed** transform available in preview (`VideoPreview.setPlaybackSpeed` / `VideoSegment.speed`, min 0.1). | fact | ticket; `VideoPreview.kt:200-205,113`; `video_playback.cpp:399` |
| FR-3 | Export applies the same **appearance adjustments** (the 16-value `VideoAdjustments` set: brightness, contrast, saturation, exposure, darks, levels{min,gamma,max}, vignette, vibrance, temperature, hue, highlights, shadows, lights, clarity). | fact | ticket; `VideoAppearance.kt:23-38`; `appearance.h:10-27` |
| FR-4 | Export applies the same **filter** (versioned GLSL `addFilter` source + opacity + RGBA8888 lookup textures) using the identical validation as preview. | fact | ticket; `VideoPreview.kt:433-453`; `video_playback.cpp:95-124`; `gl_program.h` |
| FR-5 | All code changes are contained in the `:videolib` module; no other module is edited. | fact | ticket ("scope code: just update into module `:videolib`") |
| FR-6 | Output video is **H.264 in an MP4 container**, encoded via **Android MediaCodec** (AMediaCodec); FFmpeg is used for demux/decode, effect rendering, audio, and MP4 muxing — not for H.264 video encode. | fact | clarification (Q1) — forced by the encoder-availability constraint in §7 |
| FR-7 | Export **includes the source audio track** in the output (not video-only). | fact | clarification (Q2) |

> Requirements FR-2..FR-4 are product intent from the ticket; the code references are the current
> constraints that make the effect set concrete, not the source of the requirement.

---

## 4. Actors & User Stories

| Story-ID | FR-ID | Story |
|---|---|---|
| Story-1 | FR-1..FR-4 | As an **integrating app developer**, I can call a `:videolib` export API to render a source video with the speed/adjustment/filter I already configured for preview into a saved file, so the user can keep the edited result. `[assumption:ticket]` (actor inferred; `:videolib` is a library and `app` is its only in-repo consumer — `app/build.gradle.kts:55`, `MainActivity2.kt`) |
| Story-2 | FR-6, FR-7 | As an **integrating app developer**, I get a standard, broadly-playable MP4 (H.264 video + audio) as the export output. `[fact:clarification]` |

---

## 5. Observable Success Conditions

| SC-ID | FR-ID | Explicit / clarified outcome | Evidence / Source |
|---|---|---|---|
| SC-1 | FR-1 | An export call for a valid local source produces an output video **file** that did not require a preview surface to be on screen. | ticket `[fact]` (export is a file-producing operation distinct from on-screen preview) |
| SC-2 | FR-2..FR-4 | The exported video's frames reflect the **same** speed, appearance adjustments, and filter that preview applies to the same input — i.e. the file matches "these applying before". | ticket `[fact]` |
| SC-3 | FR-6 | The produced file is a playable MP4 whose video track is H.264. | clarification `[fact:clarification]` |
| SC-4 | FR-7 | The produced file contains the source audio track. | clarification `[fact:clarification]` |

### 5a. Proposed edge cases & boundary behavior (best-effort — not normative)

Every row is a labelled proposal, **not** a confirmed success condition, and carries no SC-ID until the
user confirms it.

| FR-ID | Edge / boundary case | Expected handling (proposed) | Status | Source |
|---|---|---|---|---|
| FR-1 | Timeline vs single clip — preview supports **both** a single clip and an ordered multi-segment timeline (per-segment speed/appearance). Does export cover the timeline too? | Mirror preview's input model: export a single clip and/or a concatenated timeline, applying each segment's own speed/appearance/filter (the `runTimeline` orchestration already exists as a reference). | `[assumption]` — deferrable; see §8 Q-A | code `video_playback.cpp:1115-1168`; `VideoPreview.kt:108-165`; commit `fcd6ae6` |
| FR-1 | Invalid / unreadable source path, or non-video / unsupported codec. | Reject or fail the export with a typed error, reusing the existing `PlaybackError`/`PlaybackErrorCode` taxonomy rather than crashing. | `[assumption]` | code `video_playback.h:33-38`; `PlaybackError.kt` |
| FR-1 | Export cancellation / release while running. | Cancellable like playback (the pipeline already has `cancelRequested_` + interrupt callback); a cancelled export must not leave a partial file presented as success. | `[assumption]` | code `video_playback.cpp:63-66,314-346` |
| FR-2 | Speed `> 1.0` (frame dropping) vs `< 1.0` (frame stretch) for a *file* target. | Export must not drop/duplicate frames the way real-time preview does for wall-clock pacing; it should re-time by PTS so the whole `[A,B)` interval is written at the target rate. | `[assumption]` — differs from preview's real-time scheduler (`scheduleFrame` drops when late) | code `video_playback.cpp:725-797` |
| FR-4 | Filter requires a GL context; export has no on-screen `Surface`. | Effect rendering must run on an **offscreen** GL/EGL target (pbuffer/FBO) since the current `PreviewRenderer` binds to a host `ANativeWindow`. | `[assumption]` | code `preview_renderer.h:43,76`; `VideoPreview.kt:213-217` (filter apply rejects when surface unavailable) |
| FR-7 | Speed change applied to audio. | Audio must be re-timed to match the video speed (e.g. `atempo`) or A/V will desync; `atempo`/`aresample` filters and AAC encode/decode are present in the prebuilt libs. | `[assumption]` | symbol probe: `ff_af_atempo`, `ff_af_aresample`, `ff_aac_encoder`, `ff_aac_decoder` present |
| FR-6 | Output path / file lifecycle ownership. | Consistent with the module today, the **host** likely supplies the output path/URI and owns permissions; `:videolib` writes to a caller-given local path. | `[assumption]` | context §6 "A consuming host owns … output-path creation"; `video_playback.cpp:55-61` (local-file only) |

---

## 6. Engineering Evidence — *Non-normative*

### Module impact hypothesis

| Module | Owner / consumer | Dependency evidence | Likely contract change | Status / confidence |
|---|---|---|---|---|
| `videolib` (`com.cii.videolib`) | **primary owner** | ticket pins scope; all preview/export code lives here | New **public Kotlin API** for export (new class or new `VideoPreview` methods) + new **JNI** entry points + **CMake** link to `mediandk` for AMediaCodec/AMediaMuxer + native encode/mux code | fact / high |
| `app` (`com.chiistudio.library`) | direct consumer | `app/build.gradle.kts:55` `implementation(project(":videolib"))`; `MainActivity2.kt` drives preview | Source-compatible (additive API); may add an export trigger in the demo, but that is **out of scope** per FR-5 unless requested | assumption / high |
| External consumers | unknown | `:videolib` contract is "public; external consumers unknown" (registry) | Additive only → no break expected; keep additions additive | unknown / medium |

- **Primary owner:** `videolib`. **Changed modules:** `videolib` only (FR-5). **Dependency closure:** `videolib → app` (project dependency), semantics = additive API; no other in-repo edge.
- **Crossed boundaries:** Kotlin public API · JNI signature/ownership · CMake/native linkage (**new** `mediandk` system lib for MediaCodec/Muxer) · ABI packaging (arm64-v8a, armeabi-v7a only) · 16 KB page alignment.

### Entry points (smallest confirmed set)

| Symbol | Role | File:line |
|---|---|---|
| `VideoPreview.play` / `playTimeline` | Kotlin public entry that starts the decode→filter→present pipeline | `VideoPreview.kt:63`, `VideoPreview.kt:108` |
| `VideoPlayback::decodeAttempt` | Native decode loop: FFmpeg demux/decode → `sws_scale` to RGBA → render | `video_playback.cpp:528` |
| `PreviewRenderer` / `GlProgram` | EGL/GLES appearance + filter program applied per frame | `preview_renderer.h:26`, `gl_program.h:12` |
| `VideoPreview.setAppearance` / `setPlaybackSpeed` | Kotlin controls for the transforms export must reuse | `VideoPreview.kt:208`, `VideoPreview.kt:201` |

### Current behavior (summarized)

| Behavior | Status | Evidence |
|---|---|---|
| Preview decodes a local file with FFmpeg, converts frames to RGBA via `sws_scale`, applies the appearance+filter GL program, and **presents to a host `Surface`** (real-time, wall-clock paced). | fact | `video_playback.cpp:799-898`; `preview_renderer.h:43` |
| Speed, 16 appearance adjustments, and versioned GLSL filter+textures are already validated identically on the Kotlin and native sides. | fact | `VideoPreview.kt:407-453`; `video_playback.cpp:72-124` |
| Multi-segment timeline sequences per-segment `[startMs,endMs)`, speed, and appearance back-to-back; single terminal callback. | fact | `video_playback.cpp:1115-1168` |
| Pipeline is **decode-only** today — no encode/mux path exists in `:videolib`. | fact | grep: no `AMediaCodec`/`AMediaMuxer`/`avcodec_send_frame`/muxer use in `videolib/src` |

### Affected boundaries (confirmed)

| Boundary | Why it matters | Status | Evidence |
|---|---|---|---|
| FFmpeg encoder set (build contract) | Determines whether video encode is even possible in-lib (see §7) | fact | `nm` probe of `libavcodec.a` |
| Native lifecycle / threading | Export adds a long-running worker crossing JNI ↔ FFmpeg ↔ GL/EGL ↔ MediaCodec, like preview's decode worker; attach/detach, cancellation, ref ownership all apply | fact | `video_playback.cpp` worker + `videolib.cpp` JNI bridge |
| Offscreen GL context | Filter needs GL, but `PreviewRenderer` is Surface-bound; export needs a windowless EGL path | fact | `preview_renderer.h:76`; `VideoPreview.kt:213-217` |
| CMake linkage | AMediaCodec/AMediaMuxer require linking the NDK `mediandk` lib (not currently linked) | fact | `CMakeLists.txt:51-68` (no `mediandk`) |
| ABI packaging / 16 KB alignment | New native object must keep the same two-ABI filter and 16 KB max-page-size | fact | `build.gradle.kts:21`; `CMakeLists.txt:10,43-46` |

### Reuse candidates (no reuse decision made)

| Candidate | Location | Apparent fit | Confidence |
|---|---|---|---|
| Decode + `sws_scale`-to-RGBA loop | `video_playback.cpp:528-1095` | Directly reusable as the export **read/decode** stage (trim window, per-segment appearance already threaded through) | high |
| `GlProgram` appearance+filter program | `gl_program.{h,cpp}` | Reusable as the export **effect** stage, but currently drawn to a Surface — needs offscreen target | high |
| `AppearanceSnapshot` / validation | `appearance.h`, `video_playback.cpp:72-124` | Reuse verbatim for export effect parameters | high |
| Timeline sequencing (`runTimeline`) | `video_playback.cpp:1115-1168` | Reference for concatenated multi-segment export ordering | medium |
| Camera `VideoEncoder` (AMediaCodec H.264 + AMediaMuxer MP4) | `camera/src/main/cpp/video/video_encoder.{h,cpp}` | **Pattern/reference only** — different module (`com.chiistudio.camerandk`); do **not** cross-link modules. May inform the new `:videolib` encoder. | medium |

---

## 7. Non-functional / Technical Constraints

- **C-1 [fact] — No in-lib H.264/HEVC software video encoder.** The prebuilt LGPL FFmpeg 7.1 archives
  (`config.sh`: `LICENSE=lgpl`, no `--enable-gpl`) contain **no usable software H.264/HEVC encoder**.
  Symbol probe of `libavcodec.a` shows video encoders limited to `mpeg4`, `msmpeg4v2/v3`, `mjpeg`,
  `h263`/`h263p`, `prores*`, `ffv1`, `rawvideo`, `gif`; H.264/HEVC exist only as `ff_h264_v4l2m2m_encoder`
  / `ff_hevc_v4l2m2m_encoder` (Linux V4L2 M2M wrappers, **not usable on Android**). → **Direct FFmpeg H.264
  encode is not achievable with current binaries.** Resolution: FR-6 uses **Android MediaCodec** for H.264.
- **C-2 [fact] — Muxer & audio codec are available.** `libavformat.a` has `ff_mp4_muxer`/`ff_mov_muxer`;
  `libavcodec.a` has `ff_aac_encoder`+`ff_aac_decoder`; `libavfilter.a` has `ff_af_atempo`+`ff_af_aresample`.
  → FFmpeg can mux MP4 and handle the audio (FR-7) including speed re-timing without a rebuild.
- **C-3 [fact] — Native/build invariants must be preserved.** 16 KB page alignment
  (`CMAKE_ANDROID_PAGE_SIZE 16384`, `-Wl,-z,max-page-size=16384`, arm64 `-Bsymbolic`), C++17, and the
  `arm64-v8a`/`armeabi-v7a`-only ABI filter must be kept; new NDK media libs must link the same way.
- **C-4 [assumption] — Effect parity.** Export must produce visually the **same** result as preview for
  the same parameters (SC-2); the identical GL program should drive both to avoid divergence.
- **C-5 [assumption] — Export is offscreen / headless.** Unlike preview, export should not require an
  on-screen `Surface`; MediaCodec's input surface + an offscreen EGL context is the likely target.
- **C-6 [assumption] — Frame timing for files.** Export must write frames by PTS at the target speed
  (no real-time drop/stretch), unlike preview's wall-clock scheduler.

---

## 8. Open Questions, Assumptions & Conflicts

| Item | Classification | Owner | Consequence |
|---|---|---|---|
| **Resolved Q1** — video codec/container: **H.264/MP4 via MediaCodec** (FFmpeg for decode/filter/mux + AAC). | `[fact:clarification]` | user (answered) | Fixes FR-6; avoids a GPL/libx264 rebuild of vendored binaries. |
| **Resolved Q2** — audio: **include source audio**. | `[fact:clarification]` | user (answered) | Fixes FR-7; pulls audio decode/atempo/AAC-encode into scope. |
| **Q-A** — Does export cover the **multi-segment timeline** (concat of trimmed segments), or only a single clip/segment? | `[unknown:deferrable]` — assumption: mirror preview (both) | solution-design / product | Changes orchestration scope. Best-effort assumption (mirror `playTimeline`) is safe because the per-frame effect logic is identical; confirm in solution-design. |
| **Q-B** — Output destination contract: caller-supplied local path vs URI/`ParcelFileDescriptor`; who owns creation/permissions. | `[unknown:deferrable]` — assumption: caller-supplied local path (matches current local-file-only input) | solution-design / product | Affects the public API signature; does not change core feasibility. |
| **Q-C** — Output resolution/bitrate/fps policy (match source vs configurable). | `[unknown:deferrable]` — assumption: match source dimensions & frame rate | solution-design | Encoder configuration detail; deferrable. |
| **A-1** — Actor is an integrating app developer; `:videolib` is a library with no UI of its own. | `[assumption]` | — | Story framing only. |
| **Conflict-1** — Stage packet says `:videolib` is a bare scaffold; **source shows a full preview pipeline**. Resolved in favor of source (ground rule 0). | `[conflict]` (non-blocking) | — | Documented; source is authoritative. |

No **blocking** gap remains — the two scope-defining unknowns were resolved by clarification; Q-A..Q-C
are deferrable with stated best-effort assumptions.

---

## 9. Risk Analysis

| Risk | Likelihood / Impact | Affected FR / area | Status | Source |
|---|---|---|---|---|
| Assuming FFmpeg can encode H.264 in-lib and hitting the missing-encoder wall late. | Was High / High — **mitigated** by FR-6 (MediaCodec) | FR-1, FR-6 | resolved | `nm` probe C-1 |
| Native lifecycle bugs in a new export worker crossing JNI ↔ FFmpeg ↔ EGL ↔ MediaCodec (attach/detach, cancellation, buffer/ref ownership, stop-before-release). | Med / High | FR-1; native boundary | open | context §11; `video_playback.cpp` worker model |
| Effect divergence between preview (real-time, Surface) and export (offscreen, PTS-paced) — colors/filter or timing not matching. | Med / High | SC-2; FR-2..FR-4 | open | `preview_renderer.h`; `scheduleFrame` `video_playback.cpp:725` |
| A/V desync when speed ≠ 1.0 (video re-timed but audio not, or vice-versa). | Med / Med | FR-7, FR-2 | open | audio atempo constraint C-2 |
| CMake/link regression from adding `mediandk`; 16 KB alignment or ABI packaging broken → load/link failure on a supported ABI. | Low / High | build; C-3 | open | `CMakeLists.txt`; `build.gradle.kts` |
| MPEG-4/ProRes fallback (if MediaCodec unavailable on a device) producing low-compatibility output. | Low / Med | FR-6 | open (out of chosen path, noted) | encoder set C-1 |
| Public-API addition later needs breaking change (external consumers unknown). | Low / Med | §6 contract | open | registry "external consumers unknown" |

---

### Validation

```yaml
validation:
  status: PASS
  failures: []
  warnings:
    - "Stage packet vs source conflict on :videolib maturity — resolved to source (Conflict-1); packet/modules.json is stale for this module."
    - "Deep depth used (native + FFmpeg + build boundary), above the standard cap — justified by the encoder-availability blocking constraint."
  coverage:
    requirements_with_evidence: "7/7"
    stories_mapped: "2/2"
    success_conditions_mapped: "4/4"
    material_unknowns_resolved: "2/2"
```

**Handoff:** validation PASS, `AUTOMATION: CONTINUE`, no blocking gap. Ready for **solution-design**
(`/design`). Deferrable open questions Q-A..Q-C carry stated best-effort assumptions for solution-design
to confirm; every SC-ID remains available for `SC-ID → AC-ID` mapping.
