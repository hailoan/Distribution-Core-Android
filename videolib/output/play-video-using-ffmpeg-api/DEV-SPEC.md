AUTOMATION: CONTINUE

# DEV-SPEC — play-video-using-ffmpeg-api

## 0. Analysis Control

- **Outcome:** CONTINUE — the ticket and direct `/study` clarifications define the observable playback scope; final validation passed with no blocking gaps.
- **Triage path:** Full path — existing public Kotlin, JNI, FFmpeg/native ABI, threading, and EGL surfaces are affected.
- **Scope classification:** Existing-code, single-owner library feature. Primary owner `:videolib`; direct in-repository consumer `:app`; external consumers unknown.
- **Evidence depth:** Standard (approximately 14 bounded lookups). The current knowledge graph was used first to locate `VideoPreview`/JNI entry symbols and was validated against source. Vendored FFmpeg headers and generated/build directories were excluded.
- **Lookup ledger:** ticket → compact stage packet/module registry → current graph → `VideoPreview` and JNI bridge → renderer lifecycle → CMake/Gradle/native packaging → focused native tests. Two related implemented capabilities were confirmed from source: FFmpeg linkage and OpenGL ES frame rendering.

| # | Focus area | Applicability |
| --- | --- | --- |
| 1 | Requirements | Applicable — ticket plus direct clarifications (§2–§5) |
| 2 | Edge cases | Applicable — proposed, non-normative boundary behavior (§5a) |
| 3 | Feature impact | Applicable — public Kotlin/JNI/native/EGL behavior in `:videolib`, with `:app` as a direct consumer (§6) |
| 4 | Risk | Applicable — native lifecycle, threading, frame ownership, ABI, and public-contract exposure (§9) |
| 5 | API docs | N/A — no remote/backend API or external API document was supplied; “FFmpeg API” refers to the bundled native library |
| 6 | Figma/design | N/A — no design source was supplied and the feature has no specified new UI |

## 1. Sources

| Source | Type | Location | Read status | Revision |
| --- | --- | --- | --- | --- |
| Feature ticket | Markdown | `videolib/output/play-video-using-ffmpeg-api.md` | read | working tree, 2026-09-09 |
| Direct `/study` clarification | conversation | 2026-09-09 responses | read | current session |
| Compact project/stage packet | generated context | `.aidlc/lib/stage-context.js feature-analysis --flow impl-flow --ticket-dir output/play-video-using-ffmpeg-api` | read | working tree, graph built at `ad934e7250bc` |
| Module registry and Gradle edges | registry/code | `.aidlc/modules.json:124`, `videolib/build.gradle.kts:6`, `app/build.gradle.kts:45` | read | working tree |
| Current preview contract | Kotlin/code | `videolib/src/main/java/com/cii/videolib/VideoPreview.kt:6` | read | working tree |
| JNI and FFmpeg linkage | C++/code | `videolib/src/main/cpp/videolib.cpp:8`, `videolib/src/main/cpp/videolib.cpp:37` | read | working tree |
| EGL renderer lifecycle | C++/code | `videolib/src/main/cpp/preview_renderer.h:25`, `videolib/src/main/cpp/preview_renderer.cpp:77` | read | working tree |
| Native build/ABI contract | Gradle/CMake/code | `videolib/build.gradle.kts:9`, `videolib/src/main/cpp/CMakeLists.txt:8` | read | working tree |
| FFmpeg build profile | shell/config | `videolib/ffmpeg-build/config.sh:5` | read | working tree |
| Existing native verification | Android test/code | `videolib/src/androidTest/java/com/cii/videolib/PreviewBindingTest.kt:10`, `FfmpegLinkageTest.kt:10` | read | working tree |

Design source: none. API docs: none. Converted inputs: none. Design index: N/A — no design source.

## 2. Overview & Business Goal

`[fact:ticket+clarification]` Extend `:videolib` so a caller can supply a readable local video file path from Kotlin, start continuous video-only playback, have FFmpeg decode the video frames, and display those frames through the module's OpenGL ES surface pipeline. The caller can explicitly stop playback. Natural end-of-stream leaves the last frame visible and produces a completion notification; failures to open, decode, or render produce an error notification.

Audio playback, pause/resume, seeking, looping, remote URLs, `content://` inputs, and sample-app UI work are outside the clarified scope. `[fact:clarification]`

## 3. Functional Requirements

| FR-ID | Requirement | Status | Evidence/source |
| --- | --- | --- | --- |
| FR-1 | Accept a readable local video file path from Kotlin and continuously decode its video stream using the bundled FFmpeg API for display through OpenGL ES. | fact | ticket (“path video pass from kotlin native”, “Using ffmpeg api … play video on Opengles”) + clarification (continuous playback, video only, local path) |
| FR-2 | Allow the caller to explicitly stop active playback; pause, resume, and seek are not required. | fact | clarification (play and stop) |
| FR-3 | On natural end-of-stream, stop playback, keep the last rendered frame visible, and notify the caller that playback completed. | fact | clarification (stop on last frame; completion notification) |
| FR-4 | Notify the caller when the video cannot be opened or when decoding or rendering fails. | fact | clarification (open/decode/render error notification) |

## 4. Actors & User Stories

| Story-ID | FR-ID | Story |
| --- | --- | --- |
| ST-1 | FR-1 | As a `:videolib` consumer, I can pass a local video path from Kotlin and see its video frames play continuously on the attached OpenGL ES preview surface. `[fact:ticket+clarification]` |
| ST-2 | FR-2 | As a `:videolib` consumer, I can stop playback before the file reaches its end. `[fact:clarification]` |
| ST-3 | FR-3 | As a `:videolib` consumer, I am notified when playback finishes and can still see the final frame. `[fact:clarification]` |
| ST-4 | FR-4 | As a `:videolib` consumer, I am notified when opening, decoding, or rendering the video fails. `[fact:clarification]` |

Primary actor: an Android host using the public `:videolib` Kotlin API. `[fact:ticket+code]`

## 5. Observable Success Conditions

| SC-ID | FR-ID | Explicit/clarified outcome | Design-Ref | Evidence/source |
| --- | --- | --- | --- | --- |
| SC-1 | FR-1 | Given a readable supported local video path and an attached preview surface, playback continuously displays decoded video frames through OpenGL ES. | — | fact: ticket + clarification |
| SC-2 | FR-2 | When the caller stops active playback, playback stops without requiring object release. | — | fact: clarification |
| SC-3 | FR-3 | When the video reaches natural end-of-stream, no loop begins, the last rendered frame remains visible, and the caller receives a completion notification. | — | fact: clarification |
| SC-4 | FR-4 | When opening, decoding, or rendering fails, the caller receives an error notification. | — | fact: clarification |

### 5a. Proposed edge cases & boundary behavior

| FR-ID | Edge/boundary case | Expected handling | Status | Source |
| --- | --- | --- | --- | --- |
| FR-1, FR-4 | Empty, missing, unreadable, or non-file path | Do not begin playback; notify the caller of an open/input failure. | assumption — deferrable | ticket local-path input + clarified error notification |
| FR-1, FR-4 | File has no decodable video stream, is corrupt, or uses an unsupported container/codec | Do not crash or silently complete; notify the caller of the relevant open/decode failure. | assumption — deferrable | FFmpeg/native boundary + clarified error notification |
| FR-1 | Playback is requested before a surface is attached | Do not render until a valid surface is available; exact start/rejection behavior remains a design input. | unknown — deferrable | current `VideoPreview.attachSurface` contract at `VideoPreview.kt:27` |
| FR-1, FR-4 | Surface is destroyed or EGL fails during playback | Stop producing frames for that surface and report a rendering failure; exact retry behavior remains a design input. | assumption — deferrable | current surface lifecycle at `preview_renderer.cpp:77`, `preview_renderer.cpp:164` |
| FR-2 | Stop races with decode or render work | Stop future playback work and release owned native resources without deadlock, use-after-free, or post-stop callbacks. | assumption — deferrable | native-thread/lifecycle boundary in `preview_renderer.h:25` |
| FR-2 | Stop is called with no active playback or more than once | Treat it safely; exact no-op/status contract remains a design input. | assumption — deferrable | existing idempotent `detachSurface`/`release` precedent at `VideoPreview.kt:61`, `VideoPreview.kt:68` |
| FR-1, FR-2 | A second play request arrives while playback is active | Behavior must be made explicit during solution design; replacement versus rejection is not specified. | unknown — deferrable | public playback lifecycle is new |
| FR-3 | Video reaches end-of-stream after a stop/release race | Do not emit a misleading natural-completion outcome after caller-initiated termination. | assumption — deferrable | clarified completion semantics + native lifecycle boundary |
| FR-4 | Error notification crosses from a native worker to Kotlin | Deliver at most one terminal error for the failed playback attempt and do not access released callback state. | assumption — deferrable | JNI/callback ownership risk; no current callback contract |

## 6. Engineering Evidence — Non-normative

This section records current-system constraints and candidates only; it does not prescribe a solution.

### Module impact hypothesis

| Module | Owner/consumer | Dependency evidence | Likely contract | Status/confidence |
| --- | --- | --- | --- | --- |
| `:videolib` | primary owner / expected changed module | ticket explicitly limits scope to `:videolib`; module root and public status at `.aidlc/modules.json:124` | public Kotlin API, JNI, native lifecycle/threading, FFmpeg decode, EGL rendering, ABI packaging | fact / high |
| `:app` | direct in-repository consumer; no requested feature code | `implementation(project(":videolib"))` at `app/build.gradle.kts:55` | source/behavior compatibility with the library | fact / high |
| External consumers | unknown consumers of public library | `.aidlc/modules.json:135-136` | source, binary, runtime/behavior, and native compatibility | unknown / high |
| `:camera` | reference only; not affected | separate module with its own vendored FFmpeg/native pipeline | no contract change; must remain untouched | fact / high |

Dependency closure: `:videolib` → `:app` by Gradle project dependency. The benchmark reaches `:app`, but no benchmark or sample-host behavior is requested, so it is outside the changed-module set unless later solution design adds host wiring. External consumers remain visible because absence of repository callers is not evidence of absence.

### Verification implications

| Module/consumer | Candidate command or device/manual check | Reason | Status |
| --- | --- | --- | --- |
| `:videolib` | `./gradlew :videolib:assembleDebug` | registry default; proves Kotlin/JNI/CMake/native linkage for both packaged ABIs | candidate |
| `:app` | `./gradlew :app:assembleDebug` | compile the direct project consumer against any public API change | candidate |
| `:videolib` native contract | existing and future focused instrumented JNI/native-load checks on `arm64-v8a` and `armeabi-v7a` | JNI exports, FFmpeg symbols, and ABI packaging are runtime contracts | candidate/device-required |
| Playback behavior | supported-device check with a readable video, explicit stop, natural completion, invalid path, decode failure, and surface teardown | observable rendering and EGL/native lifecycle cannot be established by a host JVM test | candidate/device-required |

### Entry points

| Symbol | Role | File:line |
| --- | --- | --- |
| `VideoPreview` / `attachSurface` / `pushFrame` | Current public Kotlin facade and host-owned surface/frame entry | `videolib/src/main/java/com/cii/videolib/VideoPreview.kt:22`, `:32`, `:45` |
| `Java_com_cii_videolib_VideoPreview_nativeCreate` and preview JNI exports | Current Kotlin-to-native renderer ownership and surface/frame bridge | `videolib/src/main/cpp/videolib.cpp:47-108` |
| `PreviewRenderer::surfaceAvailable` / `pushFrame` / `releaseSurface` | Current render-thread EGL lifecycle and synchronous RGBA draw boundary | `videolib/src/main/cpp/preview_renderer.cpp:77`, `:102`, `:164` |

### Current behavior

| Behavior | Status | Evidence/source |
| --- | --- | --- |
| `VideoPreview` accepts caller-supplied direct RGBA8888 frames or a test pattern and renders them to a host-supplied `Surface`; it explicitly has no video-decode dependency. | fact | `VideoPreview.kt:6-20` |
| The Kotlin facade is single-owner-thread/not thread-safe, while GL/EGL work is marshalled synchronously to a dedicated native render thread. | fact | `VideoPreview.kt:19-20`; `preview_renderer.h:4-8` |
| Native renderer ownership is an opaque pointer held by each Kotlin `VideoPreview`; destroy deletes it and its destructor releases the surface and drains the render executor. | fact | `videolib.cpp:37-49`, `videolib.cpp:103-108`; `preview_renderer.cpp:13-17` |
| FFmpeg 7.1 static libraries and headers are already linked into `libvideolib.so`; current runtime use is limited to reporting version/linkage. | fact | `CMakeLists.txt:12-26`, `CMakeLists.txt:47-67`; `videolib.cpp:21-35` |
| No production symbol currently opens a media path, selects a video stream, decodes packets/frames, schedules playback, stops decoding, or notifies Kotlin of completion/errors. | fact | bounded graph and source inspection of `VideoPreview.kt`, `videolib.cpp`, and `preview_renderer.*` |

### Affected boundaries

| Boundary | Why it matters | Status | Evidence/source |
| --- | --- | --- | --- |
| Public Kotlin library API | New playback controls and terminal outcomes become source/runtime behavior for `:app` and unknown external consumers. | fact | `.aidlc/modules.json:124-144`; `app/build.gradle.kts:55` |
| JNI declarations and exported names | Kotlin native declarations, C++ exports, parameters, and callback ownership must remain synchronized. | fact | `VideoPreview.kt:79-84`; `videolib.cpp:37-41` |
| FFmpeg demux/decode/frame ownership | Playback reaches previously unused `avformat`/decoder paths and introduces packet/frame cleanup and terminal errors. | assumption — high confidence | linked FFmpeg boundary at `CMakeLists.txt:47-59`; ticket goal |
| Native threading/lifecycle | Decode progress, stop, surface loss, completion/error delivery, and object destruction can race. | fact | existing dedicated renderer executor and native pointer ownership at `preview_renderer.h:25-67`; `videolib.cpp:40-41` |
| EGL/GLES surface lifecycle | Rendering requires a valid attached surface and context; current teardown is ordered and idempotent. | fact | `preview_renderer.cpp:77-100`, `:141-176` |
| ABI/build packaging | FFmpeg archives exist only for `arm64-v8a` and `armeabi-v7a`; the shared object has 16 KB page-size constraints. | fact | `videolib/build.gradle.kts:18-31`; `CMakeLists.txt:8-18`, `:38-45` |
| Input/permission ownership | Scope is a local filesystem path; the consuming host must provide a readable path. | fact | ticket + clarification; library manifest has no feature-specific permissions |

### Reuse candidates

| Candidate | Location | Apparent fit | Confidence |
| --- | --- | --- | --- |
| Existing `VideoPreview` surface/public lifecycle | `videolib/src/main/java/com/cii/videolib/VideoPreview.kt` | Reusable current host-facing surface owner and frame-rendering facade; exact playback extension is undecided. | high |
| Existing `PreviewRenderer::pushFrame` RGBA draw path | `videolib/src/main/cpp/preview_renderer.cpp:102` | Reusable sink for decoded/converted frames if its synchronous ownership and render-thread constraints fit. | high |
| Bundled FFmpeg 7.1 static linkage | `videolib/src/main/cpp/CMakeLists.txt:12-59` | Reusable native demux/decode libraries; actual enabled format matrix is not established as a public promise. | high |
| Existing JNI binding/linkage tests | `videolib/src/androidTest/java/com/cii/videolib/PreviewBindingTest.kt`, `FfmpegLinkageTest.kt` | Canonical regression pattern for library load, exported symbols, and safe no-surface lifecycle; not proof of rendered playback. | high |

## 7. Non-functional / Technical Constraints

- `[fact:project+code]` Keep implementation ownership in `:videolib`; do not modify `:camera` or its vendored `camera/src/main/cpp/ffmpegv2` content.
- `[fact:code]` Preserve module `minSdk 21`, NDK `29.0.14206865`, C++17, `arm64-v8a`/`armeabi-v7a` packaging, and 16 KB page-size linkage unless separately authorized.
- `[fact:code]` Preserve exact agreement between Kotlin native declarations and exported JNI symbols, plus single-release ownership for native objects, windows, FFmpeg packets/frames, and callback references.
- `[fact:code]` GL/EGL operations must remain on the renderer's native GL thread with a current context; surface teardown must remain safe and idempotent.
- `[fact:clarification]` Playback is video-only. Audio decode/output and audio-video synchronization are not required.
- `[fact:clarification]` Input is a caller-provided readable local filesystem path. URL fetching, content-resolver ownership, and new Android permissions are not included.
- `[unknown:deferrable]` No exact public container/codec guarantee is specified. The FFmpeg build configuration defaults to the full LGPL profile (`config.sh:25-30`); a slim override example includes file protocol, H.264, and MP4/Matroska support but does not prove the exact committed artifact configuration.
- `[unknown:deferrable]` No playback clock tolerance, frame-drop policy, rotation/SAR handling, scaling mode, or color-conversion contract is specified. These must not be presented as ticket requirements.

## 8. Open Questions, Assumptions & Conflicts

| Item | Classification | Owner | Consequence |
| --- | --- | --- | --- |
| Input is a readable local filesystem path, not a URL or `content://` URI. | fact — resolved | user | Fixes input/data-ownership scope. |
| Playback is continuous and video-only. | fact — resolved | user | Fixes primary observable scope; excludes audio synchronization. |
| Controls are play and stop only. | fact — resolved | user | Excludes pause/resume/seek from this feature. |
| Natural end stops on the last frame; completion and open/decode/render errors are reported. | fact — resolved | user | Defines terminal observable outcomes. |
| Exact supported container/codec matrix. | unknown — deferrable | product/maintainer | A future public compatibility guarantee must follow the actually bundled FFmpeg profile. |
| Timing tolerance, frame dropping, rotation/aspect/scaling, and color behavior. | unknown — deferrable | product/solution design | Influences rendering quality and scheduling but does not change the confirmed capability boundary. |
| Play-before-surface, repeated play, and callback-thread semantics. | unknown — deferrable | solution design | Must be made explicit before implementing the public/native lifecycle contract. |
| Source conflicts. | non-material | — | None found. |

## 9. Risk Analysis

| Risk | Likelihood/impact | Affected FR/area | Status | Source |
| --- | --- | --- | --- | --- |
| Stop, surface loss, release, and native terminal callbacks may race with decode/render work, causing deadlock, use-after-free, or callbacks into released state. | medium / high | FR-1–FR-4 / JNI and native lifecycle | assumption — open; introduced by change; design decision required | code: opaque native ownership and dedicated render executor at `videolib.cpp:37-41`, `preview_renderer.h:25-67` |
| Decoder work may block the caller or GL executor, producing unresponsive playback or teardown. | medium / high | FR-1, FR-2 / threading | assumption — open; introduced by change; design decision required | code: synchronous rendering at `preview_renderer.cpp:102-120`; ticket adds continuous decode work |
| FFmpeg packet/frame/pixel memory may be freed too early, leaked, or used after release while crossing into the renderer. | medium / high | FR-1, FR-2 / native ownership | assumption — open; introduced by change; design decision required | ticket adds demux/decode; code requires pixels to remain valid through synchronous draw at `preview_renderer.h:40-43` |
| Surface/context loss during playback may produce native crashes or terminal notifications after a new surface is attached. | medium / high | FR-1, FR-4 / EGL lifecycle | assumption — open; introduced by change; design decision required | code: explicit surface state and ordered teardown at `preview_renderer.cpp:77-100`, `:141-176` |
| Public playback behavior may break the sample app or unknown external consumers, including binary/JNI compatibility. | low-medium / high | FR-1–FR-4 / public contract | assumption — open; introduced by change; design decision required | registry: `:videolib` public/external consumers unknown; code: `app/build.gradle.kts:55` |
| A claimed format may work in configuration but not in the bundled archive, or the shipped profile may omit a needed demuxer/decoder/protocol. | medium / high | FR-1, FR-4 / FFmpeg compatibility | assumption — open; introduced by change uncertain; design decision required | code: full profile is the overridable default at `config.sh:25-41`; no artifact manifest found beside committed archives |
| Native build or runtime may work on one ABI but fail on the other, or violate 16 KB alignment. | low-medium / high | FR-1 / native ABI | assumption — open; introduced by change uncertain; verification required | code: two packaged ARM ABIs and ABI-specific linkage at `build.gradle.kts:18-31`, `CMakeLists.txt:38-45` |
| Natural completion and caller stop may be reported inconsistently during a terminal race. | medium / medium-high | FR-2–FR-4 / observable state | assumption — open; introduced by change; design decision required | clarification defines distinct outcomes; code has no current playback state/callback contract |
