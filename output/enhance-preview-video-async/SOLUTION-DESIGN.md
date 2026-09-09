AUTOMATION: CONTINUE

# SOLUTION-DESIGN — enhance-preview-video-async

## 1. Decision ledger

### Sources and investigation control

| ID | Evidence/decision | Status | Impact |
| --- | --- | --- | --- |
| SRC-1 | `DEV-SPEC.md` defines FR-1/SC-1 as progressive presentation before complete-video decode and FR-2/SC-2 as a `:videolib`-only change boundary. | observed | Normative product and scope source. |
| SRC-2 | `VideoPreview.play` is the public entry; native playback owns a decode worker; decoded frames are converted and submitted inside the packet loop; EGL/GLES work is serialized on a render executor. | observed | Confirms an existing seam that already structurally supports progressive presentation. Evidence: `VideoPreview.kt:52-85`; `video_playback.cpp:146-173,300-506`; `preview_renderer.cpp:102-135`. |
| SRC-3 | `app` directly depends on `:videolib` and invokes `VideoPreview.play`; public library consumers outside the repository are unknown. | observed | Requires source/runtime compatibility and consumer verification without changing `app`. Evidence: `app/build.gradle.kts:55`; `MainActivity2.kt:209-235`; DEV-SPEC §6. |
| SRC-4 | The design crosses Kotlin/JNI, native worker ownership, FFmpeg frame/conversion ownership, EGL thread affinity, ARM ABI packaging, and 16 KB page alignment. | observed | Protected native/public contracts require explicit compatibility, rollback, and verification obligations. Evidence: DEV-SPEC §§6–9; `videolib.cpp:18-119,146-255`; `videolib/build.gradle.kts:9-31`; `CMakeLists.txt:8-68`. |
| DEPTH | Deep cap selected because the packet declares public/JNI/native/ABI risk; approximately 8 material lookups of the 20-lookup cap were used. The current graph was queried first, then Kotlin, JNI, C++, FFmpeg, EGL, Gradle, CMake, and consumer edges were source-validated. | observed | Sufficient evidence; no escalation required. |

### Decisions

| ID | Decision | Status | Rationale/impact |
| --- | --- | --- | --- |
| D-1 | `videolib` remains the sole implementation owner; `app` and external consumers remain consumers of the existing library contract. | derived | Satisfies FR-2/SC-2 and avoids moving reusable native playback behavior into the sample host. |
| D-2 | Retain the existing two-execution-boundary model: one playback/decode worker and one serialized EGL/GLES renderer. Do not add a module, service, repository, DI layer, or parallel public abstraction. | derived | It is the smallest decomposition that satisfies progressive playback and matches repository structure. |
| D-3 | An accepted playback request returns before input open, stream discovery, decoding, conversion, pacing, or frame presentation. Decode begins on the playback worker. | derived | Separates caller responsiveness from media work and supports AC-1. |
| D-4 | Decode and presentation are progressive: after a frame becomes decodable and reaches its presentation time, that frame crosses to the surface before the decoder continues toward full-file completion. The first decodable frame uses the playback clock origin and does not wait for EOF. | derived | Directly satisfies the clarified visible outcome in SC-1. |
| D-5 | Keep at most one decode-produced RGBA frame in the decode-to-render handoff. Decode may wait for that frame's presentation to complete; it must not build an unbounded queue. | derived | Supplies bounded backpressure, prevents stale-frame latency, and preserves the current buffer-lifetime invariant. Independent decode/render buffering is unnecessary for the approved outcome. |
| D-6 | Preserve the existing public behavior: valid-surface and single-active-attempt preconditions, Boolean acceptance, terminal callback/error categories, main-thread listener delivery, cancellation suppression, and idempotent lifecycle calls. | derived | No public contract break is authorized, and external consumers cannot be enumerated. |
| D-7 | Stop/release/surface loss invalidates the attempt before teardown; no later frame or terminal callback from the invalidated attempt may become observable. EGL/window release remains ordered after playback work is quiescent. | derived | Resolves lifecycle races without weakening the current stop-before-release guarantee. |
| D-8 | Preserve current FFmpeg static linkage, Kotlin/JNI names and signatures, JavaVM callback handling, supported ARM ABIs, min SDK 21, and 16 KB page compatibility. | derived | The feature changes runtime behavior only; build/publication or ABI migration is outside scope. |
| D-9 | Treat the current source as the target architecture because it already performs per-frame presentation inside ongoing decode. Any implementation delta is justified only by runtime evidence that AC-1 through AC-5 are not met. | derived | Avoids inventing a second pipeline when the clarified behavior is structurally present. |

### Explicitly unspecified choices

| Item | Status | Impact |
| --- | --- | --- |
| Numeric time-to-first-frame threshold | intentionally unspecified | The source gives no latency target; conformance proves progressive presentation, not a duration guarantee. |
| Exact native thread/queue primitive and frame allocation optimization | intentionally unspecified | Implementation-local as long as D-2, D-5, ownership, and teardown contracts hold. |
| Frame-dropping policy for a future multi-frame buffer | intentionally unspecified | This design does not introduce such a buffer. Adding one would require a new latency/memory behavior decision. |
| Audio playback, network sources, seeking, pause/resume, looping, and background playback | intentionally unspecified and out of scope | The approved source covers local, video-only progressive preview. |
| UI layout, status copy, and navigation | intentionally unspecified and out of scope | No design/UI change is required; `app` remains a verification consumer. |

### Blockers

None. The lack of a numeric latency target limits performance claims but does not block the clarified progressive-presentation contract.

## 2. Behavior and state transitions

### Behavior contract

| FR-ID | SC-ID | AC-ID | Story-ID | Design-Ref | Rule/trigger | Observable outcome | Failure/recovery |
| --- | --- | --- | --- | --- | --- | --- | --- |
| FR-1 | SC-1 | AC-1 | Story-1 | N/A | A caller starts a valid local video while a usable surface exists and no attempt is active. | The request reports acceptance without waiting for input probing, complete decode, or frame presentation; media work proceeds off the caller thread. | Invalid input state or worker-start failure rejects the attempt without creating an active playback lifecycle. |
| FR-1 | SC-1 | AC-2 | Story-1 | N/A | FFmpeg yields the first decodable video frame. | That frame is converted, paced from the playback clock origin, and presented on the surface before complete-video decode/EOF. | Decode/conversion failure produces the existing decode outcome; presentation failure produces the existing render outcome. |
| FR-1 | SC-1 | AC-3 | Story-1 | N/A | Subsequent decoded frames become available. | Frames are presented progressively in non-decreasing presentation order, with at most one decode-produced frame crossing the renderer boundary at a time. | Missing/irregular timestamps use a monotonic fallback; cancellation interrupts pacing and stops further presentation. |
| FR-1 | SC-1 | AC-4 | Story-1 | N/A | Stop, release, or surface loss occurs during starting, pacing, conversion, or presentation. | The attempt becomes invalid before teardown; after the cancelling lifecycle call completes, no frame or listener event from that attempt is observable. | Surface loss retains the existing render-failure semantics when the attempt has not already been explicitly cancelled. A later attempt requires a usable surface. |
| FR-1 | SC-1 | AC-5 | Story-1 | N/A | An accepted attempt reaches EOF or a terminal input/unsupported/decode/render failure. | Exactly one non-cancelled terminal outcome is delivered through the existing listener contract on the main thread; a later attempt may start when lifecycle preconditions are again valid. | Cancellation wins over a competing terminal event and suppresses the stale outcome. |
| FR-2 | SC-2 | AC-6 | — | N/A | The progressive-preview behavior is implemented or verified. | Production changes, if runtime evidence shows they are needed, remain inside `:videolib`; `app` requires no source change. | A required change to another module or public contract is outside this design and would require renewed scope approval. |

### State model

Playback-attempt state and surface readiness are separate state dimensions owned by the native playback boundary. A playback attempt requires a ready surface but does not own the host's `Surface` lifecycle.

| State | Meaning/invariants | Permitted events | Prohibited/ignored events |
| --- | --- | --- | --- |
| Idle | No active attempt; native owner is reusable if the surface dimension is Ready. | Attach/replace surface; accept play when surface is Ready; stop as no-op; release. | Play is rejected when the surface is not Ready. |
| Starting | An attempt ID is active; FFmpeg open/probe/decoder setup runs off the caller thread; no terminal outcome claimed. | Decoder-ready transition; stop; surface loss; release; terminal setup failure. | A second play request is rejected. |
| Playing | Decoder is producing frames progressively; only the current attempt may pace and present; at most one decoded frame is in the renderer handoff. | Present next frame; EOF; stop; surface loss; release; terminal decode/render failure. | A second play request and stale-attempt frames/events are rejected. |
| Stopping | Cancellation is claimed and wait/input operations are interrupted; no new frame or listener result may be admitted. | Worker quiescence, then Idle or Released. | New presentation and terminal notification from the cancelled attempt are ignored. |
| Completed | The current attempt reached EOF after presenting at least one frame and its terminal outcome was claimed. | Re-enter Starting for a new accepted attempt when surface is Ready; surface change; release. | Duplicate completion/error for the prior attempt is ignored. |
| Failed | A terminal input, unsupported-video, decode, or render failure was claimed. A render failure also makes the surface not ready. | Re-enter Starting only when required preconditions are restored; surface attach; release. | Duplicate terminal outcome and stale frames are ignored. |
| Released | Native owner, playback work, callback ownership, surface, and EGL resources are no longer usable. | Repeated release/stop/detach as safe no-ops. | Play, frame presentation, and surface work are rejected or ignored. |

Surface dimension:

| State | Meaning/invariants | Permitted events | Prohibited/ignored events |
| --- | --- | --- | --- |
| Detached | No usable native window/EGL surface. | Attach a valid surface; release. | Playback acceptance and frame presentation. |
| Ready | Native window and EGL/GLES context are usable on the render thread. | Start playback; replace/detach surface; release. | EGL/GLES work from any non-render thread. |
| Failed | Surface initialization or presentation failed; partial resources are released. | Attach a valid surface to recover; release. | Playback acceptance until recovery succeeds. |

### Transition contract

| From | Event/precondition | To | Side effect | Failure/cancellation/recovery |
| --- | --- | --- | --- | --- |
| Idle/Completed/Failed | Play requested; owner valid, path non-blank, surface Ready, no active attempt | Starting | Claim a new attempt and start media work off the caller thread; return accepted. | If claim/worker start fails, remain/recover Idle and return rejected. |
| Starting | Input opens, a supported video stream/decoder is ready, and attempt remains current | Playing | Establish pacing origin when the first frame arrives. | Input/unsupported/decode failures transition to Failed and claim AC-5. |
| Playing | A decoded frame reaches its presentation deadline and the attempt/surface remain valid | Playing | Convert to renderer input, present on EGL thread, then release/reuse frame storage before continuing decode. | Cancellation suppresses the frame; render failure transitions to Failed and invalidates surface readiness. |
| Starting/Playing | Stop requested | Stopping | Invalidate listener/attempt, signal interrupt/wait cancellation, and quiesce worker. | On quiescence, transition to Idle without a terminal callback. |
| Starting/Playing | Surface detached or becomes unusable without prior explicit stop | Stopping | Prevent new frames, quiesce worker, then release EGL/window in order. | Claim the existing render-failure outcome, then transition to Failed with surface not ready. |
| Starting/Playing/Stopping | Release requested | Released | Invalidate callbacks/attempt, interrupt and join playback work, release EGL/window, then destroy native ownership. | Repeated release is a no-op; no recovery on the same instance. |
| Playing | EOF after at least one successfully presented frame | Completed | Claim exactly one completion result and deliver it on the main thread. | EOF without a presented frame uses the existing decode-failure outcome. |
| Completed/Failed | A new valid play request with surface Ready | Starting | Retire/join the finished worker and claim a new attempt ID. | A stale result from the prior attempt cannot match the new attempt ID. |

## 3. Components and responsibilities

### Module Contract Matrix

| Module | Owner/consumer | Responsibility | Depends on | Crossed contract | Compatibility obligation | Verification obligation |
| --- | --- | --- | --- | --- | --- | --- |
| `videolib` | Owner; only permitted changed module | Own public video-preview lifecycle and the Kotlin-to-native FFmpeg/EGL implementation | Android `Surface`, JNI, FFmpeg 7.1 static archives, swscale, EGL/GLES, C++ runtime | Public Kotlin source/runtime; JNI names/callbacks; native ownership; frame memory; EGL affinity; CMake/ABI packaging | Preserve public signatures and behavior, callback/error semantics, supported ABIs, min SDK, and native packaging unless separately approved | Compile/link the module; validate native/JNI loading; prove progressive surface output and lifecycle behavior on supported devices/ABIs; inspect packaged alignment/linkage |
| `app` | Direct in-repository consumer; unchanged | Supplies the local path, surface lifecycle, and listener; displays consumer-visible status | `implementation(project(":videolib"))` | Kotlin source/runtime and packaged native library | Continue compiling and running without source modification | Compile/package the host and use it for supported-device behavioral observation |
| External AAR consumers | Unknown consumers; unchanged | Integrate the public `videolib` contract | Published/consumed library artifact outside repository visibility | Source, binary, runtime behavior, JNI/ABI | No incompatible API or lifecycle semantic change | Covered indirectly by compatibility checks; consumer inventory remains unavailable |

### Component responsibilities

| Component role | Observed/proposed | Responsibility/owned state | Delegates to | Dependency direction | Must not own/know | Evidence/decision |
| --- | --- | --- | --- | --- | --- | --- |
| Host consumer | observed, retained | Owns the Android `Surface` lifecycle, readable local path, user-visible controls/status, and release timing | Public preview facade | Host -> `videolib` public API | FFmpeg contexts, native attempt state, EGL internals | `MainActivity2.kt:202-235`; D-1 |
| Public preview facade | observed, retained | Owns the opaque native handle, surface-attached view, active listener/attempt correlation, and main-thread terminal delivery | JNI playback boundary | Kotlin public facade -> private JNI | Decode loop, frame memory, GL resources | `VideoPreview.kt:25-220`; D-6 |
| Native playback coordinator | observed, retained | Sole source of truth for playback attempt ID/state, cancellation claim, worker lifetime, surface readiness, and terminal arbitration | FFmpeg decode/conversion worker and surface renderer | JNI boundary -> coordinator -> native participants | Host UI state or Android main-thread dispatch | `video_playback.h:40-89`; D-2, D-7 |
| FFmpeg decode/conversion worker | observed, retained | Owns per-attempt FFmpeg resources, packet/frame loop, timestamps, pacing, RGBA conversion storage, and incremental emission | Surface renderer, one frame at a time | Coordinator -> decode -> renderer | Kotlin listener state, EGL context ownership, full-file frame buffering | `video_playback.cpp:28-49,300-506`; D-3 through D-5 |
| Surface renderer and GL executor | observed, retained | Owns native window reference, EGL display/surface/context, GL program, presentation result, and single-thread GL execution | Android/EGL/GLES platform | Decode/coordinator -> renderer -> platform surface | FFmpeg demux/decode state, public callbacks, an unbounded frame queue | `preview_renderer.h:25-71`; `preview_renderer.cpp:77-190`; D-2, D-5 |
| JNI callback bridge | observed, retained | Owns the Java global reference and method IDs; derives thread-local JNI environment and transfers one terminal result to Kotlin | Public preview facade callbacks | Native worker -> bridge -> Kotlin facade | Playback policy, Android UI state, cached cross-thread `JNIEnv*` | `videolib.cpp:20-119`; D-6, D-8 |

No new DI/composition framework is introduced. Each preview instance continues to own its native playback coordinator directly through its opaque handle.

## 4. End-to-end data flow

### Flow A — accepted progressive playback

| Step | Participant | Input/source | Decision/transformation | Output/side effect | Error propagation |
| --- | --- | --- | --- | --- | --- |
| 1 | Host consumer | Valid attached surface, readable local path, listener | Requests playback through the existing public contract | Immediate accepted/rejected result | Rejection creates no active attempt and no terminal callback. |
| 2 | Public preview facade/JNI boundary | Accepted request | Copies the path across JNI and associates the listener with the native attempt identity | Native coordinator receives caller-independent path ownership | JNI/native-owner failure rejects the request. |
| 3 | Native playback coordinator | Path and current ready surface | Atomically claims one attempt, resets cancellation, and starts playback work outside the caller thread | State becomes Starting; caller is released under AC-1 | Worker-start failure rolls back to Idle/rejected. |
| 4 | FFmpeg worker | Local path | Opens input, discovers the best video stream, configures decoder, then reads packets incrementally | Decoded `AVFrame` values become available without full-file accumulation | Input/unsupported/decode category is claimed once. |
| 5 | FFmpeg worker | Current decoded frame and source timestamp | Derives non-decreasing relative presentation time; missing timestamps use the existing fallback; waits interruptibly until due | Frame becomes eligible for presentation | Cancellation exits without exposing a stale frame. |
| 6 | FFmpeg worker | Decoded pixel format/planes | Converts the current frame to owned RGBA storage for the duration of the handoff | One renderer input frame | Conversion failure becomes decode failure. |
| 7 | Surface renderer | RGBA frame, dimensions, ready EGL surface | Serializes upload/draw/swap on the EGL thread and reports completion before the RGBA storage is reused | Visible frame on the surface under AC-2/AC-3 | EGL/GL/swap failure becomes render failure and invalidates surface readiness. |
| 8 | FFmpeg worker | Presentation result | Releases the current `AVFrame`, then resumes packet/frame decoding | Repeats steps 5–7 while the video remains incomplete | At most one decode-produced frame is in handoff; no reconciliation store exists. |
| 9 | Coordinator/callback bridge/facade | EOF after a presented frame | Claims one completion result, invokes Kotlin through a thread-valid JNI environment, and posts to main | Viewer/host receives completion after the final frame | Stale/cancelled attempt IDs suppress delivery. |

Source of truth is the native coordinator's current attempt identity plus cancellation/terminal claim. Frame data is ephemeral per attempt; there is no persistence, remote source, offline reconciliation, or full-video frame cache.

### Flow B — cancellation or surface loss

| Step | Participant | Input/source | Decision/transformation | Output/side effect | Error propagation |
| --- | --- | --- | --- | --- | --- |
| 1 | Host/facade | Stop, detach, or release | Invalidates the Kotlin listener/attempt correlation before native teardown | Stale native outcomes cannot reach the consumer | Repeated lifecycle calls remain safe. |
| 2 | Native coordinator | Lifecycle event | Claims cancellation/terminal ownership, clears the active attempt, signals interruptible I/O/pacing, and prevents new presentation | Decode worker exits and is joined/quiesced | Explicit stop/release suppresses terminal callback; surface loss retains render error when applicable. |
| 3 | Surface renderer | Quiesced playback and owned native window/EGL resources | Tears down GL resources with the EGL context current, then releases the window reference | Surface becomes Detached/Failed or owner becomes Released | Reattach is the recovery path unless the owner was released. |

## 5. Boundary contracts

| Contract/boundary | Observed/proposed/blocked | Semantic input | Output/result | Invariants | Errors | Compatibility/versioning | Owner |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Host -> public preview | observed, retained | Local path, attached `Surface`, terminal listener | Immediate acceptance plus later terminal outcome for an accepted non-cancelled attempt | One active attempt; video-only; host owns path/surface lifecycle; terminal listener delivery occurs on main | Existing input-open, unsupported-video, decode, render categories | No source/binary/runtime break; no new public API | `videolib` Kotlin facade |
| Kotlin facade -> JNI playback | observed, retained | Opaque valid handle and copied path/lifecycle command | Attempt ID/acceptance or lifecycle completion | Kotlin declarations and hand-mangled exports remain exact; string chars are released; handle lifetime is instance-bound | Invalid handle/input rejects or no-ops safely | Kotlin and native sides ship/roll back together in one library artifact | `videolib` JNI boundary |
| Native worker -> Kotlin callback | observed, retained | Attempt ID and optional stable error code | Exactly one main-thread listener result when not cancelled | Worker derives `JNIEnv*` from cached `JavaVM`; global ref survives until native owner destruction; stale attempt IDs are filtered | JNI callback exception is contained; cancellation suppresses delivery | Method names/signatures and numeric error mapping remain stable | JNI bridge + Kotlin facade |
| FFmpeg decode ownership | observed, retained | Caller-owned readable local file path | Incremental decoded frames and terminal decode status | All format/codec/packet/frame/sws resources are per attempt; packet/frame refs are released; no complete-video frame cache | Open, unsupported stream/decoder, packet/frame, timing, conversion failures | Bundled FFmpeg version/link set is unchanged | FFmpeg worker |
| Decode -> renderer frame handoff | observed, retained as design contract | One RGBA frame, width, height, current attempt | Presentation success/failure before producer storage is reused | At most one in-flight decode-produced frame; renderer does not retain producer memory past handoff completion | Invalid dimensions/data or GL presentation failure | Internal contract; may change implementation only if equivalent ownership/backpressure is proven | FFmpeg worker + renderer |
| Renderer -> Android surface | observed, retained | RGBA frame and ready native window/EGL context | GL upload/draw and buffer swap | Every EGL/GL create/use/destroy operation executes on the single render thread with context current; window ref released once | EGL/GL/swap failure marks renderer/surface failed | No host surface-ownership change | Surface renderer |
| Native build/package | observed, retained | `videolib` C++ sources and per-ABI FFmpeg static archives | One loadable `libvideolib.so` in the library artifact | `arm64-v8a` and `armeabi-v7a`; NDK 29; C++17; min SDK 21; 16 KB page settings; existing link group/order | Compile/link/load/alignment failures | No ABI set, soname/linkage, SDK, or publication-coordinate migration | `videolib` build boundary |

## 6. Conditional cross-cutting design

### Async/concurrency and memory ownership

- Caller, playback worker, render worker, and main callback thread remain distinct execution contexts.
- Attempt identity is the ordering token across Kotlin callbacks and native work. Only the current, non-cancelled attempt may present or terminate.
- The renderer boundary remains serialized and bounded to one decode-produced frame. This deliberately favors bounded memory and fresh playback progress over speculative decode-ahead.
- RGBA storage remains producer-owned until presentation completes. If an implementation later makes the handoff non-blocking, it must transfer independent ownership and retain the same one-frame bound; borrowing the current buffer would violate the contract.
- Cancellation must interrupt both source I/O and presentation pacing, then quiesce the decode worker before EGL/window/native-owner destruction.

### Performance contract

- Progressive visibility is normative: the first decodable frame is eligible for presentation before EOF/full decode.
- A numeric startup latency, decode-ahead depth, or frame-drop threshold is not normative because none is evidenced.
- Slow input probing or first-frame decode may still determine time to first frame; the design must not claim a latency guarantee that verification cannot support.

### Failure and recovery

- Input-open, unsupported-video, decode/conversion/timing, and render failures retain distinct existing public categories.
- Render failure invalidates surface readiness; a successful surface attachment is required before retry.
- Non-render terminal outcomes may retry on the same ready surface after the finished worker is retired.
- Cancellation is not reported as completion or failure and takes precedence over racing worker outcomes.

### Compatibility and rollback

- Public Kotlin types/signatures, JNI exports/callback signatures, error mapping, and lifecycle semantics remain unchanged for `app` and unknown external consumers.
- No data/schema migration, host migration, new permission, or dependency rollout exists.
- Rollback is atomic at the `videolib` artifact boundary: Kotlin facade and `libvideolib.so` must revert together. Mixing a facade and native binary from different revisions is unsupported because JNI names and lifecycle contracts are coupled.
- Publishing, distribution, upload, signing, and consumer rollout are outside this design and require separate authorization.

### Risk mitigation

| Risk | Blast radius | Severity | Mitigation contract |
| --- | --- | --- | --- |
| Producer buffer reuse before GL upload | Native decode/render; crash or corrupted frame | High | Preserve completion-bounded ownership and one in-flight frame; any non-blocking handoff owns an independent buffer. |
| Unbounded queued frames | Playback latency and native memory | High | Do not introduce an unbounded queue; retain one-frame backpressure. |
| Stop/detach/release race | JNI callbacks, EGL/window lifetime, consumer lifecycle | High | Invalidate attempt first, interrupt waits/I/O, quiesce worker, then release render/native resources; filter stale attempt IDs. |
| JNI signature/reference regression | All consumers; load/callback failure or leak | High | Keep exports/method descriptors/error values stable; preserve global-ref and attach/detach ownership. |
| Apparent progressive code but poor startup | SC-1 user experience | Medium | Prove that a visible frame occurs before full decode on device; make no unsupported numeric latency claim. |
| ABI/link/alignment regression | One or both supported ARM device families | High | Preserve ABI filters/link/page configuration and verify the packaged native artifact plus device load. |

### Architectural verification obligations

| Boundary | Evidence required | What it proves / does not prove |
| --- | --- | --- |
| `videolib` Kotlin/native build | `./gradlew :videolib:assembleDebug` | Proves compilation/linkage for configured variants/ABIs; does not prove visible GL output, timing, or lifecycle races. |
| JNI/native binding lifecycle | `./gradlew :videolib:connectedDebugAndroidTest` on a supported ABI | Proves packaged library load and exercised JNI bindings on that device; existing binding coverage does not alone prove progressive playback. |
| Direct consumer | `./gradlew :app:assembleDebug` | Proves `app` source/package compatibility; does not prove runtime frame visibility. |
| Progressive behavior and lifecycle | Supported-device playback observation using a video long enough to distinguish first-frame display from full decode, including cancellation and surface loss | Proves AC-1 through AC-5 at runtime for that device/ABI; does not establish a numeric startup target. |
| Native artifact compatibility | Inspect/load packaged `libvideolib.so` for `arm64-v8a` and `armeabi-v7a`, including 16 KB alignment | Proves package/link/alignment presence; only per-ABI device execution proves runtime FFmpeg/EGL behavior. |

## 7. Coverage audit

### Requirement and contract coverage

| Source item | Design coverage | Status |
| --- | --- | --- |
| FR-1 | D-2 through D-7; AC-1 through AC-5; Flows A/B; playback/surface states; decode, renderer, JNI, and lifecycle contracts | covered |
| SC-1 | AC-1 through AC-5, especially AC-2 progressive first-frame visibility before EOF | covered |
| Story-1 | AC-1 through AC-5 and Flow A | covered |
| FR-2 | D-1, D-8, AC-6, Module Contract Matrix | covered |
| SC-2 | AC-6; `videolib` is sole owner and `app` remains unchanged | covered |

### DEV-SPEC question/assumption coverage

| Source item | Decision or disposition | Status |
| --- | --- | --- |
| No numeric time-to-first-frame target | Progressive-before-EOF is normative; numeric latency remains intentionally unspecified. | resolved without blocker |
| Independent decoder-to-render buffering unspecified | D-5 chooses one-frame, completion-bounded handoff; no independent multi-frame buffer is needed. | resolved |
| Audio assumed out of scope | Explicitly retained as out of scope. | resolved |
| Existing callback/error/cancellation/single-attempt behavior assumed compatible | D-6/D-7 make preservation mandatory. | resolved |
| External consumers unavailable | Compatibility is preserved; no consumer migration is introduced. | resolved without inventory |
| Current code already structurally matches progressive presentation | D-9 adopts the current architecture; runtime evidence, not speculative redesign, determines whether an implementation delta exists. | resolved |

### AC coverage

| AC-ID | State/flow/boundary coverage | Status |
| --- | --- | --- |
| AC-1 | Starting transition; Flow A steps 1–3; host/public/JNI contracts | complete |
| AC-2 | Playing transition; Flow A steps 4–7; decode-render-surface contracts | complete |
| AC-3 | Playing self-transition; Flow A steps 5–8; concurrency/memory contract | complete |
| AC-4 | Stopping/Released transitions; Flow B; lifecycle and risk mitigation | complete |
| AC-5 | Completed/Failed transitions; Flow A step 9; failure/recovery and callback contracts | complete |
| AC-6 | Module Contract Matrix; compatibility and verification obligations | complete |

Unresolved inputs required to complete the design: none.

