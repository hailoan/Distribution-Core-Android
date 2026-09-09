AUTOMATION: CONTINUE

# DEV-SPEC — control-preview-video

## 0. Analysis Control

- Outcome: `CONTINUE` — [fact:clarification] the material playback-control semantics were confirmed and no blocking gap remains. Sources: clarification.
- Scope classification: deep, existing-code, public-contract, JNI/native, threading, FFmpeg timing/seek, and EGL rendering surface. [fact:code] Sources: code.
- Evidence depth: approximately 11 focused lookups within the deep budget of 22; no escalation used. [fact:code] Sources: code.
- Graph preflight: no repository was registered in the available code knowledge graph, so focused symbol/text search was used and consequential results were validated against source. [fact:code] Sources: code.
- Owning module: `:videolib`. [fact:ticket] Sources: ticket.
- Proposed changed modules: `:videolib` only; `:app` remains a compile/runtime consumer in the verification closure. [fact:ticket] Sources: ticket, code.
- Dependency closure: `:videolib` → `:app` through `implementation(project(":videolib"))` → `:benchmark` as the app's benchmark target; out-of-repository consumers remain unknown because `:videolib` is an Android library with public Kotlin APIs. [fact:code] Sources: code.
- Public/native/build contracts: public `VideoPreview`/listener behavior, private Kotlin-to-JNI declarations and exported JNI names, native playback state/thread ownership, FFmpeg demux/decode/timing, EGL surface presentation, and packaged ARM ABI linkage. [fact:code] Sources: code.
- Verification closure: `:videolib` native build, `:app` consumer build, and supported ARM device playback/control checks with a real EGL/GLES surface. [fact:code] Sources: code.
- Focus area 1 — Requirements: applicable.
- Focus area 2 — Edge cases: applicable.
- Focus area 3 — Feature impact: applicable — existing `VideoPreview` playback is extended.
- Focus area 4 — Risk: applicable — public JNI/native playback and rendering boundaries are affected.
- Focus area 5 — API docs: N/A — no remote/backend/API document or network surface was supplied or reached.
- Focus area 6 — Figma/design: N/A — no design source was supplied.

### Lookup ledger

| Lookup | Evidence added | Result |
| --- | --- | --- |
| Ticket read | Requested controls and explicit `:videolib` scope | Feature confirmed |
| Graph availability | Registered repositories | No usable project graph; focused source fallback selected |
| `videolib` production symbol search | Public facade, JNI exports, native playback owner, renderer | Entry path confirmed |
| `VideoPreview.kt` inspection | Current public lifecycle, one-attempt rule, callbacks, JNI declarations | Kotlin/public contract confirmed |
| `video_playback.h/.cpp` inspection | State machine, worker cancellation, timestamp pacing, FFmpeg decode, EOF handling | Native/timing boundary confirmed |
| `videolib.cpp` inspection | Kotlin/JNI name mapping and native-owner bridge | JNI contract confirmed |
| Gradle/CMake inspection | App consumer edge, FFmpeg/EGL linkage, supported ABIs | Module/build/ABI closure confirmed |
| App consumer inspection | Existing automatic play/stop/surface integration | Direct in-repository consumer confirmed |
| Focused instrumentation-test inspection | Existing lifecycle, callback, surface, and progressive-frame behavior | Verification implications confirmed |
| User clarification | Loop completion, seek state, speed range, and scope checkpoint | Blocking semantics resolved |

## 1. Sources

| Source | Location | Revision / retrieved | Read status |
| --- | --- | --- | --- |
| Ticket / BA specification | `output/control-preview-video.md` | Working-tree input; 2026-09-09 | Read completely |
| Clarification | This `/study` conversation | 2026-09-09 | Four control answers plus scope checkpoint confirmed |
| Current public facade | `videolib/src/main/java/com/cii/videolib/VideoPreview.kt` | Working tree | Read completely |
| Current terminal contract | `videolib/src/main/java/com/cii/videolib/PlaybackListener.kt` and `PlaybackError.kt` | Working tree | Read completely |
| Native playback contract | `videolib/src/main/cpp/video_playback.h` and `video_playback.cpp` | Working tree | Relevant owner/state/decode path read |
| JNI bridge | `videolib/src/main/cpp/videolib.cpp` | Working tree | Relevant exports and callback bridge read |
| Native linkage / ABI configuration | `videolib/src/main/cpp/CMakeLists.txt` and `videolib/build.gradle.kts` | Working tree | Read completely |
| In-repository consumer | `app/src/main/java/com/chiistudio/library/MainActivity2.kt` and `app/build.gradle.kts` | Working tree | Relevant playback/lifecycle path read |
| Transitive benchmark consumer | `benchmark/build.gradle.kts` and `benchmark/src/main/java/com/example/benchmark/ExampleStartupBenchmark.kt` | Working tree | Target edge and startup-only scope read |
| Existing device behavior evidence | `videolib/src/androidTest/java/com/cii/videolib/ProgressivePlaybackInstrumentedTest.kt` and `PlaybackLifecycleInstrumentedTest.kt` | Working tree | Relevant behavioral cases read |
| API docs | None | — | N/A |
| Design source / index | None | — | N/A |

No files required conversion. Design index: N/A — no design source was supplied.

## 2. Overview & Business Goal

[fact:ticket] Extend the video preview in `:videolib` so playback can be controlled through play/pause, looping, playback-speed changes, and millisecond seeking that updates the frame shown on the surface in real time. Sources: ticket.

[fact:clarification] The feature remains a local, video-only library capability. Audio behavior and host application UI are outside this scope. Sources: clarification.

## 3. Functional Requirements

| FR-ID | Requirement | Status | Evidence/source |
| --- | --- | --- | --- |
| FR-01 | The video preview shall support pausing and resuming playback. | `[fact:ticket]` | ticket: “play/pause”; clarification: paused and playing states were confirmed for seek behavior |
| FR-02 | The video preview shall support an enabled loop mode that restarts playback at EOF continuously. | `[fact:clarification]` | ticket: “loop”; clarification: continuous EOF restart confirmed |
| FR-03 | The video preview shall support changing playback speed to any value greater than or equal to `0.1×`, with no defined maximum. | `[fact:clarification]` | ticket: “change speed”; clarification: flexible `0.1×` minimum and no upper limit confirmed |
| FR-04 | The video preview shall seek to a caller-specified position expressed in milliseconds and update the corresponding frame on the attached surface during seeking. | `[fact:ticket]` | ticket: “seek to mms (while seeking update realtime frame on surface)” |
| FR-05 | After a seek, playback shall continue from the new position if it was playing; if it was paused, it shall remain paused after immediately rendering the requested-position frame. | `[fact:clarification]` | clarification |
| FR-06 | While loop mode is enabled, reaching EOF shall not emit `onPlaybackCompleted()`; after looping is disabled, completion shall be emitted when the final pass reaches EOF. | `[fact:clarification]` | clarification |

## 4. Actors & User Stories

| Story-ID | FR-ID | Story |
| --- | --- | --- |
| Story-01 | FR-01 | `[assumption:code]` As a host application using `VideoPreview`, I can pause and resume an active local-video preview without replacing its surface or input. Sources: code, ticket. |
| Story-02 | FR-02, FR-06 | `[assumption:code]` As a host application, I can enable continuous repeat playback and disable it so the current final pass can complete normally. Sources: code, clarification. |
| Story-03 | FR-03 | `[assumption:code]` As a host application, I can change the rate at which the active video preview progresses. Sources: code, clarification. |
| Story-04 | FR-04, FR-05 | `[assumption:code]` As a host application, I can move playback to a millisecond position, see the sought frame, and retain whether playback was playing or paused. Sources: code, clarification. |

The host application is an inferred actor because the ticket names a library module and the current public entry point is consumed by `:app`; the ticket does not name an end-user persona. [assumption:code] Sources: ticket, code.

## 5. Observable Success Conditions

| SC-ID | FR-ID | Explicit/clarified outcome | Design-Ref | Evidence/source |
| --- | --- | --- | --- | --- |
| SC-01 | FR-01 | Playback can be paused and later resumed. | N/A | `[fact:ticket]` ticket |
| SC-02 | FR-02 | With looping enabled, every EOF restarts playback continuously. | N/A | `[fact:clarification]` clarification |
| SC-03 | FR-03 | A requested playback speed of any value `≥0.1×` is accepted; the requirement defines no maximum. | N/A | `[fact:clarification]` clarification |
| SC-04 | FR-04 | A seek request identifies its destination in milliseconds and updates the frame visible on the attached surface during seeking. | N/A | `[fact:ticket]` ticket |
| SC-05 | FR-05 | Seeking while playing continues playback from the new position. | N/A | `[fact:clarification]` clarification |
| SC-06 | FR-05 | Seeking while paused immediately displays the requested-position frame and leaves playback paused. | N/A | `[fact:clarification]` clarification |
| SC-07 | FR-06 | Looping suppresses completion at intermediate EOF boundaries; after looping is disabled, the final EOF produces completion. | N/A | `[fact:clarification]` clarification |

### 5a. Proposed edge cases & boundary behavior

| FR-ID | Edge/boundary case | Expected handling | Status | Source |
| --- | --- | --- | --- | --- |
| FR-03 | A requested speed is below `0.1×`, zero, negative, NaN, or infinite. | Reject the invalid speed without corrupting the active playback state. | `[assumption:deferrable]` | clarification checkpoint; reasoning from the confirmed `0.1×` minimum |
| FR-04 | Multiple seek requests arrive faster than frames can be decoded and rendered. | A newer request may supersede an older pending request so stale frames do not become the final visible result. | `[assumption:deferrable]` | clarification checkpoint; reasoning from “real time” seek updates |
| FR-04, FR-05 | The requested millisecond lies before zero or after duration/EOF. | Boundary normalization and its observable result are not specified. | `[unknown:deferrable]` | ticket |
| FR-01, FR-03, FR-04 | A control is called before play, after terminal completion, after release, or while no surface is attached. | Acceptance/rejection and state effects are not specified. | `[unknown:deferrable]` | ticket, code |
| FR-01, FR-04 | Surface loss, stop, or release races with pause/resume or seek. | Existing stop/release quiescence and surface-error guarantees remain material constraints; exact control-call result is not specified. | `[unknown:deferrable]` | code |
| FR-02, FR-06 | Loop mode is disabled near an EOF boundary. | The final-pass boundary must be defined consistently so completion is neither lost nor duplicated. | `[assumption:deferrable]` | clarification; current exactly-once terminal behavior in code |
| FR-03 | An accepted speed is substantially faster than decode/render capacity. | The requirement still accepts it, but frame-dropping/catch-up behavior is not specified. | `[unknown:deferrable]` | clarification |
| FR-04 | “Real time” frame updates are evaluated. | No numeric latency, sampling frequency, or every-request presentation threshold is defined. | `[unknown:deferrable]` | ticket |

## 6. Engineering Evidence — Non-normative

This section describes current constraints and impact hypotheses; it does not prescribe a solution. [fact:code] Sources: code.

### Module impact hypothesis

| Module | Owner/consumer | Dependency evidence | Likely contract | Status/confidence |
| --- | --- | --- | --- | --- |
| `:videolib` | Primary owner and only ticket-authorized changed module | Ticket explicitly says `Scope code: module :videolib`; production facade/native pipeline live under `videolib/src/main` | Public Kotlin playback controls plus private JNI/native state, timing, seek, and render behavior | `[fact:ticket]` high |
| `:app` | Direct in-repository consumer; verification-only unless scope changes | `app/build.gradle.kts:55`; `MainActivity2.kt:29,79-128,202-238` | Source/runtime behavior compatibility with `VideoPreview` lifecycle and terminal callbacks | `[fact:code]` high |
| `:benchmark` | Transitive package/benchmark-target consumer | `benchmark/build.gradle.kts:31`; it targets `:app`, which packages `:videolib` | App package/startup compatibility only; its current scenario does not exercise playback controls | `[fact:code]` high |
| External hosts | Potential consumers outside this repository | Android library exposes public `VideoPreview`, `PlaybackListener`, and `PlaybackError`; repository cannot enumerate published consumers | Source/binary/runtime compatibility | `[unknown:deferrable]` medium |

Dependency closure: `:videolib` producer → `:app` direct project consumer → `:benchmark` transitive target/package consumer. The benchmark scenario does not call the reached playback symbols, so its default device benchmark is not part of functional control verification; the app assembly check covers the affected package integration. External consumers remain visible but uninspectable. [fact:code] Sources: code.

### Contract matrix

| Boundary | Owner / consumers | Compatibility obligation | Risk | Status/source |
| --- | --- | --- | --- | --- |
| Public Kotlin API | `:videolib` / `:app`, external hosts unknown | Preserve existing attach/play/stop/detach/release and terminal-callback behavior while adding controls | Source, binary, runtime/behavior | `[fact:code]` code |
| Kotlin ↔ JNI | `VideoPreview.kt` / `videolib.cpp` | Declarations, signatures, exported names, handles, and result semantics must stay synchronized | Native/ABI and runtime | `[fact:code]` code |
| Native state/threading | `VideoPlayback` decode worker, mutexes, condition variable, renderer lock | Controls must coexist with one active attempt, cancellation, surface teardown, and exactly-once terminal claiming | Runtime/concurrency | `[fact:code]` code |
| FFmpeg timing/seek | `VideoPlayback::decodeAttempt` / bundled FFmpeg 7.1 | Playback clock, decoder state, timestamps, EOF, and repositioning affect pause, rate, loop, and seek behavior | Runtime/native | `[fact:code]` code |
| EGL/GLES surface | `PreviewRenderer` / host-owned `Surface` | Sought and resumed frames must use the existing serialized renderer and respect surface ownership/loss | Runtime/native | `[fact:code]` code |
| CMake/ABI packaging | `:videolib` / consuming APKs | Keep FFmpeg/EGL linkage and `arm64-v8a`/`armeabi-v7a` native packaging valid | Build/native ABI | `[fact:code]` code |

### Verification implications

| Module/consumer | Candidate command or device/manual check | Reason | Status |
| --- | --- | --- | --- |
| `:videolib` | `./gradlew :videolib:assembleDebug` | Compile Kotlin/JNI declarations and link FFmpeg/EGL native code for configured ABIs | `[fact:code]` |
| `:app` | `./gradlew :app:assembleDebug` | Confirm the direct Kotlin consumer still compiles and packages `libvideolib.so` | `[fact:code]` |
| `:videolib` device suite | `./gradlew :videolib:connectedDebugAndroidTest` on a supported ARM EGL/GLES 3.0 device | Current behavior depends on real FFmpeg decode, JNI callbacks, EGL presentation, surface lifecycle, and timing | `[fact:code]` |
| Public/native control behavior | Supported-device checks covering pause/resume, loop EOF, dynamic rate, playing/paused seek, surface loss, stop, and release | The requested outcomes cross worker/timing/render boundaries and cannot be established by compilation alone | `[assumption:code]` |

### Entry points

| Symbol | Role | File:line |
| --- | --- | --- |
| `VideoPreview.play` | Current public start entry and owner of accepted-attempt/listener state | `videolib/src/main/java/com/cii/videolib/VideoPreview.kt:60` |
| `VideoPlayback::play` / `decodeAttempt` | Native attempt owner and FFmpeg decode/timestamp-paced presentation path | `videolib/src/main/cpp/video_playback.cpp:146`, `videolib/src/main/cpp/video_playback.cpp:300` |

### Current behavior

| Behavior | Status | Evidence/source |
| --- | --- | --- |
| `VideoPreview` starts asynchronous video-only playback of a local file only when a surface is attached and no other attempt is active. | `[fact:code]` | `VideoPreview.kt:52-85`; `video_playback.cpp:146-173` |
| The public facade exposes start and stop, but no pause/resume, loop, speed, or seek controls. | `[fact:code]` | `VideoPreview.kt:39-142,204-211` |
| Native playback uses one worker, cancellation/condition-variable wakeup, a state mutex, and a renderer mutex; `stop()` joins the worker before returning. | `[fact:code]` | `video_playback.h:40-90`; `video_playback.cpp:175-205,270-298` |
| Frames are paced from stream timestamps against a steady-clock origin, converted to RGBA, and synchronously submitted to the renderer; natural EOF drains the decoder and completes the attempt. | `[fact:code]` | `video_playback.cpp:300-494` |
| Surface loss cancels the active attempt before EGL teardown and reports a render failure; release cancels and joins active work before renderer release. | `[fact:code]` | `video_playback.cpp:207-268` |
| `:app` automatically starts a selected cached local video and uses `stop()` for host/surface lifecycle; it currently has no playback-control UI. | `[fact:code]` | `MainActivity2.kt:71-128,202-238` |

### Affected boundaries

| Boundary | Why it matters | Status | Evidence/source |
| --- | --- | --- | --- |
| Public library API | Host applications need callable controls; existing consumers and callbacks must remain compatible. | `[fact:code]` | `VideoPreview.kt:25-142`; `PlaybackListener.kt:3-9` |
| JNI/native ABI | Every native declaration maps one-to-one to an exported C++ symbol and opaque native owner. | `[fact:code]` | `VideoPreview.kt:204-211`; `videolib.cpp:146-255` |
| Playback concurrency/state | The reached owner coordinates one attempt across decode, stop/release, surface loss, and callback terminal claims. | `[fact:code]` | `video_playback.h:18-90`; `video_playback.cpp:146-298,509-535` |
| FFmpeg clock, EOF, and decode state | All four requested controls affect the current sequential demux/decode and timestamp pacing behavior. | `[fact:code]` | `video_playback.cpp:300-505` |
| EGL surface/thread ownership | Every new visible seek frame must pass through the renderer lock and its dedicated EGL/GLES thread; teardown order remains binding. | `[fact:code]` | `video_playback.cpp:441-455`; `preview_renderer.h:25-65` |
| ABI/build packaging | Native code links bundled FFmpeg and EGL/GLES for two configured ARM ABIs. | `[fact:code]` | `videolib/build.gradle.kts:9-33`; `CMakeLists.txt:12-68` |

Reuse candidates: N/A — the reached code contains the current playback owner and lifecycle contracts, but no analogous pause/loop/rate/seek control scaffold was named or confirmed. [fact:code] Sources: code.

## 7. Non-functional / Technical Constraints

- `[fact:clarification]` Accepted playback speeds have a lower bound of `0.1×` and no defined upper bound. Sources: clarification.
- `[fact:ticket]` Seeking is expressed in milliseconds and must update the surface frame “in real time”; no quantitative latency threshold is supplied. Sources: ticket.
- `[fact:clarification]` Audio behavior is out of scope; current playback is already video-only. Sources: clarification, code.
- `[fact:clarification]` Host UI is out of scope; controls belong to the library capability specified by the ticket. Sources: clarification, ticket.
- `[fact:code]` `VideoPreview` is documented as single-owner-thread, while native FFmpeg decode and EGL/GLES rendering execute on dedicated native threads. Sources: code (`VideoPreview.kt:22-23`, `video_playback.h:40-42`, `preview_renderer.h:4-6`).
- `[fact:code]` Existing accepted-attempt behavior includes one active playback attempt, main-thread terminal delivery, cancellation suppression, and stop-before-release/surface-teardown ordering. Sources: code (`VideoPreview.kt:55-98,123-211`, `video_playback.cpp:175-268,509-535`).
- `[fact:code]` Native linkage is limited to configured `arm64-v8a` and `armeabi-v7a` FFmpeg artifacts and retains 16 KB page-size settings. Sources: code (`videolib/build.gradle.kts:9-31`, `CMakeLists.txt:8-18,39-46`).

API contract constraints: N/A — no remote/backend API applies. The public Kotlin/JNI playback contract is covered above as an engineering boundary.

## 8. Open Questions, Assumptions & Conflicts

| Item | Status/classification | Owner | Consequence |
| --- | --- | --- | --- |
| Invalid speed inputs are rejected without changing valid playback state. | `[assumption:deferrable]` | Solution design | Defines public validation/result semantics below `0.1×` and for non-finite values. |
| Rapid seek requests may supersede older pending requests. | `[assumption:deferrable]` | Solution design | Defines coalescing and stale-frame visibility without changing the confirmed final sought position outcome. |
| Seek behavior before zero and beyond duration/EOF is unspecified. | `[unknown:deferrable]` | Product / solution design | Boundary normalization and visible frame/completion behavior must be made explicit downstream. |
| Control-call behavior without active playback, without a surface, after completion, or after release is unspecified. | `[unknown:deferrable]` | Product / solution design | Affects return/no-op/error semantics but not the confirmed core scope. |
| No numeric definition of “real time” seeking is supplied. | `[unknown:deferrable]` | Product | Performance evaluation can verify visible responsiveness but cannot enforce a numeric threshold without later clarification. |
| Frame dropping/catch-up at extremely high accepted speeds is unspecified. | `[unknown:deferrable]` | Product / solution design | The no-maximum requirement may exceed decode/render capacity; observable degradation policy remains open. |
| Exact behavior when loop disablement races an EOF boundary is unspecified. | `[unknown:deferrable]` | Solution design | Must preserve the confirmed eventual final completion without duplicate or lost terminal delivery. |

Conflicts: none identified. Blocking unknowns: none. [fact:clarification] Sources: ticket, clarification, code.

## 9. Risk Analysis

| Risk | Likelihood / impact | Affected FR/area | Status | Source |
| --- | --- | --- | --- | --- |
| Pause, seek, speed, or loop mutations can race the decode worker, surface loss, stop, and release, violating current quiescence or exactly-once terminal behavior. | High / High | FR-01–FR-06; native state/threading | `[fact:code]` existing boundary, change-sensitive | `video_playback.h:40-90`; `video_playback.cpp:175-298,509-535` |
| Seeking or looping without correctly resetting demux/decoder/timestamp state can display stale frames, use the wrong playback clock, or complete incorrectly. | High / High | FR-02, FR-04–FR-06; FFmpeg timing/seek | `[assumption:code]` introduced by change | `video_playback.cpp:300-505`; clarification |
| A seek-visible frame can race newer seek requests or cross pause/resume state, leaving the wrong final frame or unintended playback state. | High / High | FR-04, FR-05; renderer/state coordination | `[assumption:code]` introduced by change | ticket; clarification; `video_playback.cpp:441-455` |
| An unbounded maximum speed can outpace decode, conversion, and EGL presentation; expected frame-dropping/catch-up behavior is undefined. | High / Medium | FR-03; performance/observable behavior | `[unknown:deferrable]` introduced by requirement | clarification; `video_playback.cpp:376-455` |
| Kotlin declarations and exported JNI functions can diverge when controls cross the boundary, causing runtime linkage failures not visible in Kotlin-only checks. | Medium / High | FR-01–FR-06; JNI/native ABI | `[fact:code]` existing boundary, change-sensitive | `VideoPreview.kt:204-211`; `videolib.cpp:146-255` |
| Added public controls can unintentionally change existing `play`, `stop`, completion, error, or lifecycle behavior for `:app` and uninspectable external consumers. | Medium / High | FR-01–FR-06; public API/consumer behavior | `[fact:code]` existing boundary, change-sensitive | `MainActivity2.kt:79-128,202-238`; `PlaybackListener.kt:3-9` |
| Native control changes may compile for one ABI but fail to link, load, or behave on the other packaged ARM ABI. | Medium / High | Native/ABI verification area | `[fact:code]` existing boundary, change-sensitive | `videolib/build.gradle.kts:18-31`; `CMakeLists.txt:12-68` |
