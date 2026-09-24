AUTOMATION: CONTINUE

# DEV-SPEC — multiple-videos

Preview multiple picked videos as one sequential timeline, where each video contributes a trimmed
A→B segment with its own filter and playback speed.

## 0. Analysis Control

| Field | Value |
|---|---|
| Outcome | **CONTINUE** — validation PASS; the one blocking ambiguity (preview mode) resolved by clarification |
| Kind | feature (`ticket-reading` → `kind: feature`) |
| Scope classification | existing-code extension of `videolib`; cross-boundary (Kotlin public API + JNI/native); demo wiring in `app` |
| Path taken | Full path (existing-code, cross-module, high-risk native boundary) |
| Depth / lookup cap | standard (~15 lookups); honored, no escalation |
| Evidence used | ticket; clarification (1 Q/A); `videolib` Kotlin public API; native `video_playback.{h,cpp}`; `app/MainActivity2`; `videolib/build.gradle.kts`; `CMakeLists.txt`; module packet |
| Interactivity | direct `/study` (interactive); one clarification asked and answered |

**Focus-area applicability**

| # | Focus area | Status |
|---|---|---|
| 1 | Requirements | applicable — §2–§5 |
| 2 | Edge cases | applicable — §5 (explicit) + §5a (proposed) |
| 3 | Feature impact | applicable — §6 (touches existing `videolib` + `app`) |
| 4 | Risk | applicable — §9 |
| 5 | API docs | N/A — no remote/backend/network surface; the feature is on-device FFmpeg playback |
| 6 | Figma/design | N/A — no design source supplied (text-only ticket; no ticket-local Figma cache) |

## 1. Sources

| Source | Type | Location | Read status |
|---|---|---|---|
| Feature ticket | markdown | `output/multiple-videos.md` | read (5 lines, native `.md`, no conversion) |
| Clarification | interactive | this `/study` run | recorded (1 Q/A) |
| `videolib` public API | code | `videolib/src/main/java/com/cii/videolib/*.kt` | read |
| `videolib` native playback | code | `videolib/src/main/cpp/video_playback.{h,cpp}` | read (h full; cpp entry/validation) |
| Demo host | code | `app/src/main/java/com/chiistudio/library/MainActivity2.kt` | read |
| Build/native config | code | `videolib/build.gradle.kts`, `videolib/src/main/cpp/CMakeLists.txt`, `app/build.gradle.kts` | read |

Design index: N/A — no design references.

## 2. Overview & Business Goal

Let a user pick several local videos and preview them together as a single composed sequence. Each
picked video is trimmed to a start→end (A→B) interval and rendered with its own visual filter and
playback speed, so the preview shows the edited result rather than the raw source files.
`[fact: ticket]`

Presentation is a **sequential timeline**: one preview surface plays each trimmed clip back-to-back
in order. `[fact: clarification]`

## 3. Functional Requirements

| FR-ID | Requirement | Status | Evidence/Source |
|---|---|---|---|
| FR-1 | The user can pick multiple videos in one selection flow ("pick many video"). | fact | ticket |
| FR-2 | Each picked video defines a trim interval — a start position A and end position B — and only that A→B interval is previewed. | fact | ticket ("every video can start from special position A to B") |
| FR-3 | Each picked video applies its own visual filter to its previewed segment. | fact | ticket ("every video will apply special filter") |
| FR-4 | Each picked video applies its own playback speed to its previewed segment. | fact | ticket ("… speed") |
| FR-5 | The picked, trimmed, filtered, speed-adjusted videos are previewed together as one sequential timeline: their A→B segments play back-to-back on a single preview surface, in selection order. | fact | clarification |

Note: FR-3 (filter) and FR-4 (speed) already exist as *per-preview* capabilities today (`VideoPreview.setFilter`, `setPlaybackSpeed`); the new requirement is that they become *per-clip within one composed timeline* (see §6). This is a scope observation, not a solution. `[assumption: code]`

## 4. Actors & User Stories

| Story-ID | FR-ID | Story |
|---|---|---|
| ST-1 | FR-1 | As a user, I select multiple videos at once so I can preview them together. |
| ST-2 | FR-2 | As a user, I set a start (A) and end (B) point per video so only the part I want is shown. |
| ST-3 | FR-3, FR-4 | As a user, I choose a filter and a speed per video so each segment looks and plays the way I want. |
| ST-4 | FR-5 | As a user, I watch the trimmed, filtered, sped segments play one after another as a single preview. |

Actor: single on-device end user of a host app that integrates `videolib`. Library manifests are
empty; the host owns picking UI, permissions, and lifecycle. `[fact: code]` (context §6)

## 5. Observable Success Conditions

| SC-ID | FR-ID | Explicit/clarified outcome | Evidence/Source |
|---|---|---|---|
| SC-1 | FR-1 | More than one video can be chosen and all chosen videos are retained for preview. | ticket |
| SC-2 | FR-2 | For each video, only frames within its A→B interval are presented; frames before A and after B are not shown. | ticket |
| SC-3 | FR-3 | The filter configured for a given video is visibly applied to that video's previewed segment. | ticket |
| SC-4 | FR-4 | The configured speed for a given video governs how fast that video's segment plays. | ticket |
| SC-5 | FR-5 | The segments play back-to-back in order on one preview: when one segment's B is reached, the next segment begins from its A. | clarification |

### 5a. Proposed edge cases & boundary behavior (best-effort — every row is an assumption/unknown)

| FR-ID | Edge/boundary case | Expected handling | Status | Source |
|---|---|---|---|---|
| FR-2 | A ≥ B, or A/B outside the video's real duration | Reject or clamp to a valid interval; do not present an empty/negative segment | `[assumption]` | reasoning; today `seekTo` clamps to the playable interval (`VideoPreview.seekTo` doc, video_playback.h `seekTo`) |
| FR-2 | A = B (zero-length interval) | Segment is skipped or shows a single frame | `[unknown]` | not specified by ticket |
| FR-1 | One picked file is unreadable/unsupported mid-list | Whole timeline fails vs. that segment is skipped and the rest continues | `[unknown:deferrable]` | current single-play reports one terminal `PlaybackError` (`PlaybackError.kt`, `PlaybackListener.kt`); multi-clip failure policy is undefined |
| FR-4 | Speed below the existing floor (0.1) | Rejected per existing rule | `[assumption]` | `VideoPreview.setPlaybackSpeed` requires `speed ≥ 0.1` (`MIN_PLAYBACK_SPEED`) |
| FR-5 | Ordering of segments | Selection order | `[assumption]` | not stated; simplest consistent with "pick many" |
| FR-5 | Looping of the whole composed timeline | Out of scope unless requested | `[assumption]` | `setLooping` exists per-preview but ticket is silent on timeline loop |
| FR-1 | Very large / many files copied to cache | Bounded or streamed | `[unknown:deferrable]` | host copies each selection to a temp file today (`MainActivity2.prepareSelectedVideo`); N files multiplies cache/copy cost |

No 5a row is a success condition; none receives an `SC-ID` until confirmed.

## 6. Engineering Evidence — Non-normative

Records what currently constrains the feature. No solution, file, or API is proposed here.

### Module impact hypothesis

| Module | Owner/consumer | Dependency evidence | Likely contract change | Status/confidence |
|---|---|---|---|---|
| `videolib` | primary owner | `com.cii.videolib`; owns Kotlin preview facade + native FFmpeg playback | New public capability: multi-clip **timeline**, per-clip **A→B trim**, per-clip **filter+speed** composed into one sequential preview | fact (owner) / high; contract-shape assumption |
| `app` | consumer | `app/build.gradle.kts:55` `implementation(project(":videolib"))`; `MainActivity2` is the only `com.cii.videolib` consumer in repo | Demo wiring: multi-select picker (today `OpenDocument()` single-select) + per-clip trim/filter/speed controls + timeline preview | fact (edge) / high |
| external consumers | consumer | `videolib` is public with **unknown** external consumers (packet; context §11) | Additive vs. breaking API matters even without in-repo callers | unknown (blocking for API-compat design, not for scope) |

### Verification implications (candidates, non-binding)

| Module/consumer | Candidate check | Reason | Status |
|---|---|---|---|
| `videolib` | `:videolib:assembleDebug` | module default; compiles Kotlin + native | candidate |
| `videolib` (native) | build + on-device smoke on **both** ABIs (`arm64-v8a`, `armeabi-v7a`) | native playback change; only these ABIs have prebuilt FFmpeg (`build.gradle.kts` abiFilters; 16 KB alignment) | candidate |
| `app` | `:app:assembleDebug` | host integration compiles against new API | candidate |
| `videolib` | instrumented playback tests (existing `androidTest` playback suite present) | trim + sequential composition are device/decoder-observable | candidate |

### Entry points (smallest confirmed set)

| Symbol | Role | File:line |
|---|---|---|
| `VideoPreview.play(path, listener)` | Kotlin facade: starts one playback attempt on the attached surface | `videolib/src/main/java/com/cii/videolib/VideoPreview.kt:62` |
| `VideoPreview.seekTo(positionMs)` | seek within the current single video (clamped to playable interval) | `VideoPreview.kt:186` |
| `VideoPreview.setFilter` / `setPlaybackSpeed` | per-preview filter and speed (exist today) | `VideoPreview.kt:182`, `:124` |
| `VideoPlayback` (native) | "Per-VideoPreview native owner … coordinates exactly **one** playback attempt" | `videolib/src/main/cpp/video_playback.h:44-48` |
| `MainActivity2.prepareSelectedVideo` / `pickVideo` | demo single-video pick → copy → play | `app/.../MainActivity2.kt:76`, `:191` |

### Current behavior (summarized)

| Behavior | Status | Evidence/Source |
|---|---|---|
| One `VideoPreview` renders **one** video onto **one** `Surface`; native owner allows exactly one active attempt. | fact | `VideoPreview.kt` (single `nativeHandle`, single `activeAttemptId`); `video_playback.h:44-48` |
| No trim/interval (A→B) concept exists. `seekTo` clamps to the full playable interval; there are no in/out points and no segment boundaries. | fact | `VideoPreview.seekTo` doc `:185-190`; `video_playback.h:80` `seekTo(int64_t positionMs)` |
| Filter and speed are **per-preview**, not per-segment; changing filter requires an attached surface. | fact | `VideoPreview.setFilter`/`setAppearance` (`SURFACE_UNAVAILABLE` path `:135-140`); `setPlaybackSpeed` floor 0.1 `:124-128` |
| Host picks a **single** video (`OpenDocument()`), copies it to a temp cache file, then plays. | fact | `MainActivity2.kt:76`, `:191-267` |
| Native playback is a single-worker state machine (`Idle…Playing…Completed/Failed`); FFmpeg decode off the JNI/EGL threads. | fact | `video_playback.h:19-129`; `video_playback.cpp:1-70` |
| Native library `libvideolib.so`; ABI-filtered to `arm64-v8a`/`armeabi-v7a`; 16 KB page-aligned; FFmpeg 7.1 static archives. | fact | `CMakeLists.txt` (`add_library videolib SHARED … video_playback.cpp`); `build.gradle.kts:9-33` |

### Affected boundaries (confirmed only)

| Boundary | Why it matters | Status | Evidence/Source |
|---|---|---|---|
| `videolib` public Kotlin API | New multi-clip/trim capability changes the consumer contract; external consumers unknown → additive vs breaking is a real decision | fact | packet (public; external consumers unknown); context §11 |
| JNI ↔ native playback | Timeline + trim likely require new native semantics beyond "exactly one attempt"; JNI signatures/ownership are a cross-boundary contract | fact | `video_playback.h:44-48`; context §0, §11 |
| Native lifecycle/threading | Single-worker, single-attempt model; sequencing N segments touches state, cancellation, thread affinity | fact | `video_playback.h:107-129`; context §11 |
| ABI packaging | Native changes must build/link on both prebuilt-FFmpeg ABIs; 16 KB alignment invariant | fact | `build.gradle.kts:18-33` |
| `app` picker/UI | Single-select picker + single-video state must become multi-select + per-clip config | fact | `MainActivity2.kt:76` |

### Reuse candidates (no reuse decision)

| Candidate | Location | Apparent fit | Confidence |
|---|---|---|---|
| `VideoPreview` play/seek/filter/speed facade | `VideoPreview.kt` | Existing single-clip primitives a timeline could compose | medium |
| Native `VideoPlayback` decode/seek pipeline | `video_playback.{h,cpp}` | Existing FFmpeg decode + `seekTo` is the basis for A→B interval decode | medium |
| `VideoFilter` + `VideoAppearance` (per-clip look) | `VideoFilter.kt`, `VideoAppearance.kt` | Per-clip filter is expressible with the existing filter/appearance model | medium |
| Host copy-to-cache + duration read | `MainActivity2.prepareSelectedVideo`, `readVideoDuration` | Reusable per-file for a multi-pick flow | medium |

## 7. Non-functional / Technical Constraints

- Native/ABI: builds must link and run on `arm64-v8a` and `armeabi-v7a` only (prebuilt FFmpeg); preserve 16 KB page alignment. `[fact: code]` (`build.gradle.kts`)
- `videolib` `minSdk = 21` (lower than the workspace default 24). `[fact: code]`
- Playback speed floor is `0.1` in the current contract; per-clip speed should respect it unless redesigned. `[fact: code]`
- Filter source is validated/trusted GLES 3.0 (`addFilter(vec4, vec2)` entry point, reserved-token checks); per-clip filters inherit this contract. `[fact: code]` (`VideoPreview.validate`, `VideoFilter.kt`)
- Not thread-safe: `VideoPreview` must be driven from a single owner thread; GL/EGL work is marshalled internally. `[fact: code]` (`VideoPreview` class doc)
- API docs: N/A — no remote/network/auth surface.

## 8. Open Questions, Assumptions & Conflicts

| Item | Classification | Owner | Consequence |
|---|---|---|---|
| Preview mode = sequential timeline (vs simultaneous grid) | **resolved** `[fact:clarification]` | user | Decided; drives FR-5 and native design |
| How A and B are expressed (ms, frames, fractions) and precision (keyframe-accurate vs exact) | `[unknown: deferrable]` | product + design | Affects trim UX and native seek accuracy; solution-design can pick a defensible default |
| Failure policy when one segment in the timeline errors (fail all vs skip-and-continue) | `[unknown: deferrable]` | product | Affects `PlaybackListener` contract for multi-clip |
| Audio: is audio in scope, or is preview video-only? | `[unknown: deferrable]` | product | Current pipeline is **video-only** (`VideoPreview.play` doc); per-clip speed + audio pitch is a larger scope if added |
| Transitions/gaps between segments | `[assumption: none]` | product | Assumed hard cuts, no crossfade, unless requested |
| Export/save of the composed result | `[assumption: out of scope]` | product | Ticket says "preview"; no encode/mux requirement stated. Note: `videolib` has no recording/muxer path today |
| Maximum number of videos / total duration | `[unknown: non-material]` | product | Bounds cache/memory; not scope-blocking |
| Per-clip filter+speed became per-segment (was per-preview) | `[assumption: code]` | — | Interpretation of ticket vs current API shape; labelled, not asserted as requirement |

No unresolved **material** conflict. No blocking gap remains → CONTINUE.

## 9. Risk Analysis

| Risk | Likelihood/Impact | Affected FR/area | Status | Source |
|---|---|---|---|---|
| Native model change: "exactly one playback attempt" must become an ordered multi-segment sequence — state machine, cancellation, and worker lifecycle are the documented high-risk native surface. | med / high | FR-5, native boundary | fact | `video_playback.h:44-48,107-129`; context §11 |
| Trim (A→B) is new native decode/seek-interval logic; FFmpeg seek accuracy and keyframe boundaries can make A/B imprecise. | med / med | FR-2 | assumption | `video_playback.h:80` `seekTo`; `video_playback.cpp` decode path |
| Public API compatibility: `videolib` has unknown external consumers; a breaking change to the preview contract can break them silently. | med / high | `videolib` public API | fact | packet; context §11 |
| Per-clip filter/speed switching mid-timeline: appearance apply needs an attached surface and can reject (`SURFACE_UNAVAILABLE`), and filter changes trigger GL program work. | med / med | FR-3, FR-4 | fact | `VideoPreview.setAppearance` `:131-159`; `AppearanceRejectionReason` |
| ABI/native build breakage: changes must link on both prebuilt-FFmpeg ABIs and keep 16 KB alignment. | low / high | build/native | fact | `build.gradle.kts:18-33` |
| Resource pressure: multi-pick copies N files to cache and decodes trimmed segments; memory/IO scales with N. | med / med | FR-1 | assumption | `MainActivity2.prepareSelectedVideo` copy path |
| Surface teardown/cleanup: repo notes native surface cleanup is a known weak spot in the camera sibling; verify `videolib` teardown across segment transitions. | low / med | native lifecycle | assumption | context §6, §11 (native cleanup caution) |

---

### Validation

```yaml
validation:
  status: PASS
  failures: []
  warnings:
    - "API-compat obligation depends on unknown external consumers of videolib (recorded as unknown; not scope-blocking)"
    - "Several deferrable unknowns (A/B units, per-segment failure policy, audio) carried as labelled assumptions for solution-design"
  coverage:
    requirements_with_evidence: "5/5"
    stories_mapped: "4/4"
    success_conditions_mapped: "5/5"
    material_unknowns_resolved: "1/1"
```

Handoff: eligible for **solution-design** (`AUTOMATION: CONTINUE`, validation PASS, no blocking gap).
