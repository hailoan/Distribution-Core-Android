AUTOMATION: CONTINUE

# SOLUTION-DESIGN — play-video-using-ffmpeg-api

## 1. Decision ledger

### Sources and investigation

| Item | Status | Evidence / decision | Impact |
| --- | --- | --- | --- |
| Approved behavior | observed | `DEV-SPEC.md` FR-1–FR-4, SC-1–SC-4 and Story ST-1–ST-4 | Defines local-file, video-only play/stop, completion, and failure behavior. |
| Original request | observed | `videolib/output/play-video-using-ffmpeg-api.md:1` | Restricts production ownership to `:videolib`. |
| Investigation depth | decision | Deep: 20 bounded lookup units used from a cap of 20 (current graph context, five focused symbol searches, five relationship queries, and nine focused source/config inspections). The graph was current but incomplete around C++ class relationships, so consequential results were validated against source. | Appropriate for a public Kotlin/JNI/FFmpeg/EGL boundary. |
| Primary owner | observed | `:videolib`; module registry plus `videolib/build.gradle.kts:6` and namespace at `:7`. | All production behavior remains in the library. |
| Consumer closure | observed | `:videolib` → `:app` through `app/build.gradle.kts:55`; unknown external consumers remain in scope because `:videolib` is public. `:benchmark` is not affected because no sample-host behavior or package contract changes. | Preserve source, binary, runtime, native/ABI, and packaging compatibility. |
| Existing public seam | decision | Extend the existing `VideoPreview` facade rather than creating a parallel public preview abstraction. It already owns one native renderer handle, the host surface contract, and single-owner-thread use (`VideoPreview.kt:6`, `:19`, `:22`). | Keeps the public model minimal and preserves current preview operations. |
| Existing render seam | decision | Reuse `PreviewRenderer` and its RGBA upload path as the only presentation sink. It already serializes EGL/GLES work on one native render thread and synchronously consumes caller-owned pixels (`preview_renderer.h:25`, `:35`, `:40`). | Decode does not gain EGL ownership; no second GL path is introduced. |
| Playback decomposition | proposed | Add a native playback coordinator and a dedicated decode/timing worker behind the existing facade. The coordinator owns playback state and attempt identity; the worker owns FFmpeg demux/decode/conversion resources. | Keeps blocking media work off the Kotlin caller and GL threads while centralizing race resolution. |
| Surface prerequisite | proposed | A play request is accepted only while a valid preview surface is attached. A request made earlier is rejected without opening the file or creating a playback attempt. | Removes hidden buffering and makes render readiness explicit. |
| Overlapping play | proposed | A second play request while starting, playing, or stopping is rejected; it does not replace or disturb the active attempt. | Avoids ambiguous completion/error ownership and implicit teardown. |
| Terminal notifications | proposed | Each accepted attempt produces at most one terminal completion or error notification. Explicit stop and release are caller-directed cancellation and produce neither. Notifications are serialized onto the Android main thread; stale native events are discarded by attempt identity. | Gives hosts deterministic event and threading behavior. |
| Stop and release | proposed | Stop is an idempotent barrier: when it returns, that attempt can render no more frames and enqueue no terminal notification. Release first establishes the same barrier, then destroys native playback and renderer ownership; the instance remains inert. | Prevents use-after-free, misleading completion, and post-release callbacks. |
| Surface loss | proposed | Detaching or losing the active surface terminates active playback as a rendering error unless explicit stop/release has already won the terminal race. Renderer teardown remains ordered and idempotent. | Makes an observable EGL failure path explicit. |
| Playback timing | proposed | Presentation follows decoded video timestamps against a monotonic clock. Late intermediate frames may be skipped to preserve forward progress, but ordering cannot reverse and the final decoded frame must be presented before natural completion. Exact lateness tolerance is implementation-local. | Provides continuous video-only playback without coupling decode throughput to wall-clock speed. |
| Visual mapping | decision | Preserve the renderer's observed full-surface RGBA quad and vertical texture-coordinate correction (`gl_program.cpp:18`, `:194`). Rotation metadata, sample-aspect correction, aspect-fit/crop modes, and color-management guarantees are outside this feature. | Avoids inventing a new rendering policy; supported inputs display in coded pixel orientation. |
| Format guarantee | decision | “Supported” means accepted by the FFmpeg capabilities actually bundled for the device ABI. No stable container/codec matrix is added. Unsupported/no-video inputs terminate as decode/open errors. | The default build is broad but overridable (`ffmpeg-build/config.sh:25`); configuration is not promoted to an API promise. |
| Native build contract | observed | FFmpeg 7.1 static archives are linked into `libvideolib.so` for `arm64-v8a` and `armeabi-v7a`, with 16 KB page alignment (`CMakeLists.txt:8`, `:12`, `:21`; `build.gradle.kts:18`). | Preserve ABI filters, NDK/CMake settings, archive linkage, and AAR packaging. No vendored FFmpeg modification is required. |
| Explicitly unspecified | intentionally unspecified | Public naming/signatures; internal filenames/classes; exact frame-drop threshold; queue sizes; diagnostic wording; FFmpeg error-code mapping; absolute timing tolerance; rotation/SAR/scaling enhancements; and codec allowlists. | These choices do not alter the behavioral and compatibility contract defined here. |
| Blockers | observed | None. The missing exact codec and visual-transform guarantees are explicitly outside the approved compatibility promise. | Guard may continue. |

## 2. Behavior and state transitions

### Behavior contract

| FR-ID | SC-ID | AC-ID | Story-ID | Design-Ref | Rule / trigger | Observable outcome | Failure / recovery |
| --- | --- | --- | --- | --- | --- | --- | --- |
| FR-1 | SC-1 | AC-1 | ST-1 | — | The host submits a non-empty local filesystem path while the facade is live, a surface is ready, and no attempt is active. | The request is accepted; video frames are decoded, paced in presentation order, and displayed through the attached GLES surface. The call does not perform decoding on the caller thread. | An empty path or invalid lifecycle state is rejected without starting. An accepted path that cannot be opened or decoded follows AC-4. |
| FR-2 | SC-2 | AC-2 | ST-2 | — | The host stops an accepted attempt, including while it is opening, decoding, waiting for presentation time, or rendering. | Stop is idempotent and returns only after future frame presentation and terminal notification for that attempt are suppressed. The facade remains reusable with the still-attached surface. | A stop with no active attempt is a safe no-op. Stop does not report completion or error. |
| FR-3 | SC-3 | AC-3 | ST-3 | — | The accepted stream reaches natural end-of-stream and all decoder-delayed video frames have been consumed. | The final decoded frame is presented, playback becomes terminal without looping, the displayed texture is not cleared, and exactly one completion notification is delivered on the main thread. | Stop, release, or surface failure winning first suppresses natural completion. Last-frame visibility lasts only while that same surface/EGL lifecycle remains valid. |
| FR-4 | SC-4 | AC-4 | ST-4 | — | An accepted attempt fails during file opening, stream selection, decode/conversion, presentation, or surface/EGL use. | Playback stops and exactly one main-thread error notification identifies the semantic failure phase without exposing unstable FFmpeg internals as the public contract. | Resources owned by the attempt are released. A later request may retry after the surface is ready; no automatic retry occurs. |
| FR-1, FR-4 | SC-1, SC-4 | AC-5 | ST-1, ST-4 | — | Play is requested with no valid attached surface, or a second play is requested while an attempt is active. | The request is synchronously rejected; no file is opened, the current attempt is unchanged, and no terminal callback is generated for the rejected request. | The caller may attach a surface or wait for/stop the active attempt, then request play again. |
| FR-1 | SC-1 | AC-6 | ST-1 | — | Decoded timestamps establish presentation order. | Timing uses a monotonic clock; late intermediate frames may be dropped, frames are never displayed out of order, and decode does not block EGL ownership. | Timestamp irregularities are normalized to non-decreasing presentation. If usable ordering/timing cannot be established, the attempt fails as decode/timing rather than running unbounded. |
| FR-2, FR-3, FR-4 | SC-2, SC-3, SC-4 | AC-7 | ST-2, ST-3, ST-4 | — | Stop, EOS, error, surface loss, and release race for the same accepted attempt. | One serialized state owner chooses the winner. Completion/error is at most once and only for the still-current attempt; stop/release yield no terminal event. | Events from a superseded or cancelled attempt are ignored before crossing to Kotlin. |
| FR-1, FR-4 | SC-1, SC-4 | AC-8 | ST-1, ST-4 | — | A decoded frame crosses from FFmpeg ownership into the renderer. | The frame is converted to tightly packed RGBA8888 and remains alive through the renderer's synchronous upload; GL work stays on the renderer thread. | Conversion allocation/failure becomes a decode error; EGL upload/swap failure becomes a render error. No borrowed frame survives the synchronous hand-off. |
| FR-1–FR-4 | SC-1–SC-4 | AC-9 | ST-1–ST-4 | — | Existing consumers continue to use surface attach/detach, caller-supplied frames, patterns, or release without invoking playback. | Existing behavior and JNI exports remain available. New playback behavior is additive and shares the established renderer lifecycle. | Playback additions must not change `NativeLib` linkage/version behavior or current safe no-op/idempotent behavior. |

### State model

Surface readiness is a separate precondition: `unattached`, `ready`, or `lost/released`. Playback cannot enter `Starting` without `ready`.

| State | Meaning / invariants | Permitted events | Prohibited / ignored events |
| --- | --- | --- | --- |
| Idle | No attempt owns FFmpeg resources; facade may retain a ready surface and the last texture. | Valid play; detach; release; idempotent stop. | Stop performs no work. |
| Starting | One accepted attempt owns its identity and is opening/selecting the video stream. | Open success; stop; release; surface loss; open/decode error. | Additional play is rejected. |
| Playing | Frames are decoded, paced, converted, and synchronously handed to the ready renderer. | Next frame; EOS; stop; release; surface/render/decode failure. | Additional play is rejected; stale attempt events are ignored. |
| Stopping | Cancellation has won; decode/timing/render work is converging on the stop barrier. | Worker quiescence. | EOS/error cannot escape as a terminal callback; new play is rejected until Idle. |
| Completed | Natural EOS won, decoder is drained, the final frame is retained by the current surface, and completion was scheduled exactly once. | New play; detach; release; idempotent stop. | Late frames/events from the completed attempt are ignored. |
| Failed | One terminal error won and attempt resources are released; the surface may remain reusable unless it was lost. | New play when surface readiness is restored; detach; release; idempotent stop. | Late frames/completion from the failed attempt are ignored. |
| Released | Native ownership and callback reachability are gone; terminal facade state. | Idempotent stop/detach/release. | Play is rejected; no frame or callback can be produced. |

### Transition contract

| From | Event / precondition | To | Side effect | Failure / cancellation / recovery |
| --- | --- | --- | --- | --- |
| Idle, Completed, Failed | Valid play and surface ready | Starting | Establish a new current attempt and begin native open off the caller/GL threads. | Immediate lifecycle/input rejection leaves the prior state; accepted open failure goes to Failed. |
| Starting | Video stream and decoder become ready | Playing | Establish timestamp origin and begin paced frame delivery. | Missing/unsupported video stream goes to Failed. |
| Playing | Decoded frame becomes due | Playing | Convert, synchronously present, then release attempt-owned frame/conversion storage when safe. | Decode/conversion/render failure goes to Failed. |
| Playing | Decoder-drained EOS after final presentation | Completed | Release FFmpeg resources and schedule one completion event. | A prior stop/release/surface-loss winner suppresses this transition. |
| Starting, Playing | Explicit stop | Stopping | Interrupt waits/input where supported, prevent new render submissions, and invalidate pending terminal events. | Worker quiescence completes the barrier. |
| Stopping | All attempt work is quiescent | Idle | Release attempt-owned FFmpeg and conversion resources; retain the valid surface/texture without clearing it. | No callback is emitted. |
| Starting, Playing | Open/decode/conversion/render failure | Failed | Stop the attempt, release its resources, and schedule one semantic error event. | The same ready surface may accept another play unless the failure was surface-related. |
| Starting, Playing | Surface detach/loss before explicit cancellation | Failed | Stop playback before ordered EGL teardown; report a render error once. | Reattachment restores surface readiness and permits a later play. |
| Any non-Released | Release | Released | Apply the stop barrier, invalidate callback delivery, tear down GL/EGL on its owner thread, release the native window once, then destroy native ownership. | Repeated release is a no-op. |

## 3. Components and responsibilities

### Module Contract Matrix

| Module | Owner / consumer | Responsibility | Depends on | Crossed contract | Compatibility obligation | Verification obligation |
| --- | --- | --- | --- | --- | --- | --- |
| `:videolib` | owner / changed | Own additive public playback behavior, native attempt lifecycle, FFmpeg decode/conversion, renderer integration, notifications, and packaged native capability. | Android Surface/JNI, bundled FFmpeg 7.1 static archives, EGL/GLES 3.0 | Public Kotlin API; callback threading; JNI names/types/ownership; native concurrency; FFmpeg frame ownership; EGL affinity; ABI/AAR packaging | Preserve existing public methods and their behavior, exact Kotlin/JNI agreement, one-release ownership, minSdk 21, both ARM ABIs, and 16 KB alignment. | `:videolib:assembleDebug`; native-symbol/package inspection; supported-device runs on each packaged ABI for playback, lifecycle, callbacks, and GL output. |
| `:app` | direct consumer / unchanged | Continue compiling as a host of the public library; no sample playback UI is required. | `:videolib` via `implementation(project(":videolib"))` | Kotlin source/binary consumption and APK native packaging | No required host migration; additive API must not alter existing app behavior. | `:app:assembleDebug` validates consumer compilation and native packaging integration. |
| External consumers | unknown consumers / unchanged | Supply a readable local path and own the Android surface and its lifecycle. | Published/consumed `:videolib` API | Source, binary, runtime/behavior, native/ABI | Existing clients remain valid; callback thread and lifecycle rules for new APIs are stable and documented. | Public contract review plus supported-device evidence; absence of repository callers is not compatibility proof. |

Changed-module set: `:videolib` only. `:camera`, `:core`, `:network`, `:plugin`, and `:benchmark` have no crossed contract and remain outside the change and verification closure.

| Component role | Observed / proposed | Responsibility / owned state | Delegates to | Dependency direction | Must not own / know | Evidence / decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Host consumer | observed | Own readable local-file access, `Surface` lifecycle, facade lifetime, and response to acceptance/completion/error outcomes. | Public preview facade | Host → `:videolib` | FFmpeg objects, native threads, EGL internals, or decoder cleanup | DEV-SPEC §2 and `VideoPreview.kt:13`. |
| Public preview facade | observed + extended | Remain the single host-facing owner of the opaque native handle; validate lifecycle prerequisites; expose additive play/stop and terminal-event semantics; marshal native terminal outcomes to main. | Native playback coordinator and existing surface bridge | Kotlin → JNI/native | Demux/decode state, frame memory, GL calls, or host file permissions | Existing `VideoPreview` handle/lifecycle at `VideoPreview.kt:22`; extension decision above. |
| Native playback coordinator | proposed | Own exactly one current playback attempt, its semantic state/identity, cancellation barrier, terminal arbitration, and ordering between decode and renderer lifetimes. | Decode/timing worker, renderer, terminal event bridge | JNI entry → coordinator → workers/sinks | Host UI, Android navigation, codec promises, or GL resource implementation | Required by AC-2, AC-7, and stop-before-release risk. |
| FFmpeg decode/timing worker | proposed | Own the file/demuxer/decoder, packets, decoded frames, conversion resources, timestamp normalization, pacing, and cleanup for one attempt. | Renderer only for due RGBA frames; coordinator for outcomes | Coordinator → FFmpeg → renderer boundary | Kotlin callbacks, EGL context, surface/window ownership, or persistent state | FFmpeg libraries are already linked (`CMakeLists.txt:47`); no production decode exists (`videolib.cpp:8`). |
| Preview renderer | observed + narrow extension | Continue owning `ANativeWindow`, EGL display/surface/context, GLES objects, and its render thread; synchronously consume RGBA and report whether presentation succeeded. | GLES program on its executor | Playback worker → renderer executor → GLES | FFmpeg demux/decode resources, playback clock, public callbacks, or attempt terminal policy | `preview_renderer.h:4`, `preview_renderer.cpp:102`; presentation-result feedback is needed for FR-4. |
| Terminal event bridge | proposed responsibility at JNI/facade boundary | Transfer only the winning semantic completion/error outcome to a live Kotlin owner, with safe cross-thread reference handling and main-thread delivery. | Kotlin facade dispatcher | Native coordinator → JNI → Kotlin main | Decode/render resources, retry, or host business decisions | No current callback contract; native-worker callback safety is a declared high-risk boundary. |

No DI framework is introduced. Construction stays private to the existing per-`VideoPreview` native ownership boundary; `:videolib` has no established DI layer.

## 4. End-to-end data flow

Source of truth for an active attempt is the native playback coordinator. Kotlin owns host lifecycle intent; the decode worker owns media resources; `PreviewRenderer` owns only surface/render state.

### Flow A — accepted playback (AC-1, AC-6, AC-8)

| Step | Participant | Input / source | Decision / transformation | Output / side effect | Error propagation |
| --- | --- | --- | --- | --- | --- |
| 1 | Host → facade | Local filesystem path; attached surface | Validate live handle, non-empty path, ready surface, and no active attempt. | Accepted/rejected request outcome. | Rejection starts no attempt and emits no terminal event. |
| 2 | Coordinator | Accepted path and new attempt identity | Make the attempt current and move to Starting. | Decode worker begins without blocking caller or GL thread. | Startup failure is terminal only after acceptance. |
| 3 | Decode worker | Caller-owned path copied across JNI | Open local input, identify a decodable video stream, initialize decode ownership. | Playing state and stream timing basis. | Open/no-stream/unsupported failures become one semantic error. |
| 4 | Decode worker | Encoded packets and decoded frames | Decode video only, drain delayed frames at EOS, normalize non-decreasing presentation timing. | Ordered video frames with due times. | Corruption/decode/timing failure becomes one error. |
| 5 | Decode worker | Due decoded frame | Convert into owned, tightly packed RGBA8888; keep storage valid through synchronous presentation. | Renderer-ready pixels and dimensions. | Allocation/conversion failure becomes a decode error. |
| 6 | Renderer | RGBA frame; current attached EGL surface | Serialize onto the render thread, make the context current, upload/draw/swap using the existing full-surface quad. | Visible frame; synchronous success/failure back to coordinator. | EGL/GL/surface failure becomes a render error. |
| 7 | Coordinator | Render completion and current attempt identity | Release/advance frame ownership, ignore stale results, pace or skip only late intermediate frames. | Continued playback with bounded ownership. | Cancellation prevents later submissions. |

### Flow B — natural end-of-stream (AC-3, AC-7)

| Step | Participant | Input / source | Decision / transformation | Output / side effect | Error propagation |
| --- | --- | --- | --- | --- | --- |
| 1 | Decode worker | Demuxer EOS | Drain delayed decoder output and identify the final decoded frame. | Final frame follows Flow A presentation. | Drain/decode failure follows AC-4. |
| 2 | Coordinator | Successful final presentation; attempt still current | Atomically select natural completion as terminal winner. | Stop scheduling, release FFmpeg resources, retain renderer texture. | Stop/release/surface loss winning first suppresses completion. |
| 3 | Event bridge / facade | Winning completion outcome | Validate live/current attempt and serialize to main. | Exactly one caller completion notification. | Stale or cancelled delivery is discarded. |

### Flow C — explicit stop or release (AC-2, AC-7)

| Step | Participant | Input / source | Decision / transformation | Output / side effect | Error propagation |
| --- | --- | --- | --- | --- | --- |
| 1 | Host → facade → coordinator | Stop or release intent | Cancellation wins for the current attempt; invalidate terminal delivery and prevent new render submissions. | Decode/timing waits are interrupted or allowed to reach a safe cancellation point. | No completion/error is emitted for caller cancellation. |
| 2 | Coordinator / worker | In-flight decode or synchronous render | Join the ownership boundary only after borrowed buffers and render calls finish. | Attempt resources released; no post-barrier frame/callback. | Cleanup errors remain internal diagnostics after callback invalidation. |
| 3 | Coordinator / renderer | Stop versus release | Stop returns to reusable Idle; release additionally tears down EGL/window and native ownership in order. | Stable reusable or Released state. | Idempotent repeated calls do nothing. |

### Flow D — failure or surface loss (AC-4, AC-7)

| Step | Participant | Input / source | Decision / transformation | Output / side effect | Error propagation |
| --- | --- | --- | --- | --- | --- |
| 1 | Decode worker or renderer | Open/decode/conversion/presentation failure | Report semantic phase with attempt identity; do not continue frames. | Coordinator arbitrates terminal outcome. | Only the first current terminal failure can win. |
| 2 | Coordinator | Winning error | Cancel remaining work and release attempt-owned resources before exposing terminal state. | Failed state. | Surface-related failure also removes render readiness through ordered teardown. |
| 3 | Event bridge / facade | Winning error and live callback owner | Convert native details to the stable semantic error contract and dispatch on main. | Exactly one error notification. | Native diagnostic codes may be logged but are not public compatibility values. |

Offline behavior is inherent: only a readable local filesystem path is opened and no network fallback exists. There is no persistence, cache, or reconciliation boundary.

## 5. Boundary contracts

| Contract / boundary | Observed / proposed / blocked | Semantic input | Output / result | Invariants | Errors | Compatibility / versioning | Owner |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Host → preview facade | proposed extension | Local filesystem path plus existing surface/lifecycle state | Immediate accepted/rejected start outcome; idempotent stop | Single owner thread remains required for public calls; host owns path readability and surface lifecycle | Invalid lifecycle/path shape rejects synchronously; accepted operational failures notify asynchronously | Additive to existing public API; no `content://`, URL, pause, seek, loop, or audio promise | `:videolib` Kotlin facade |
| Facade → JNI coordinator | proposed | Copied path, native handle, lifecycle commands, live event receiver | Accepted/rejected status and asynchronous semantic terminal events | Exact declarations/exports and types match; strings are copied/released within JNI; no `JNIEnv*` crosses threads | Zero/invalid handle rejects safely | Preserve existing JNI exports; new exports are internal native ABI details packaged in the same library | `:videolib` JNI boundary |
| Coordinator → event bridge | proposed | Current attempt identity plus completion or semantic error phase | At-most-one event eligible for main-thread delivery | Retained callback references are valid only while facade lives; worker threads attach/detach to the JVM as needed; global references are released once | Stale/cancelled/released events are dropped | Main-thread delivery and terminal exclusivity are public runtime behavior | Native coordinator / Kotlin facade |
| Coordinator → FFmpeg decode worker | proposed | Accepted copied path, cancellation state | Ordered decoded video frames, EOS, or semantic error | One worker owns and releases format/codec/packet/frame/conversion resources for its attempt; no resource crosses attempts | Open, no-video, unsupported, decode, conversion, timing | FFmpeg numeric codes and exact supported formats are not stable public values | Native playback coordinator |
| Decode worker → renderer | proposed over observed seam | Due tightly packed RGBA8888 pixels, positive dimensions, current attempt identity | Synchronous presentation success/failure | Pixel storage outlives the synchronous call; presentation order is non-decreasing; GL/EGL calls stay on renderer thread | Missing surface, EGL/current-context, upload/draw/swap failure | Preserves existing RGBA/top-left/full-surface behavior; no OES/YUV shader contract is added | Decode worker supplies; renderer consumes |
| Host surface → renderer | observed + extended lifecycle rule | Host-owned `Surface` converted to one native-window reference | Ready rendering boundary or attach failure | Renderer owns and releases exactly its acquired native-window reference; GL objects are created/destroyed with context current; teardown is idempotent | Attach/init failure rejects readiness; active loss terminates playback as render error | Existing attach/detach behavior remains; final-frame guarantee ends with that surface lifecycle | Preview renderer |
| Bundled FFmpeg / native package | observed | ABI-selected static archives and headers | Symbols linked into `libvideolib.so` inside AAR/APK | FFmpeg 7.1 pin, two ARM ABIs, minSdk 21, NDK 29.0.14206865, C++17, and 16 KB alignment remain coherent | Link/package mismatch is a build/runtime load failure, not a playback callback contract | No archive upgrade, ABI expansion, license-profile change, or publication change in this feature | `:videolib` build/native boundary |
| Existing preview operations | observed | Surface attach/detach, direct RGBA push, test pattern, release, `NativeLib` calls | Existing outputs and safe no-op/idempotent behavior | Playback coordination cannot take over caller-frame ownership or alter existing JNI names | Existing validation remains unchanged | Source/binary/runtime regression prohibited | `:videolib` |

## 6. Conditional cross-cutting design

### Concurrency and lifecycle

- The Kotlin facade remains single-owner-thread for commands; only terminal notifications are marshalled asynchronously to main.
- Decode/open/pacing owns a dedicated native worker per active facade or an equivalent serialized worker owned by that facade. It cannot run on the JNI caller or renderer executor.
- The coordinator is the sole writer of playback state and terminal ownership. Surface state changes, stop, EOS, error, and release cross that serialization point before producing externally visible effects.
- Rendering is a synchronous ownership hand-off onto the existing renderer executor. This bounds pixel lifetime and prevents a queued pointer from outliving FFmpeg/conversion storage.
- Teardown order is playback cancellation barrier → decode resource release → callback invalidation/reference release → EGL/GL teardown with context current → native-window release → native owner destruction. A worker must not join itself; callback delivery must not hold native lifecycle locks while invoking consumer code.

### Native and FFmpeg ownership

| Resource | Sole owner | Handoff / release invariant |
| --- | --- | --- |
| Copied input path | Current playback attempt | Never retain JVM string storage; release with attempt. |
| Format/decoder contexts and packets | Decode worker | Created and destroyed on the decode lifecycle; never accessed after stop barrier. |
| Decoded FFmpeg frame | Decode worker | Retained only through conversion/decision; unreferenced or freed on every path. |
| RGBA conversion storage | Decode worker / current attempt | Valid until synchronous renderer consumption returns; not queued as a borrowed raw pointer. |
| EGL display/surface/context and GLES objects | Preview renderer | Access only on render executor with the correct context current; destroy there before executor shutdown. |
| Native window reference | Preview renderer | Exactly the reference obtained for the attached surface, released once after EGL teardown. |
| JVM callback reference | Terminal event bridge | Global only while asynchronous reachability is needed; released once on replacement/release and never invoked from a stale attempt. |

The decode path uses the bundled `libavformat`, `libavcodec`, `libavutil`, and `libswscale` capabilities already linked by the module. Decode must consume delayed frames at EOS before declaring completion. Audio streams are ignored rather than decoded or synchronized. No FFmpeg source/archive mutation, rebuild, new dependency, or link-profile change is part of this design.

### Rendering and performance

- Preserve one EGL/GLES context and one render executor. Do not introduce `SurfaceTexture`, external-OES textures, a second EGL context, or direct GL calls from decode/JNI threads.
- Decode may run ahead only within bounded native ownership; it cannot build an unbounded frame queue. Timing waits must be cancellable so stop/release is a practical barrier.
- Frame dropping applies only to late intermediate frames. The first presentable frame and final decoded frame are not skipped solely for lateness; completion follows successful final presentation.
- The current renderer stretches the coded RGBA raster over the surface. Any later aspect, rotation, crop, or color-management policy is a separate observable contract.

### Error and observability contract

Public failure phases are stable semantic categories: input/open, unsupported-or-missing-video, decode/conversion/timing, and render/surface. Diagnostics may retain FFmpeg/EGL codes internally, but consumers cannot branch on those unstable native values. Logs must exclude the caller's full path unless explicitly safe because paths may contain user-sensitive names.

### Compatibility and rollback boundary

The feature is additive behind new public entry points. Existing preview/frame/pattern calls remain usable without configuring playback callbacks. Because there is no persistence, remote contract, schema, or publication change, rollback consists of removing the additive playback exposure and native coordinator while retaining the existing renderer and FFmpeg linkage. An implementation must not require consumer migration to make that rollback safe.

### Architectural verification obligations

| Proof level | Obligation | What it can establish | What it cannot establish |
| --- | --- | --- | --- |
| Library compile/link | `./gradlew :videolib:assembleDebug` | Kotlin/JNI/CMake compilation and static FFmpeg/EGL/GLES linkage for configured variants/ABIs | Device library loading, callback races, playback timing, or visible GL correctness |
| Direct consumer integration | `./gradlew :app:assembleDebug` | Source compatibility of the in-repository consumer and APK native merge | External-consumer binary behavior or device playback |
| Package/native inspection | Inspect produced AAR/APK and native dependencies/alignment for both configured ABIs | Presence of `libvideolib.so`, supported ABI set, FFmpeg symbol resolution expectations, and 16 KB packaging/link properties | Runtime decoder/renderer correctness |
| Supported-device/ABI run | Exercise the public lifecycle on `arm64-v8a` and `armeabi-v7a` where hardware is available | Local playback output, stop barrier, EOS/failure callbacks, surface teardown/recreation, JVM thread attachment, and EGL behavior | Unsupported codecs outside the bundled capability |

No UI toolkit, navigation, accessibility, durable storage, DI, permissions, backend API, or design-conformance contract is crossed by the approved feature.

## 7. Coverage audit

| Source item | Covered by | Status |
| --- | --- | --- |
| FR-1 / SC-1 / ST-1 | AC-1, AC-5, AC-6, AC-8, AC-9; Flows A and Boundary Contracts | Complete |
| FR-2 / SC-2 / ST-2 | AC-2, AC-7, AC-9; Flow C and lifecycle transitions | Complete |
| FR-3 / SC-3 / ST-3 | AC-3, AC-7, AC-9; Flow B and Completed state | Complete |
| FR-4 / SC-4 / ST-4 | AC-4, AC-5, AC-7, AC-8, AC-9; Flow D and error contract | Complete |
| Empty/missing/unreadable/non-file path | AC-1, AC-4; host/facade and FFmpeg contracts | Complete: empty input rejects; accepted unreadable/non-file input reports input/open failure. |
| No video, corrupt, unsupported input | AC-4; Flow D; FFmpeg contract | Complete: one semantic unsupported/decode error, no silent completion. |
| Play before surface | AC-5; Surface prerequisite decision | Complete: reject without starting or callback. |
| Surface destruction / EGL failure | AC-4, AC-7; Flow D | Complete: render error unless stop/release already won. |
| Stop versus decode/render race | AC-2, AC-7, AC-8; Flow C | Complete: synchronous cancellation barrier with no later frame/callback. |
| Repeated stop / stop while idle | AC-2; state model | Complete: safe no-op. |
| Second play while active | AC-5; Overlapping-play decision | Complete: reject and preserve active attempt. |
| EOS versus stop/release race | AC-3, AC-7; Flow B | Complete: one winner; caller cancellation suppresses completion. |
| Native callback ownership/thread | AC-4, AC-7; event bridge and JNI contracts | Complete: at-most-once, live reference, main-thread delivery, stale suppression. |
| Codec/container matrix | Format-guarantee decision; bundled-native contract | Complete as intentionally capability-based, with no new fixed matrix promised. |
| Timing/drop policy | AC-6; Rendering and performance | Complete at architectural level; numerical threshold intentionally unspecified. |
| Rotation/SAR/scaling/color behavior | Visual-mapping decision | Complete as explicitly outside scope; current coded-orientation/full-surface behavior preserved. |
| Public/JNI/native compatibility | AC-9; Module and Boundary Contract matrices | Complete |
| Design references | None supplied | Complete; no UI/design evidence exists. |

Unresolved inputs required to complete the design: none.
