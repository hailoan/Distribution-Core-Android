# RESUME-STATE — multiple-videos (/implement, android-dev)

> Working handoff note to resume the Android Dev stage next session. Not an AI-DLC pipeline
> artifact. The pipeline artifact for this stage is `CHANGESET.md` (not yet written).

## Feature

Sequential video timeline in `videolib`: pick many videos, each trimmed A→B with its own filter +
speed, played back-to-back on one preview surface. Branch: `feature/video`.

## Pipeline status (all under `output/multiple-videos/`)

| Stage | Artifact | Status |
|---|---|---|
| Feature Analysis | `DEV-SPEC.md` | done — `AUTOMATION: CONTINUE` |
| Solution Design | `SOLUTION-DESIGN.md` | done — `AUTOMATION: CONTINUE` |
| Implementation Plan | `IMPLEMENT-PLAN.md` | done — `AUTOMATION: CONTINUE` |
| Android Dev | `CHANGESET.md` | **NOT written** — write at end of /implement, start with `AUTOMATION: CONTINUE` |

## Core design (do not re-decide)

- **Extend** the existing single-attempt native `VideoPlayback` seam into an ordered-segment engine;
  add **additive** Kotlin API `VideoPreview.playTimeline`. No public-API break.
- Single-clip `play()` path must stay **byte-for-byte observable-equivalent** (AC-8).
- ms A/B units; `[A,B)` exclusive B; fail-fast carrying segment index; video-only; export out of
  scope; selection order; whole-timeline loop → segment 0 (segments do not loop individually).
- FFmpeg here is **7.1 static** (not camera's 3.2.12 shared); AVFrame/swscale principles apply, not
  the version pins. No CMake/ABI/16 KB change (engine extends existing sources).
- JNI is **hand-mangled 1:1** — every `Java_com_cii_videolib_VideoPreview_native*` export must match
  a Kotlin `external fun` name exactly (mismatch = runtime `UnsatisfiedLinkError`, not compile error).

## Task DAG / waves (IMPLEMENT-PLAN §6)

Wave0 **P1 ‖ P3** → **P2** → **P4** → (**ANDT-TL ‖ P5**) → **P6** → **IT-BUILD** →
(**IT-APP ‖ IT-DEVICE**).

android-dev tasks: P1, P3, P2, P4, P5, P6. testing handoff: UT-SEG, ANDT-TL. integration handoff:
IT-BUILD, IT-APP, IT-DEVICE.

## Where we stopped — mid-P1 (native engine)

Files: `videolib/src/main/cpp/video_playback.{h,cpp}`. All edits already committed by the session
auto-commit hook to `2cb85e9 "multiple videos"` (working tree clean; verify with
`git show HEAD:videolib/src/main/cpp/video_playback.h | grep PlaybackKind`).

### P1 edits already applied

- `video_playback.h`: added `PlaybackKind{Single,Timeline}`; `TimelineSegment{path,startMs,endMs,
  speed,appearance}`; changed `PlaybackTerminalCallback` → `(uint64_t, PlaybackKind,
  optional<PlaybackErrorCode>, int)`; added `playTimeline(vector<TimelineSegment>)`; `<vector>`
  include; parameterized `decodeAttempt(attemptId, path, startMs, endMs, speed, appearance,
  honorLooping)`; `runTimeline` decl; `finishAttempt(+PlaybackKind kind, +int segmentIndex)`;
  `currentKind_` member.
- `video_playback.cpp`: `releaseSurface` captures `failedKind` under lock; `play()` sets
  `currentKind_ = Single`; added `playTimeline()` impl; updated `decodeAttempt` signature; applied
  per-segment appearance (`renderer_.applyAppearance` under rendererMutex_, before first frame) +
  speed after `markPlaying`.

### P1 remaining (next session, in order)

1. **Enforce A** — seek to `startMs` at decode start (reuse `resetDecoder`/skip-until-target). Note
   the loop already skips frames with `seeking && mediaUs < seekTargetUs` (~line 847 pre-edit).
2. **Enforce B** — clamp effective playable end to `endMs`; end the segment when `mediaUs ≥ endMs`
   (`endMs < 0` = natural EOF). Mirror the existing `durationUs` last-playable clamp in
   `resetDecoder`.
3. **Gate looping** with `honorLooping` at the EOF/loop block (segments must NOT loop individually;
   only the whole timeline loops → segment 0).
4. **`runAttempt`** — call `decodeAttempt(attemptId, path, /*startMs*/0, /*endMs*/-1, /*speed*/0.0,
   /*appearance*/nullopt, /*honorLooping*/true)` then `finishAttempt(attemptId, Single, error, -1)`.
   Defaults must reproduce today's single-clip behavior exactly.
5. **Implement `runTimeline`** — iterate segments; per segment call `decodeAttempt` with its
   window/speed/appearance and `honorLooping=false`; advance on natural completion; on a segment
   error emit ONE `finishAttempt(attemptId, Timeline, error, i)`; on all-complete emit ONE
   `finishAttempt(attemptId, Timeline, nullopt, -1)`. One worker, one clock, single terminal claim.
6. **Update `finishAttempt` body** to accept and route `kind` + `segmentIndex` to the callback.

## Then continue the DAG

- **P3** `videolib/src/main/java/com/cii/videolib/VideoSegment.kt` (immutable: path, startMs, endMs,
  speed, appearance) + `TimelineListener.kt` (`onTimelineCompleted()` /
  `onTimelineError(error: PlaybackError, segmentIndex: Int)`).
- **P2** `videolib/src/main/cpp/videolib.cpp`: add
  `Java_com_cii_videolib_VideoPreview_nativePlayTimeline`; extend `PlaybackJniBridge` to resolve +
  call `onNativeTimelineCompleted(J)V` / `onNativeTimelineError(JII)V` (attemptId, errorCode,
  segmentIndex) and route by `PlaybackKind`. Single-clip callbacks
  (`onNativePlaybackCompleted(J)V` / `onNativePlaybackError(JI)V`) unchanged; index `-1`.
- **P4** `videolib/src/main/java/com/cii/videolib/VideoPreview.kt`: add `playTimeline(segments,
  listener)`; `external fun nativePlayTimeline(...)`; `@Keep onNativeTimelineCompleted` /
  `onNativeTimelineError`; per-segment validation reusing `validate()` plus `0 ≤ A < B` and
  `speed ≥ MIN_PLAYBACK_SPEED`; reject empty list / no surface; marshal terminal event to main
  thread via existing `callbackHandler`. Additive — leave `play`/`stop`/`pause`/`resume`/`seekTo`/
  `setFilter`/`setPlaybackSpeed` untouched.
- **P5** `app/src/main/java/com/chiistudio/library/MainActivity2.kt` +
  `res/layout/activity_main.xml` + `res/values/strings.xml`: `OpenMultipleDocuments`, copy each to
  cache, assemble `List<VideoSegment>`, drive `playTimeline`, handle `TimelineListener`, add status
  strings. Keep user-visible text in `strings.xml`.
- **P6** same app files: per-clip trim (A/B) + filter + speed controls; cross-segment progress.
- Write **CHANGESET.md** (`AUTOMATION: CONTINUE`) with the full manifest, task completion, authorized
  command results (`:videolib:assembleDebug` / `:app:assembleDebug` = not run — authorization
  required), Testing Handoff (UT-SEG, ANDT-TL-1..7), Integration Handoff (MI-1..4).

## Change-surface reference

Exact paths/symbols/collision keys: `IMPLEMENT-PLAN.md` §2 (Change-surface inventory) and §4 (Task
backlog). Test scope: §7.

## Note

Session auto-commit hook is committing changes as you go (message "multiple videos"), squashed —
not per-task. Incremental work is captured automatically; no manual commit needed to preserve it.

## To resume

Run `/implement multiple-videos`, finish **P1 remaining** items 1–6 above, then proceed through the
DAG and write `CHANGESET.md`.
