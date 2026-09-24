AUTOMATION: CONTINUE

# SOLUTION-DESIGN — multiple-videos

Design a **sequential video timeline** in `videolib`: one preview surface plays an ordered list of
trimmed (A→B) segments, each with its own filter and speed, back-to-back, reporting one terminal
outcome. The design **extends the existing single-attempt native playback seam** rather than adding a
parallel decoder, and keeps the current single-video public API working unchanged (additive, non-
breaking).

Traceability preserved: `FR-ID → SC-ID → AC-ID`, plus `Story-ID`. Source: `DEV-SPEC.md`.

---

## 1. Decision ledger

**Investigation:** depth = **deep**; ~14 lookups of a 20 cap. Evidence: `video_playback.{h,cpp}`
(full decode loop), `VideoPreview.kt`, `VideoFilter/VideoAppearance/PlaybackListener/PlaybackError/
AppearanceUpdateResult.kt`, `MainActivity2.kt`, `videolib/build.gradle.kts`, `CMakeLists.txt`,
`app/build.gradle.kts`, module packet. Graph unavailable/empty → source inspection used throughout.

| # | Item | Status | Impact |
|---|---|---|---|
| D-1 | **Trim (A→B), per-segment speed, and segment sequencing are a NATIVE playback contract, not Kotlin orchestration.** The decode loop already owns per-frame `mediaUs` (PTS relative to stream start), the wall-clock speed scheduler, seek/skip-to-target, pause, and appearance-apply. Kotlin exposes only a wall-clock **estimate** of position, so it cannot enforce a precise B boundary. | decision (observed evidence) | `video_playback.cpp:646-819` (scheduler+speed), `:847-858` (skip-until-target = A), `:610-614` (duration clamp = B analog), `:939-974` (EOF/loop = segment-advance analog); `MainActivity2` progress is `SystemClock` estimate |
| D-2 | **Extend the existing `VideoPlayback` single-attempt owner to accept an ordered segment list (one worker, one clock, one terminal outcome).** A 1-segment untrimmed timeline is the current behavior as a degenerate case. Rejected alternative: Kotlin-orchestrated playlist that calls `play()` per clip on `onPlaybackCompleted`. | decision | Rejected because trim must be native anyway (D-1); per-`play()` decoder re-init (`avformat_open_input`+`find_stream_info`+seek, `:507-533`) adds an inter-clip stall, and N attempts fragment one logical timeline into N terminal callbacks the facade must suppress. Extending the seam keeps one coherent contract (agent rule: extend a proven seam over a parallel abstraction). |
| D-3 | **New public timeline API is ADDITIVE; existing `play(path, listener)` and all single-clip controls are preserved unchanged.** `videolib` is public with unknown external consumers. | decision | packet (public; external consumers unknown); context §11. No source/binary break. |
| D-4 | Preview is **sequential** on the **single existing surface**; no N-surface/N-decoder concurrency. | decision (clarification) | DEV-SPEC §0/§5 `[fact:clarification]` |
| D-5 | A and B expressed in **milliseconds** (consistent with `seekTo(positionMs)` and ms durations); segment start is frame-accurate via existing keyframe-backward seek + skip-until-A; B is an exclusive PTS upper bound `[A, B)`. | proposed | `VideoPreview.seekTo(positionMs)`, `resetDecoder` clamp `:602-644`; DEV-SPEC 5a A/B-units unknown resolved as design choice |
| D-6 | **Per-segment failure policy = fail-fast:** a segment that cannot open/decode ends the whole timeline with an error that identifies the failing segment index; the current terminal-error contract is reused. Skip-and-continue is a richer policy deferred to a later iteration. | proposed | Matches existing single-attempt terminal-error shape (`finishAttempt`, `PlaybackError`); DEV-SPEC 5a/§8 failure-policy unknown |
| D-7 | Timeline is **video-only** (no audio), matching the current pipeline. | proposed | `VideoPreview.play` doc "video-only"; DEV-SPEC §8 audio unknown |
| D-8 | **Export/encode/mux is OUT OF SCOPE** (ticket says *preview*). `videolib` has no muxer/encoder path today. | out-of-scope | DEV-SPEC §8; no recording path in `videolib` source |
| D-9 | Segment **order = user selection order**; timeline **loop** (via existing looping control) = restart from segment 0. | proposed | DEV-SPEC 5a ordering/loop assumptions |
| D-10 | Host (`app/MainActivity2`) is demo wiring: multi-select pick, per-clip trim/filter/speed capture, drive the timeline API, show cross-segment progress. Host owns picking UX, cache files, permissions, lifecycle. | decision | `app` is demonstration code (context §2, §6); `MainActivity2` currently single-picks `OpenDocument()` |
| B-? | **No blockers.** All open items (D-5..D-9) are new first-party internal contracts the architect decides here; none is an unresolved *external* contract, destructive migration, or public-API break. | — | → `AUTOMATION: CONTINUE` |

Explicitly implementation-local (not decided here): inter-segment pre-open/prefetch optimization,
maximum segment count guard, exact seek-accuracy tuning, thread/executor mechanics, JNI marshalling
shape, cache-file management details.

---

## 2. Behavior and state transitions

### Behavior contract

| FR | SC | AC | Story | Rule / trigger | Observable outcome | Failure / recovery |
|---|---|---|---|---|---|---|
| FR-1 | SC-1 | AC-1 | ST-1 | Host supplies an ordered list of ≥1 segment descriptors to the timeline API | All supplied segments are retained and previewed in order | Empty list or no valid surface → request rejected (no attempt started) |
| FR-2 | SC-2 | AC-2 | ST-2 | Segment has start A | Presentation of that segment begins at A (frames before A not shown) | A invalid (see §5 invariants) → segment/timeline rejected before start |
| FR-2 | SC-2 | AC-3 | ST-2 | Segment has end B | Presentation of that segment stops at B (frames at/after B not shown); timeline advances | B ≤ A or B beyond real duration → rejected/clamped per §5 |
| FR-3 | SC-3 | AC-4 | ST-3 | Segment carries its own filter/appearance | That filter is visibly applied only to that segment's frames | Invalid filter (existing appearance validation) → timeline rejected before start |
| FR-4 | SC-4 | AC-5 | ST-3 | Segment carries its own speed (≥ 0.1) | That speed governs only that segment's playback rate | speed < 0.1 or non-finite → rejected (existing rule) |
| FR-5 | SC-5 | AC-6 | ST-4 | A segment reaches its B | The next segment begins from its A with its own filter/speed applied, on the same surface | Mid-timeline decode/render failure → fail-fast terminal error (D-6) |
| FR-5 | SC-5 | AC-7 | ST-4 | Multiple segments queued | Segments play back-to-back in selection order; exactly one terminal completion for the whole timeline | Surface lost mid-timeline → terminal render error, timeline ends |
| FR-1..5 | — | AC-8 | — | Existing single-clip `play(path)` still invoked | Unchanged single-video behavior (degenerate 1-segment, untrimmed) | — (compatibility guarantee, D-3) |

### State model (timeline attempt — semantic, not framework state)

| State | Meaning / invariants | Permitted events | Prohibited / ignored |
|---|---|---|---|
| Idle | No timeline active; surface may be attached | startTimeline, attach/detach surface, release | pause/resume/seek ignored |
| Starting | Timeline accepted; first segment opening | (internal) → SegmentPlaying / Failed | second startTimeline rejected |
| SegmentPlaying | Segment *i* presenting within [A_i, B_i) at speed_i with appearance_i | pause, seek-within-segment, stop, (internal) segment-boundary, surface-lost | startTimeline rejected |
| SegmentPaused | Segment *i* paused after last presented frame | resume, seek-within-segment, stop | boundary advance frozen until resume |
| Advancing | Segment *i* reached B_i; segment *i+1* being prepared | (internal) → SegmentPlaying / Completed / Failed | external control coalesced |
| Completed | Last segment finished; one terminal completion emitted | startTimeline (new), release | pause/resume/seek ignored |
| Failed | A segment failed (fail-fast) or surface lost; one terminal error emitted (with segment index for decode/open failures) | startTimeline (new), release | — |
| Released | Native owner released; inert | — | all ignored |

### Transition contract

| From | Event / precondition | To | Side effect | Failure / cancellation / recovery |
|---|---|---|---|---|
| Idle | startTimeline(list) ∧ surfaceReady ∧ list valid | Starting | worker begins; segment 0 opened | invalid list / no surface → stay Idle, reject |
| Starting | segment 0 opened, first frame at ≥ A_0 presented | SegmentPlaying(0) | apply appearance_0 + speed_0; clock anchored | open/decode fail → Failed(err, seg 0) |
| SegmentPlaying(i) | present frame with mediaUs ≥ B_i | Advancing(i→i+1) | (i+1 exists) prepare next; else finalize | render fail → Failed(Render); surface lost → Failed(Render) |
| Advancing(i→i+1) | segment i+1 opened, boundary crossed | SegmentPlaying(i+1) | apply appearance_{i+1} + speed_{i+1}; re-anchor clock; seek to A_{i+1}, skip-until-A | open/decode fail → Failed(err, seg i+1) |
| SegmentPlaying(last) | present frame with mediaUs ≥ B_last (or EOF ≤ B) | Completed | emit single onTimelineCompleted | — |
| Completed | looping enabled (D-9) | Advancing(→0) | restart at segment 0 | — |
| SegmentPlaying(i) | pause | SegmentPaused(i) | freeze clock after current frame | ignored if not playing |
| SegmentPaused(i) | resume | SegmentPlaying(i) | re-anchor clock | — |
| SegmentPlaying/Paused(i) | seek(posMs) within [A_i,B_i) | SegmentPlaying/Paused(i) | clamp to segment interval, reset decoder to target | out-of-range → clamp to [A_i, B_i) |
| any active | stop / detachSurface / release | Idle / Failed / Released | cancel worker, join, release renderer | idempotent; no terminal callback after stop/release |

Notes: seek semantics are **intra-segment** (clamped to the active segment's [A,B)); cross-segment
scrubbing is not required by any SC and is intentionally unspecified. Pause/seek reuse the existing
per-attempt scheduler machinery.

---

## 3. Components and responsibilities

### Module Contract Matrix

| Module | Owner/consumer | Responsibility | Depends on | Crossed contract | Compatibility obligation | Verification obligation |
|---|---|---|---|---|---|---|
| `videolib` | **owner** | Public timeline API + native ordered-segment playback (trim/speed/appearance per segment) | FFmpeg 7.1 static, EGL/GLES renderer | Kotlin public API; JNI signatures; native lifecycle/threading; ABI packaging | **Additive** to public API (D-3); JNI both-sides in sync; keep ARM `abiFilters` + 16 KB alignment | `:videolib:assembleDebug`; on-device smoke on `arm64-v8a` **and** `armeabi-v7a`; instrumented timeline/trim tests |
| `app` | consumer | Demo: multi-pick, per-clip trim/filter/speed capture, drive timeline, cross-segment progress | `videolib` (`project(":videolib")`) | UI/state; consumes new public API | Compiles against additive API; no host contract exported | `:app:assembleDebug`; manual multi-video preview run |
| external consumers | consumer (unknown) | — | `videolib` public API | Public API surface | Unaffected because change is additive (D-3) | N/A in-repo; compatibility preserved by construction |

### Components

| Component (semantic role) | Observed / proposed | Responsibility / owned state | Delegates to | Dependency direction | Must not own/know | Evidence / decision |
|---|---|---|---|---|---|---|
| Timeline preview facade | proposed (extends observed `VideoPreview`) | Accept an ordered segment list + one timeline listener; validate each segment's appearance/speed/interval using existing rules; marshal terminal event to main thread | native timeline owner (JNI) | Kotlin → JNI | FFmpeg, EGL, threads, host UI | `VideoPreview.kt` play/validate/callback seam extended additively (D-2,D-3) |
| Segment descriptor | proposed | Immutable value: path, startMs (A), endMs (B), speed, appearance (filter+adjustments) | — | held by facade + JNI | presentation/timing | Composes existing `VideoAppearance`/`VideoFilter` (DEV-SPEC reuse) |
| Timeline listener | proposed (extends observed `PlaybackListener`) | Receive one terminal outcome for the whole timeline (completed / error+segment index) | — | native → Kotlin (main thread) | intra-timeline frame events | `PlaybackListener`/`PlaybackError` shape reused (D-6) |
| Native timeline playback owner | proposed (extends observed `VideoPlayback`) | One worker sequences segments; per segment: seek-to-A, present [A,B) at speed_i with appearance_i, advance at B; one terminal callback; owns state machine (§2) | preview renderer, FFmpeg decode | JNI → native | Kotlin/UI specifics | `video_playback.{h,cpp}` single-attempt owner generalized to N ordered segments (D-1,D-2) |
| Preview renderer (unchanged) | observed | Surface/EGL/GLES; `applyAppearance`, `pushFrame`, `surfaceAvailable`, `releaseSurface` | EGL thread | native → GL thread | decode/timeline sequencing | `video_playback.cpp` renderer_ calls; per-segment filter = `applyAppearance` at boundary |
| Host timeline controller | proposed (in `app` demo) | Multi-pick videos, capture per-clip A/B/filter/speed, copy to cache, drive facade, render cross-segment progress | timeline preview facade | app → videolib | native/decode internals | `MainActivity2` upgraded from single `OpenDocument()` (D-10) |

Decomposition is minimal: it reuses the renderer, appearance model, filter validation, scheduler,
and terminal-callback marshalling that already exist; only the **segment list + boundary/advance**
behavior is new.

---

## 4. End-to-end data flow

### Flow A — Start a multi-segment timeline (FR-1..5; AC-1..7)

| Step | Participant | Input / source | Decision / transformation | Output / side effect | Error propagation |
|---|---|---|---|---|---|
| 1 | Host controller | User multi-picks N videos | Copy each to a readable local cache file; capture per-clip A/B, filter, speed | Ordered list of segment descriptors | Copy/read failure → host surfaces error, segment omitted or timeline aborted (host policy) |
| 2 | Timeline facade | Segment list + surface-attached | Validate each: appearance ranges + filter source (existing rules), speed ≥ 0.1, 0 ≤ A < B ≤ real duration | Accept → hand list to native; else reject with reason | Any invalid segment → whole request rejected before any frame (AC-2..5 failure rows) |
| 3 | Native timeline owner | Accepted list | Start worker; open segment 0; seek to A_0 (keyframe-backward), skip-until-A_0 | State → SegmentPlaying(0); apply appearance_0 + speed_0 | Open/decode fail → Failed(err, seg 0) → single terminal error |
| 4 | Native worker (per frame) | Decoded frame PTS → `mediaUs` | If mediaUs < A_i skip; if mediaUs ≥ B_i end segment; else schedule at speed_i | Present RGBA frame via renderer (appearance_i already applied) | Render fail / surface lost → Failed(Render) |
| 5 | Native worker (at B_i) | Segment i exhausted | If i+1 exists → Advancing: open i+1, apply appearance_{i+1}+speed_{i+1}, seek A_{i+1}; else finalize | Continuous back-to-back presentation on same surface (AC-6,7) | Next-segment open fail → Failed(err, seg i+1) |
| 6 | Native → facade → host | Last segment past B_last / EOF | Terminal outcome computed once | onTimelineCompleted on main thread; host updates UI | Fail-fast error → onTimelineError(error, segmentIndex) once |

### Flow B — Single-clip playback (AC-8, compatibility)

Existing `play(path, listener)` path is unchanged: 1 untrimmed segment, natural EOF, existing
`PlaybackListener`. No new behavior; guarantees additive compatibility (D-3). Reference §2 states.

Source of truth: the **native timeline owner** owns playback position/segment index; the host holds
only a display estimate (as today). Ordering is selection order (D-9). No persistence, no offline
concerns (local files only).

---

## 5. Boundary contracts

| Contract / boundary | Observed/Proposed | Semantic input | Output / result | Invariants | Errors | Compatibility / versioning | Owner |
|---|---|---|---|---|---|---|---|
| Host → timeline facade: **startTimeline** | proposed | Ordered list of segment descriptors (path, A_ms, B_ms, speed, appearance) + timeline listener | Accepted (true) → exactly one terminal callback later; or rejected (false/reason) with no attempt | Surface attached; list non-empty; each: 0 ≤ A < B ≤ real duration; speed ≥ 0.1 finite; appearance passes existing validation; only one active timeline | Reject reasons reuse existing appearance/`SURFACE_UNAVAILABLE`/`RELEASED` set + new invalid-interval reason | Additive; existing `play` unchanged (D-3) | `videolib` |
| Facade → native: **JNI timeline entry** | proposed | Marshalled segment array + per-segment appearance/filter/textures + intervals + speeds | attemptId (0 = rejected) | JNI method name/signature identical both sides; direct buffers for textures; both sides changed together | UnsatisfiedLinkError if signatures diverge (build-time discipline) | New JNI symbol; existing JNI entries unchanged | `videolib` |
| Native → Kotlin: **terminal callback** | proposed (extends observed) | attemptId, optional error code, optional segment index | onTimelineCompleted() or onTimelineError(error, index), once, main thread | Exactly one terminal event per accepted timeline; none after stop/release; cross-thread attach/detach correct | Error codes reuse `PlaybackError` categories (D-6) | Additive to `PlaybackListener` family | `videolib` |
| Per-segment appearance apply | observed | `VideoAppearance` (filter+adjustments) | Accepted/Rejected | Applied on GL thread while context current; filter change needs surface | Existing `AppearanceRejectionReason` set | Unchanged renderer contract | `videolib` (renderer) |
| Trim interval semantics | proposed | A_ms, B_ms | Frames presented iff A ≤ mediaUs < B | A frame-accurate via seek+skip; B exclusive PTS bound; ms units (D-5) | Invalid interval → reject | New internal contract | `videolib` |
| Native library load / ABI | observed | `System.loadLibrary("videolib")`; `arm64-v8a`,`armeabi-v7a`; 16 KB aligned; FFmpeg 7.1 static | Loadable native lib on supported ABIs | Keep `abiFilters` + page alignment; add sources to existing `videolib` target (single shared lib) | Link/soname/ABI failure at build/load | Packaging unchanged | `videolib` build/CMake |

No external/backend contract is involved (API docs N/A). All new contracts are first-party.

---

## 6. Conditional cross-cutting design

**Native concurrency & lifecycle (high-risk — DEV-SPEC §9 top risk).** The single-attempt model
("coordinates exactly one playback attempt") becomes an ordered-segment sequence within **one**
worker and one clock. Preserve the existing invariants: cancellation via `cancelRequested_` +
`controlVersion_`, stop/detach/release must cancel-then-join the worker before releasing the
renderer, terminal event claimed exactly once (`terminalClaimed_`), no callback after stop/release,
and GL/appearance work only on the render thread with the context current. Segment transitions must
not present a frame from segment i+1 before its appearance_{i+1} is applied (ordering invariant), and
must re-anchor the wall clock and reset per-segment `mediaUs`/speed at each boundary (mirrors the
existing loop-restart reset at `:967-973`). `[native-boundary-guideline, ndk-cpp-guideline,
opengles-guideline]`

**FFmpeg specifics.** Trim reuses `av_seek_frame(AVSEEK_FLAG_BACKWARD)` + flush + skip-until-target
(A) and a PTS upper-bound check (B), analogous to the existing duration clamp and seek-skip.
`SwsContext` stays confined to the worker thread (do not share across threads); RGBA is the GL
upload source. Note the guideline's version pins describe `camera`'s FFmpeg **3.2.12 shared**;
`videolib` uses **7.1 static** — the AVFrame-ownership/swscale-threading *principles* apply, the
soname/version pins do not. Do not add `libavcodec`-based muxing (export is out of scope, D-8).
`[ffmpeg-guideline — principles only]`

**ABI / build.** Add new native source to the existing single `videolib` shared target; keep the
two ARM `abiFilters` and 16 KB page alignment. No vendored-tree or packaging change.

**Compatibility & rollout.** Additive public API (D-3); existing single-clip consumers unaffected.
No source/binary/navigation/persistence break. `videolib` remains `minSdk 21`.

**Performance.** Inter-segment decoder init (`avformat_open_input`+`find_stream_info`+seek) can cause
a brief stall at each boundary; a pre-open/prefetch of the next segment is an allowed but
**intentionally unspecified** optimization, not an architectural requirement. Max segment count is a
host-side guard (resource risk, DEV-SPEC §9), left implementation-local.

**Design conformance.** N/A — no design source supplied (metadata-only status is not applicable;
there is no Figma/style capture to block on).

**Verification obligations.** Native/Gradle compile proves JNI signatures, linkage, soname
resolution, packaging. Only a **supported-device run on each ARM ABI** proves trim accuracy, per-
segment filter/speed, back-to-back transition continuity, and single-terminal-callback behavior —
never claim these from compilation alone.

---

## 7. Coverage audit

| Requirement / question | Resolved by |
|---|---|
| FR-1 pick many / retain | AC-1; Flow A steps 1–2; startTimeline contract |
| FR-2 A→B trim | AC-2, AC-3; trim-interval contract; §6 FFmpeg; D-1, D-5 |
| FR-3 per-segment filter | AC-4; per-segment appearance apply; §2 Advancing transition |
| FR-4 per-segment speed | AC-5; scheduler reuse; §2 transitions |
| FR-5 sequential back-to-back | AC-6, AC-7; §2 state model; Flow A steps 4–6; D-2, D-4 |
| Single-clip compatibility | AC-8; Flow B; D-3 |
| Q: preview mode | D-4 (sequential, resolved by clarification) |
| Q: A/B units & precision | D-5 (ms; A frame-accurate, B exclusive) |
| Q: per-segment failure policy | D-6 (fail-fast + segment index) |
| Q: audio | D-7 (video-only) |
| Q: export | D-8 (out of scope) |
| Q: ordering / timeline loop | D-9 (selection order; loop → segment 0) |
| Q: max count / prefetch | §6 (intentionally unspecified, host/impl-local) |
| Public-API compat (unknown external consumers) | D-3 additive; §5 contracts; §6 rollout |

Unresolved inputs needed to complete the design: **none.** No blocker → `AUTOMATION: CONTINUE`.

Design complete. Traceability `FR → SC → AC → Story` preserved for downstream mapping.
