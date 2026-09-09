AUTOMATION: CONTINUE

# DEV-SPEC — enhance-preview-video-async

## 0. Analysis Control

- Outcome: `CONTINUE` — [fact] final evidence validation passed with no blocking gap. Source: clarification, ticket, code.
- Scope classification: [fact] existing-code, one requested changed module, public/native boundary, with one in-repository consumer and unknown external consumers. Source: ticket, code.
- Evidence depth: [fact] standard budget (approximately 14 bounded lookups); the current project graph was queried first and consequential findings were validated against focused source and Gradle inspection. Source: analysis ledger.
- Requirements: applicable — [fact] the ticket and clarification define the progressive-preview goal and module scope. Source: ticket, clarification.
- Edge cases: applicable — [assumption] asynchronous playback crosses cancellation, surface, pacing, and failure boundaries. Source: code.
- Feature impact: applicable — [fact] the behavior touches existing public Kotlin, JNI, C++, FFmpeg, and EGL/GLES surfaces. Source: code.
- Risk: applicable — [fact] the project classifies `videolib` as public/native with JNI and ABI risk. Source: stage context, code.
- API docs: N/A — [fact] no remote/backend surface or API document was supplied or reached. Source: ticket, code.
- Figma/design: N/A — [fact] no design source was supplied. Source: ticket.

Lookup ledger:

| Evidence step | Result | Status/source |
| --- | --- | --- |
| Ticket reading | Feature record extracted directly from the supplied Markdown; no conversion required | [fact:ticket] |
| Project graph | Current graph available; confirmed `VideoPreview`, native playback symbols, and the decode-attempt relationship | [fact:code] |
| Focused source confirmation | Confirmed the public entry, decode worker, per-frame presentation, JNI callbacks, EGL thread, cancellation, and FFmpeg linkage | [fact:code] |
| Module/dependency confirmation | Confirmed `videolib` ownership and `app -> videolib` Gradle/source consumption | [fact:code] |
| Clarification | Confirmed that frames should become visible progressively before the full video is decoded | [fact:clarification] |

## 1. Sources

| Source | Location | Revision/read status | Status |
| --- | --- | --- | --- |
| BA specification | `videolib/output/enhance-preview-video-async.md` | Working-tree content read 2026-09-09; Markdown used directly | [fact:ticket] |
| User clarification | Direct `/study` response | 2026-09-09; read | [fact:clarification] |
| Compact project/stage packet | Generated once by `.aidlc/lib/stage-context.js` for this ticket | 2026-09-09; read | [fact:code] |
| Current implementation | `videolib/src/main/java/com/cii/videolib/VideoPreview.kt`; `videolib/src/main/cpp/video_playback.{h,cpp}`; `videolib/src/main/cpp/videolib.cpp`; `videolib/src/main/cpp/preview_renderer.{h,cpp}`; `videolib/src/main/cpp/render_thread_executor.h`; `videolib/src/main/cpp/CMakeLists.txt` | Current checkout (`HEAD` `50232d8`); read | [fact:code] |
| Consumer/build evidence | `app/src/main/java/com/chiistudio/library/MainActivity2.kt`; `app/build.gradle.kts`; `videolib/build.gradle.kts` | Current checkout; read | [fact:code] |

- Design source: N/A — [fact] none supplied. Source: ticket.
- API documentation: N/A — [fact] none supplied and no remote/backend API surface was reached. Source: ticket, code.
- Converted input files: N/A — [fact] the supplied input was already Markdown. Source: ticket.
- Design index: N/A — [fact] no design source exists. Source: ticket.

## 2. Overview & Business Goal

[fact] Enhance local video preview in `:videolib` so decoded frames become visible on the OpenGL ES surface progressively while FFmpeg continues decoding the remainder of the video. Playback must not wait for the entire video to be decoded before showing frames. Source: ticket, clarification.

[fact] The requested code-change boundary is `:videolib` only. Source: ticket.

## 3. Functional Requirements

| FR-ID | Requirement | Status | Evidence/source |
| --- | --- | --- | --- |
| FR-1 | During local-video playback, `:videolib` shall present decoded frames on the OpenGL ES surface progressively, before FFmpeg has decoded the complete video. | [fact] | ticket, clarification |
| FR-2 | Code changes for this feature shall be confined to the `:videolib` module. | [fact] | ticket |

## 4. Actors & User Stories

| Story-ID | FR-ID | Story |
| --- | --- | --- |
| Story-1 | FR-1 | [fact] As a viewer playing a video through the library, I see available decoded frames while the rest of the video is still being decoded, instead of waiting for full-video decoding. Source: ticket, clarification. |

## 5. Observable Success Conditions

| SC-ID | FR-ID | Explicit/clarified outcome | Design-Ref | Evidence/source |
| --- | --- | --- | --- | --- |
| SC-1 | FR-1 | [fact] At least one decoded frame can become visible on the attached surface before decoding of the complete video finishes. | N/A | clarification |
| SC-2 | FR-2 | [fact] The feature requires no code changes outside `:videolib`. | N/A | ticket |

### 5a. Proposed edge cases & boundary behavior

| FR-ID | Edge/boundary case | Expected handling | Status | Source |
| --- | --- | --- | --- | --- |
| FR-1 | Playback is stopped or released while decoding or a frame presentation is pending | No stale frame or terminal callback should escape after cancellation completes. | [assumption:deferrable] | Current `stop`/`release` contract in code; preservation is not explicitly stated by the ticket. |
| FR-1 | The surface is detached or fails while decoding continues | Playback should cease presenting to the invalid surface and expose the existing render-failure outcome rather than continue using released EGL/window state. | [assumption:deferrable] | Current lifecycle and error contract in code. |
| FR-1 | Decode produces frames faster than the surface can present them | Memory use and presentation delay should remain bounded; the exact buffering or frame-dropping policy is unspecified. | [unknown:deferrable] | Progressive asynchronous producer/consumer behavior implied by clarification; no policy in ticket. |
| FR-1 | Source timestamps are missing, repeated, or irregular | Frames should remain observably progressive without waiting for full decode; the exact pacing rule remains unspecified. | [assumption:deferrable] | Current timestamp/fallback pacing in code; ticket defines no timing policy. |
| FR-1 | Input cannot be opened, has no supported video, decoding fails, or rendering fails | Preserve the existing stable playback error categories unless a later approved contract explicitly changes them. | [assumption:deferrable] | Current public `PlaybackError` contract in code. |
| FR-1 | A second play request arrives while one attempt is active | Preserve the current single-active-attempt behavior unless later product evidence changes it. | [assumption:deferrable] | Current `VideoPreview.play` and native state guards in code. |

## 6. Engineering Evidence — Non-normative

This section records current-system constraints and impact evidence; it does not prescribe a solution.

### Module impact hypothesis

| Module | Owner/consumer | Dependency evidence | Likely contract | Status/confidence |
| --- | --- | --- | --- | --- |
| `videolib` | Primary owner and only requested changed module | Ticket explicitly limits changes to `:videolib`; all confirmed playback/decode/render symbols reside there | Public Kotlin API, runtime behavior, JNI names/callbacks, native ownership, FFmpeg/CMake linkage, EGL thread affinity, ABI packaging | [fact:high] ticket, code |
| `app` | Direct in-repository consumer; verification consumer, not a requested changed module | `app/build.gradle.kts:55` declares `implementation(project(":videolib"))`; `MainActivity2.kt:209` calls `VideoPreview.play` | Source/runtime behavior and host lifecycle integration | [fact:high] code |
| External consumers | Possible AAR consumers outside the repository | `VideoPreview`, `PlaybackListener`, and `PlaybackError` are public library types; no external source is available | Source/binary/runtime behavior compatibility | [unknown:deferrable] stage context, code |

Dependency closure: [fact] `videolib -> app` is a producer-to-direct-consumer edge for compilation and sample runtime behavior. Source: `app/build.gradle.kts:55`, `MainActivity2.kt:16-18,209`. [unknown] External consumer closure cannot be enumerated from this repository. Source: stage context.

### Verification implications

| Module/consumer | Candidate command or device/manual check | Reason | Status |
| --- | --- | --- | --- |
| `videolib` | `./gradlew :videolib:assembleDebug` | Compile/link Kotlin, JNI/C++, FFmpeg archives, EGL, and GLES for configured ABIs | [fact] module default plus code/build boundary |
| `videolib` | `./gradlew :videolib:connectedDebugAndroidTest` on a supported ABI | Exercise native loading/JNI lifecycle and existing binding checks | [assumption] code/test evidence |
| `app` | `./gradlew :app:assembleDebug` | Verify the direct consumer still compiles and packages `videolib` | [fact] dependency edge |
| `app` + `videolib` | Supported-device playback check with a sufficiently long local video | Observe a frame before complete decoding, plus stop/surface-destroy behavior; this is the only explicit user-visible outcome | [assumption] clarification, native/EGL boundary |
| Packaged native library | Supported-ABI load and 16 KB page-alignment check for `arm64-v8a` and `armeabi-v7a` | Preserve the configured FFmpeg/ABI and linker contract | [fact] `videolib/build.gradle.kts:9-31`, `CMakeLists.txt:8-15,39-45` |

### Entry points

| Symbol | Role | File:line |
| --- | --- | --- |
| `VideoPreview.play(path, listener)` | Public caller entry that accepts one playback attempt and crosses JNI | `videolib/src/main/java/com/cii/videolib/VideoPreview.kt:60` |
| `VideoPlayback::play(path)` | Native entry that assigns an attempt and starts the decode worker | `videolib/src/main/cpp/video_playback.cpp:146` |

### Current behavior

| Behavior | Status | Evidence/source |
| --- | --- | --- |
| `VideoPreview.play` calls native playback and returns acceptance without performing decode work in Kotlin. | [fact] | `VideoPreview.kt:60-85` |
| Native `play` starts `runAttempt` on a dedicated `std::thread`; `runAttempt` executes `decodeAttempt`. | [fact] | `video_playback.cpp:146-173,496-506` |
| `decodeAttempt` reads packets and receives decoded frames incrementally; each received frame is converted to RGBA and presented before packet reading continues. It does not decode the complete file before the first presentation. | [fact] | `video_playback.cpp:366-475` |
| Frame presentation is marshalled onto a dedicated EGL/GLES executor, but `PreviewRenderer::pushFrame` uses `runSync`, so the decode worker waits for each render-thread presentation to finish. | [fact] | `preview_renderer.cpp:102-135`; `render_thread_executor.h:21-64` |
| Accepted attempts expose one terminal completion/error callback on the Kotlin main looper; cancellation invalidates the listener. | [fact] | `VideoPreview.kt:52-59,144-202` |
| The current source structure already implements the clarified progressive-frame outcome; runtime fulfillment has not been established by this read-only analysis. | [fact] | code inspection; no verification execution in this stage |

### Affected boundaries

| Boundary | Why it matters | Status | Evidence/source |
| --- | --- | --- | --- |
| Public Kotlin runtime contract | Acceptance, one-active-attempt behavior, callbacks, errors, stop, detach, and release are observable to consumers | [fact] | `VideoPreview.kt:52-142`; `PlaybackListener.kt:3-9`; `PlaybackError.kt:3-15` |
| JNI contract | Private native method names/signatures and callback signatures must stay synchronized across Kotlin and exported C++ names | [fact] | `VideoPreview.kt:144-211`; `videolib.cpp:22-119,146-255` |
| Native thread/lifetime ownership | Decode worker, renderer mutex, cancellation flag, condition variable, and teardown joins coordinate frame and object lifetimes | [fact] | `video_playback.h:40-89`; `video_playback.cpp:132-267` |
| FFmpeg frame/conversion ownership | `AVPacket`, `AVFrame`, codec/format contexts, `SwsContext`, and RGBA storage are per-attempt and cleaned through `AttemptResources` | [fact] | `video_playback.cpp:28-49,300-493` |
| EGL/GLES thread affinity | All EGL/GL work runs through one render executor, and input pixel storage currently remains valid because presentation is synchronous | [fact] | `preview_renderer.h:25-71`; `preview_renderer.cpp:102-135`; `render_thread_executor.h:21-88` |
| CMake/ABI packaging | FFmpeg is statically linked into `libvideolib.so` for only `arm64-v8a` and `armeabi-v7a`, with 16 KB page-size settings | [fact] | `videolib/build.gradle.kts:9-31`; `CMakeLists.txt:8-68` |

### Reuse candidates

| Candidate | Location | Apparent fit | Confidence |
| --- | --- | --- | --- |
| Existing `RenderThreadExecutor` | `videolib/src/main/cpp/render_thread_executor.h:21` | [fact] Existing module-local single-thread queue for EGL/GLES affinity; its `runAsync` and `runSync` capabilities are relevant evidence, without deciding which behavior the feature should use. | High; code |
| Camera `SingleThreadExecutor` pattern | `camera/src/main/cpp/utils/single_thread_executor.h:12` | [fact] Analogous native serialized-worker pattern exists, but direct reuse would cross the ticket's `:videolib`-only module boundary. | Medium; code |

## 7. Non-functional / Technical Constraints

- [fact] Only `:videolib` may be changed for this feature. Source: ticket.
- [fact] The library currently supports local filesystem paths and video-only playback; remote sources and audio are not part of the reached contract. Source: `VideoPreview.kt:52-59`, `video_playback.cpp:52-58,300-493`.
- [fact] EGL/GLES operations must retain single-thread/context affinity. Source: `preview_renderer.h:4-8`, `render_thread_executor.h:3-8`.
- [fact] Any asynchronous handoff of RGBA data must respect the current pixel-buffer lifetime boundary: the decoder-owned buffer is currently safe because render upload completes synchronously before reuse. Source: `video_playback.cpp:426-448`, `preview_renderer.h:40-44`.
- [fact] JNI callback method signatures, native export names, global-reference cleanup, and JavaVM attach/detach behavior are cross-boundary contracts. Source: `videolib.cpp:20-119,146-255`.
- [fact] Existing accepted-attempt completion/error semantics and public error categories are compatibility constraints for the sample and unknown external consumers. Source: `VideoPreview.kt:52-59,144-202`, `PlaybackListener.kt`, `PlaybackError.kt`.
- [fact] Native outputs remain restricted to `arm64-v8a` and `armeabi-v7a` and require the configured 16 KB page-size/link behavior. Source: `videolib/build.gradle.kts:9-31`, `CMakeLists.txt:8-68`.

API contract constraints: N/A — [fact] no remote/backend API or API document applies. Source: ticket, code.

## 8. Open Questions, Assumptions & Conflicts

| Item | Classification | Owner | Consequence |
| --- | --- | --- | --- |
| [unknown] No numeric target defines acceptable time to first visible frame. | deferrable | Product owner | Downstream design and verification can prove progressive display but cannot claim a latency threshold. |
| [unknown] The ticket does not specify whether decoder-to-render handoff must be independently buffered or whether progressive presentation on background threads is sufficient. | deferrable | Solution design / product owner if observable tradeoffs emerge | Buffering, frame dropping, and latency/memory tradeoffs must not be presented as product requirements without new evidence. |
| [assumption] Audio playback remains out of scope. | deferrable | Product owner | The reached contract is video-only and the clarification concerns visible frames. |
| [assumption] Existing callback, error, cancellation, and single-active-attempt behavior remains compatible. | deferrable | Solution design | A change to these public/runtime behaviors would broaden the ticket and external-consumer risk. |
| [unknown] External AAR consumers and their reliance on timing/lifecycle behavior cannot be inspected. | deferrable | Library owner | Compatibility must be preserved or explicitly versioned despite no enumerable external caller set. |
| [fact] Current code already presents each decoded frame before continuing the packet loop, matching the clarified behavior structurally. | non-material | Library owner | Solution design should verify the runtime gap before proposing an implementation delta. |

Conflicts: N/A — [fact] the ticket, clarification, and current code do not make contradictory product claims. Source: ticket, clarification, code.

## 9. Risk Analysis

| Risk | Likelihood/impact | Affected FR/area | Status | Source |
| --- | --- | --- | --- | --- |
| Making render submission asynchronous without transferring/copying frame storage could let the decoder reuse or resize RGBA memory before GL upload finishes. | Medium / High | FR-1; native frame ownership | [assumption] introduced by change depends on downstream design | Current synchronous lifetime contract in `video_playback.cpp:426-448` and `preview_renderer.h:40-44` |
| An unbounded decode-to-render backlog could increase memory use and make displayed frames lag behind playback time. | Medium / High | FR-1; UX/concurrency | [assumption] introduced by change depends on downstream buffering design | Progressive producer/consumer clarification; current per-frame synchronous handoff |
| Stop, surface detach, or release racing queued decode/render work could present stale frames, call a stale listener, use a released window/EGL context, or delay lifecycle teardown. | Medium / High | FR-1; lifecycle/JNI/EGL | [assumption] change-sensitive | Current cancellation/join and surface teardown boundaries in `video_playback.cpp:175-267`; `VideoPreview.kt:88-142` |
| Changing JNI names, callback signatures, error values, or global-reference lifecycle could cause linkage failure, lost callbacks, leaks, or crashes. | Low / High | FR-1; JNI/public contract | [fact] boundary risk; introduced by change uncertain | `VideoPreview.kt:144-217`; `videolib.cpp:20-119,146-255` |
| A behavior that appears progressive structurally may still have poor time-to-first-frame for slow input probing, stream discovery, codec setup, or first-frame decode. | Medium / Medium | FR-1; observable UX | [assumption] existing and change-sensitive | `video_playback.cpp:307-349`; no numeric latency target |
| Native changes may compile for one ABI but fail to link, load, or preserve 16 KB alignment on another supported ABI. | Low / High | FR-1; ABI/package | [fact] boundary risk; introduced by change uncertain | `videolib/build.gradle.kts:9-31`; `CMakeLists.txt:8-68` |
| Public behavior changes could affect external consumers that are not visible in the repository. | Unknown / High | FR-1; external compatibility | [unknown:deferrable] | Public library surface plus unavailable external consumer set |

