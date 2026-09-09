AUTOMATION: CONTINUE

# CHANGESET — control-preview-video

## 1. Implementation outcome

- **Status:** completed — android-dev tasks TASK-01 through TASK-04 are implemented.
- **Owning/changed module:** `:videolib` only.
- **Dependency and verification closure:** unchanged direct consumer `:app`; transitive benchmark
  package remains visible; unknown external AAR consumers remain part of the public/JNI/ABI contract.
- **Contracts changed:** additive Kotlin API and matching JNI exports; internal playback state, FFmpeg
  seek/reset behavior, and playback-clock control behavior.
- **Contracts preserved:** existing `play`, `stop`, surface lifecycle, callback/error mapping, JNI
  names, renderer/EGL ownership, CMake/Gradle configuration, supported ABIs, and publication setup.
- **Deviations from approved design/plan:** none. Long waits are evaluated in bounded native wait
  chunks to keep every accepted control interruptible without constraining media duration or speed.
- **Not implemented in this stage:** testing-owned TASK-05 through TASK-10 and integration-owned
  TASK-11 through TASK-14. No test, fixture, Gradle, CMake, generated, or vendored FFmpeg source was
  edited.

## 2. Actual change manifest

Design-Ref is `none — requirement/code-driven` for all rows, as specified by IMPLEMENT-PLAN.md.

| Task | Change-ID | Module | Actual path/symbol | Diff status | Purpose | Verification |
| --- | --- | --- | --- | --- | --- | --- |
| TASK-01–04 | C-01 | `:videolib` | `videolib/src/main/java/com/cii/videolib/VideoPreview.kt`: `pause`, `resume`, `setLooping`, `setPlaybackSpeed`, `seekTo`; five private native declarations | modified | Add public control surface, lifecycle guards, finite/minimum speed validation, and exact Kotlin JNI descriptors | Kotlin compile in `:videolib:assembleDebug` |
| TASK-01–04 | C-02 | `:videolib` | `videolib/src/main/cpp/video_playback.h`: `PlaybackState`, `SeekRequest`, control methods and per-instance control state | modified | Represent Paused/Seeking states, persistent loop/rate configuration, latest seek identity, and interrupt generation | C++ compile for arm64-v8a and armeabi-v7a |
| TASK-01–04 | C-03 | `:videolib` | `videolib/src/main/cpp/video_playback.cpp`: control methods, interruptible scheduler, worker-confined seek/reset, EOF loop arbitration | modified | Quiescent pause, position-preserving resume/rate changes, bounded high-rate frame dropping, clamped/latest-wins seek, same-attempt loop restart, lifecycle precedence | C++ compile/link for both configured ABIs; source/concurrency audit |
| TASK-01–04 | C-04 | `:videolib` | `videolib/src/main/cpp/videolib.cpp`: `nativePause`, `nativeResume`, `nativeSetLooping`, `nativeSetPlaybackSpeed`, `nativeSeekTo` exports | modified | Complete the literal public → JNI → native contract with Boolean acceptance results | Native link in `:videolib:assembleDebug`; declaration/export source pairing |

Every production edit maps to planned C-01 through C-04. Planned C-05 through C-10 are later-stage
test changes and remain untouched.

## 3. Task completion

| Task | Result | Implemented contract | Important invariants retained | Evidence |
| --- | --- | --- | --- | --- |
| TASK-01 | DONE | `pause()` accepts Playing/Paused; `resume()` accepts Paused/Playing; pause wakes the worker and synchronizes with the renderer lock before returning | An accepted pause cannot be followed by a progress frame until resume; duplicate calls are safe; Starting/no-attempt/no-surface/released states reject | Kotlin/JNI/native pairing plus both-ABI compile |
| TASK-02 | DONE | Per-instance speed defaults to `1.0`, persists across attempts, accepts every finite value `>= 0.1`, and re-anchors active pacing when changed | Invalid rate does not mutate state or emit terminal events; paused state remains paused; no frame queue is introduced; late frames may be dropped at rates above 1x while order remains monotonic | Native scheduler inspection plus both-ABI compile |
| TASK-03 | DONE | Signed millisecond seeks are clamped to the known playable interval, executed only on the playback worker, flush decoder state, discard pre-target frames, immediately present the settled target, and restore playing/paused disposition | Newer seek identity supersedes stale work; lifecycle cancellation wins; FFmpeg objects never cross threads; accepted reposition/decode/render failures reuse existing terminal categories | FFmpeg/control/render path inspection plus both-ABI compile |
| TASK-04 | DONE | Loop setting defaults off, persists per instance, and at EOF restarts demux/decoder/timing on the same attempt and worker; disabling makes the next EOF final | No intermediate completion, listener replacement, worker creation, renderer recreation, or accumulated frame buffer; final EOF closes the control-acceptance race before the one terminal callback | EOF/state arbitration inspection plus both-ABI compile |

## 4. Source and concurrency audit

- Public methods and five Kotlin native declarations have one exact
  `Java_com_cii_videolib_VideoPreview_*` export each. Existing JNI exports and callback descriptors
  are unchanged.
- Playback configuration is protected by `stateMutex_`; FFmpeg format/codec/packet/frame/sws state
  remains confined to the existing playback worker.
- Timestamp waits use `waitCv_` with the same state mutex and wake on pause/resume/rate/seek and all
  teardown paths. Large finite rates do not create a queue or unsafe clock conversion.
- Frame presentation still crosses `rendererMutex_` synchronously. Pause waits for a presentation
  that already passed its state check; seek identity is rechecked immediately before presentation.
- Public seek requests use `av_seek_frame` and `avcodec_flush_buffers`; decoded frames before the
  clamped target are discarded. A successful target render re-anchors the playback clock.
- Loop restart reuses the attempt resources and decoder worker, resets pass-local timing, and forces
  at least one visible frame per pass even when high-speed late-frame dropping is active.
- Stop, detach, release, and terminal claim clear pending seek work and wake the worker before join or
  EGL/window teardown. Existing callback suppression/error behavior is retained.
- `git diff --check` reports only the pre-existing trailing space in
  `output/control-preview-video.md`; no whitespace error was introduced in production files.

## 5. Authorized command results

| Check | Command | Result |
| --- | --- | --- |
| Narrow module compile/link | `JAVA_HOME='/Applications/Android Studio.app/Contents/jbr/Contents/Home' ./gradlew :videolib:assembleDebug` | **PASS** — both `arm64-v8a` and `armeabi-v7a`; 29 tasks, 8 executed/21 up-to-date on final run |
| Initial environment probe | `./gradlew :videolib:assembleDebug` with ambient Java 17 | Expected environment failure during project configuration: `:camera` plugin requires JVM 21; no compile task ran. Corrected by the successful JBR command above |

The successful build emitted only existing project-configuration warnings about publication variants,
the plugin Kotlin version, and settings repository preference. No new C++ compiler warning remained.

No app build, Android-test compilation, device execution, JNI/ELF package inspection, commit, push,
publication, signing, upload, or distribution was performed in this stage.

## 6. Testing handoff

| Testing task | Test IDs | Scope | Required behavior/evidence | Environment |
| --- | --- | --- | --- | --- |
| TASK-05 | support for TEST-01–06 | Extend `PlaybackSurfaceProbe` with monotonic presentation timestamps and bounded frame-count waits | Thread-safe real-surface observation; no production hook or sleep-based positive assertion | Android instrumentation source |
| TASK-06 | TEST-01–03 | Pause/resume and speed instrumentation | Quiescence after pause return, resumed progression, idempotency, pre-play/current/later-attempt speed persistence, invalid-rate rejection, bounded ordered high-speed behavior | Supported ARM EGL/GLES device with deterministic ramp MP4 |
| TASK-07 | TEST-04–05 | Seek instrumentation | Playing continuation, paused target settlement and quiescence, negative/end clamp, rapid latest-target-wins, existing error mapping | Supported ARM EGL/GLES device with visible time-coded/ramp fixture |
| TASK-08 | TEST-06 | Loop instrumentation | Visible end-to-start wrap on one attempt with no terminal callback, retained speed, then exactly one completion after loop disable | Supported ARM EGL/GLES device |
| TASK-09 | TEST-08 | Binding/released-state coverage | Invoke every new public/JNI control; verify no-surface/released rejection and existing load/release regression | Android instrumentation; no media required for rejection path |
| TASK-10 | TEST-07 | Lifecycle races | Pending pause/seek cannot survive stop/detach/release; detach maps one render error; clean retry/reattach retains configured loop/rate | Supported ARM EGL/GLES device |

Testing must preserve the plan’s ownership: C-05 through C-10 only. Production observability hooks are
not required or authorized.

## 7. Integration handoff

| Integration task/check | Boundary | Exact next evidence | Status |
| --- | --- | --- | --- |
| TASK-11 / CHK-01 | Full library + Android-test source | `./gradlew :videolib:assembleDebug :videolib:compileDebugAndroidTestKotlin` after TASK-05–10 | `assembleDebug` portion passed; Android-test compilation pending test authoring |
| TASK-12 / CHK-02 | Direct `:app` consumer/package | `./gradlew :app:assembleDebug`; confirm existing `MainActivity2` compiles unchanged and packages revised native library | pending integration stage |
| TASK-13 / CHK-03 | AAR/JNI/ELF contract for both configured ABIs | Inspect `videolib-debug.aar`, exact five new exports plus legacy exports, FFmpeg resolution, ABI payloads, and `>= 0x4000` LOAD alignment | pending integration stage |
| TASK-14 / CHK-04–05 | Runtime behavior across supported ARM ABI families | Execute TEST-01–08, recording device/API/ABI and visible playback/control outcomes | pending tests and device environment |

Guard remains open: implementation is complete and compile-verified, so the next stage is testing
(TASK-05 through TASK-10), followed by integration verification.
