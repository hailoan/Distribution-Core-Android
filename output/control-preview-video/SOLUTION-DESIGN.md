AUTOMATION: CONTINUE

# SOLUTION-DESIGN — control-preview-video

## 1. Decision ledger

### Sources and investigation control

| Item | Status | Decision / evidence | Impact |
| --- | --- | --- | --- |
| Approved behavior | Observed | `DEV-SPEC.md` begins with `AUTOMATION: CONTINUE` and defines FR-01–FR-06, SC-01–SC-07, and Story-01–Story-04. | All design contracts trace to the approved feature analysis. |
| Owning module | Decided | `:videolib` is the sole changed module; the ticket explicitly limits code scope to it. | No UI, app, benchmark, or unrelated library implementation is added. |
| Consumer closure | Observed | `:app` directly depends on `:videolib`; `:benchmark` targets `:app`; public-library consumers outside the repository cannot be enumerated. | Existing public behavior and native packaging remain compatible and consumer builds remain in verification scope. |
| Investigation depth | Observed | Deep budget selected for public JNI/native, FFmpeg timing/seek, concurrency, and EGL lifecycle risk. Approximately 9 focused design lookups of a 20-lookup cap were used; no escalation was needed. | Evidence was limited to the approved spec and the reached public/JNI/native/render/consumer boundaries. |
| Code graph | Observed | The immediately preceding feature-analysis preflight found no registered repository graph; this stage reused that current result and validated decisions through focused source inspection. | A missing graph edge is not used to claim absence of consumers. |
| Public compatibility | Decided | Existing attach, start, stop, detach, release, callback, and default playback behavior remain unchanged. New controls are additive; default speed is `1.0×` and looping defaults to disabled. | Existing consumers that do not call controls retain current behavior. |
| State ownership | Decided | The native per-preview playback owner is authoritative for attempt lifecycle, paused/playing state, media position, speed, loop mode, pending seek, EOF, and terminal claiming. The Kotlin facade owns caller-thread validation, native-handle/surface guards, and main-thread listener delivery only. | Avoids competing Kotlin/native playback state while preserving the current facade/JNI split. |
| Command model | Decided | Public controls synchronously report accepted/rejected; accepted media work remains asynchronous. Duplicate pause/resume or repeated configuration values are accepted idempotently. | Invalid state or values are observable without creating a new terminal callback protocol. |
| Configuration lifetime | Decided | Valid loop and speed settings may be changed before or during an attempt and persist on that `VideoPreview` instance across stop/completion and later attempts, until changed or released. | Hosts can configure short clips before start; defaults preserve compatibility. |
| Seek bounds | Decided | Accepted millisecond targets are clamped to the playable interval. The visible result is the first decodable frame at or after the clamped target, or the final decodable frame when no later frame exists. | Resolves negative and beyond-duration inputs without exposing FFmpeg keyframe mechanics. |
| Rapid seek policy | Decided | Accepted seeks are ordered by a monotonically newer request identity; pending older work may be superseded. An already-presenting frame may finish, but no stale seek result may replace the latest settled result. | Supports responsive scrubbing without concurrent FFmpeg access or stale final presentation. |
| High-speed policy | Decided | Every finite speed `≥0.1×` is accepted. Playback preserves media-time order and may omit late intermediate frames when decode/render cannot match the requested wall-clock rate; no numeric timing SLA is introduced. | Reconciles the confirmed unbounded maximum with finite device capacity. |
| Loop boundary | Decided | EOF reads the latest serialized loop setting. Enabled restarts the same attempt with no terminal event; disabled completes once. Disabling after a restart makes that new current pass the final pass. | Prevents duplicate/lost completion in the loop-disable/EOF race. |
| Error compatibility | Decided | Rejected controls return rejection and do not terminate an attempt. Failure of an accepted seek/decode operation uses the existing decode terminal category; presentation failure retains the existing render category. | No new listener method or error enum is required by the approved behavior. |
| Reuse | Decided | Extend the existing `VideoPreview` → JNI bridge → native playback owner → `PreviewRenderer` path. Do not add a parallel player, repository/domain layer, new module, or new rendering path. | Matches the technical-library architecture and keeps frame ownership/lifecycle centralized. |
| Design guides | Applied | Native guidance preserves exact JNI pairing, worker/reference lifetime, FFmpeg context ownership, surface teardown order, single-thread EGL execution, ARM ABI filters, and 16 KB packaging alignment. Camera-specific facts in the generic guides do not override the inspected `:videolib` implementation. | Constrains the design without importing unrelated `:camera` architecture. |

### Explicitly unspecified implementation-local choices

| Choice | Status | Constraint |
| --- | --- | --- |
| Concrete public method names and signatures | Intentionally unspecified | They must express the semantic accepted/rejected contracts below without changing existing symbols. |
| Native command queue/container and synchronization primitives | Intentionally unspecified | Commands must be totally ordered with decode, EOF, stop, surface loss, and release; FFmpeg contexts remain worker-confined. |
| Exact FFmpeg seek function/flags and decoder warm-up mechanics | Intentionally unspecified | The visible target, stale-request, timestamp, and error contracts below are normative. |
| Frame lateness threshold and selection algorithm at high speeds | Intentionally unspecified | Presentation order must remain monotonic and the latest seek result must win. |
| Quantitative “real time” seek latency | Intentionally unspecified | No source supplies a numeric threshold; accepted seek work must be awakened promptly and must not wait for the prior playback deadline. |

Blockers: none. No remote API, storage migration, permission, design-system, UI, or dependency decision is implicated.

## 2. Behavior and state transitions

### Behavior contract

| FR-ID | SC-ID | AC-ID | Story-ID | Design-Ref | Rule / trigger | Observable outcome | Failure / recovery |
| --- | --- | --- | --- | --- | --- | --- | --- |
| FR-01 | SC-01 | AC-01 | Story-01 | N/A | Pause is requested while the attempt is Playing or already Paused. | An accepted pause preserves the attempt and current media position; after pause returns, no playback-progress frame is presented until resume or seek. Repeating pause is an accepted no-op. | Reject when media is not yet ready, there is no active attempt, there is no usable surface, or the preview is released; keep the prior state. |
| FR-01 | SC-01 | AC-02 | Story-01 | N/A | Resume is requested while the attempt is Paused or already Playing. | Playback continues from the retained media position using the current speed. Repeating resume while already playing is an accepted no-op. | Reject when media is not yet ready, there is no active attempt, there is no usable surface, or the preview is released; keep the prior state. |
| FR-02 | SC-02 | AC-03 | Story-02 | EOF is reached while loop mode is enabled. | The same attempt restarts continuously from the beginning without emitting completion and retains its current speed and loop configuration. | A rewind/reinitialize failure terminates the attempt once with the existing decode error behavior. |
| FR-03 | SC-03 | AC-04 | Story-03 | A finite speed `≥0.1×` is set before or during playback. | The setting is accepted and retained. During playback, future presentation pacing uses the new rate without jumping media position; paused playback remains paused. | Reject values below `0.1×` or non-finite values without changing the previous speed or terminal state. |
| FR-04 | SC-04 | AC-05 | Story-04 | A seek in milliseconds is requested for a playing or paused active attempt. | The target is clamped to the playable interval; playback scheduling is interrupted promptly and the corresponding decodable frame becomes visible. | Reject without active media/surface or after release. An accepted seek that cannot reposition/decode terminates once with the existing decode error behavior. |
| FR-05 | SC-05 | AC-06 | Story-04 | AC-05 began while playing. | After the sought frame is presented, playback continues forward from its media timestamp at the current speed. | Stop, surface loss, or release takes precedence and retains its existing cancellation/error semantics. |
| FR-05 | SC-06 | AC-07 | Story-04 | AC-05 began while paused. | The sought frame is presented and remains visible; the attempt returns to paused at the sought media position. | A newer accepted seek supersedes older pending seek work; failure follows AC-05. |
| FR-06 | SC-07 | AC-08 | Story-02 | EOF is reached after loop mode has been disabled. | The current pass is final and emits exactly one completion on the main thread. Intermediate loop EOF boundaries emit no terminal event. | Existing terminal claiming suppresses duplicate completion/error and stale callbacks. |

### Cross-cutting derived behavior

| AC-ID | Source mapping | Contract |
| --- | --- | --- |
| AC-09 | SC-01, SC-04, SC-05, SC-06 | Control ordering is linear per preview instance. Stop, detach, and release invalidate or terminate pending controls according to existing lifecycle semantics, and no control may resurrect a cancelled attempt. |
| AC-10 | SC-04, SC-06 | For rapid seeks, the latest accepted target determines the settled frame and media position. Earlier in-flight presentation may finish, but an older result cannot overwrite the latest settled result. |
| AC-11 | SC-03 | At speeds beyond device decode/render capacity, ordered late frames may be skipped. Acceptance of a valid speed does not promise presentation of every source frame or a numeric wall-clock accuracy bound. |
| AC-12 | SC-02, SC-07 | Looping reuses one attempt identity and listener across passes. Only final completion, an unrecoverable error, explicit stop, surface loss, or release ends that attempt. |

### State model

Surface readiness is an orthogonal precondition: `Ready` permits playback/presentation; `Absent` rejects start/pause/resume/seek and causes an active attempt to follow existing render-failure teardown. Loop mode and speed are per-instance configuration, not lifecycle states.

| State | Meaning / invariants | Permitted events | Prohibited / ignored events |
| --- | --- | --- | --- |
| Idle | No active attempt; configuration is retained; no terminal delivery is pending. | Change valid loop/speed configuration; start when surface is ready; release. | Pause/resume/seek are rejected; stop is an idempotent no-op. |
| Starting | One accepted attempt is opening media; no second attempt may start. | Stop, detach, release; change loop/speed configuration. | Pause/resume/seek are rejected until media is ready; another start is rejected. |
| Playing | One attempt owns media resources and advances media time; at most one frame presentation is active. | Pause, seek, speed/loop change, stop, detach, release, EOF, decode/render failure. | Another start is rejected. |
| Paused | One attempt and media position are retained; no playback-progress frames are emitted. | Resume, seek, speed/loop change, stop, detach, release. | Another start is rejected; repeated pause is an idempotent no-op. |
| Seeking | One active attempt is repositioning/decoding toward the newest accepted target; the prior playing/paused disposition is retained. | Newer seek, speed/loop change, stop, detach, release, seek success/failure. | Another start is rejected; stale seek completion cannot own the settled state. |
| Stopping | Cancellation owns the attempt and precedes resource/surface teardown. | Join/quiesce and transition to Idle or Failed according to the initiating lifecycle event. | Playback controls and new start are rejected until quiescent. |
| Completed | Final non-looping EOF has claimed and delivered completion; configuration remains retained. | Start a later attempt, change loop/speed configuration, stop, detach, release. | Pause/resume/seek are rejected. |
| Failed | An existing input/decode/render terminal error has claimed the attempt; configuration remains retained. | Recover through the current permitted surface reattach/later-start path; change loop/speed; stop/release. | Pause/resume/seek are rejected. |
| Released | Native ownership and callback reference are gone; instance is permanently inert. | Repeated release/stop/detach remain safe no-ops. | Start and all controls are rejected. |

### Transition contract

| From | Event / precondition | To | Side effect | Failure / cancellation / recovery |
| --- | --- | --- | --- | --- |
| Idle, Completed, Failed | Start with valid local path and ready surface | Starting | Create one attempt identity and begin asynchronous media open. | Rejection leaves state unchanged; accepted open/decode failure goes to Failed once. |
| Starting | Media and decoder ready | Playing | Establish media position and playback-clock anchor using retained speed. | Stop/release/surface loss wins through existing terminal-claim ordering. |
| Playing | Accepted pause | Paused | Freeze media position, invalidate the running wall-clock deadline, and quiesce future progress presentation. | No terminal event. |
| Paused | Accepted resume | Playing | Re-anchor wall clock from retained media position and current speed. | No terminal event. |
| Playing or Paused | Accepted seek | Seeking | Record newer request identity, interrupt pacing, and reposition worker-confined media state. | Newer seek supersedes older pending work; lifecycle cancellation wins. |
| Seeking | Latest target frame presented; origin was Playing | Playing | Set media position from presented timestamp and re-anchor pacing. | Presentation failure goes to Failed with render error. |
| Seeking | Latest target frame presented; origin was Paused | Paused | Retain presented target as current position with no advancing deadline. | Presentation failure goes to Failed with render error. |
| Playing | Valid speed change | Playing | Preserve current media position and re-anchor future pacing at the new rate. | Invalid input is rejected with no state/config change. |
| Paused | Valid speed change | Paused | Update retained rate only. | Invalid input is rejected with no state/config change. |
| Playing | EOF and loop enabled | Playing | Reset media decode/timeline to the beginning within the same attempt; emit no terminal event. | Reset failure goes to Failed once with decode error. |
| Playing | EOF and loop disabled | Completed | Claim and deliver final completion once. | Racing terminal events are suppressed by attempt identity/terminal claim. |
| Starting, Playing, Paused, Seeking | Stop | Stopping → Idle | Invalidate listener visibility, wake blocked playback work, quiesce worker, retain ready surface and configuration. | No terminal callback for the cancelled attempt. |
| Starting, Playing, Paused, Seeking | Surface detach/loss | Stopping → Failed | Quiesce playback before EGL/window release and claim existing render failure once. | Reattach restores surface readiness; a later start creates a new attempt. |
| Any non-Released | Release | Released | Cancel/join media work, release EGL/window and JNI callback ownership in order. | No stale terminal or frame may appear after return. |

## 3. Components and responsibilities

### Module Contract Matrix

| Module | Owner/consumer | Responsibility | Depends on | Crossed contract | Compatibility obligation | Verification obligation |
| --- | --- | --- | --- | --- | --- | --- |
| `:videolib` | Owner; changed | Expose and execute local video-preview controls through its existing Kotlin/JNI/native/EGL pipeline. | Android `Surface`, bundled FFmpeg 7.1 static archives, EGL/GLES, Android/NDK runtime | Public Kotlin API, JNI symbols/types, native lifecycle/threading, FFmpeg state/timestamps, RGBA render handoff, ABI packaging | Existing public methods/defaults/callbacks remain valid; new commands are additive; native library name, ABIs, and 16 KB alignment remain unchanged. | `:videolib:assembleDebug`; native symbol/package inspection as needed; supported-device control behavior on configured ARM ABIs. |
| `:app` | Direct consumer; unchanged | Continue hosting a surface and using existing automatic start/stop/lifecycle behavior. | `:videolib` project dependency | Public Kotlin source/runtime and packaged native library | Must compile and retain current playback behavior without adopting new controls. | `:app:assembleDebug`; existing host lifecycle smoke behavior on a supported device. |
| `:benchmark` | Transitive target consumer; unchanged | Continue targeting the app benchmark variant. | `:app` | Target APK/package | No benchmark source or behavior change; app package remains buildable. | Covered by app package assembly for this feature; device macrobenchmark default is omitted because its startup-only scenario does not exercise playback controls. |
| External hosts | Unknown consumers; unchanged | Consume the public library contract outside repository visibility. | Distributed `:videolib` artifact when applicable | Source, binary, runtime, native/ABI | Existing consumers require no source change when controls are unused. | Compatibility is represented by additive API/default behavior plus AAR/APK and supported-ABI verification; consumers cannot be executed here. |

### Component decomposition

| Component role | Observed / proposed | Responsibility / owned state | Delegates to | Dependency direction | Must not own / know | Evidence / decision |
| --- | --- | --- | --- | --- | --- | --- |
| Public preview facade (`VideoPreview` seam) | Existing, extended | Own native handle, surface guard, one caller-thread access contract, listener registration, main-thread terminal delivery, and semantic validation/result exposure for controls. | JNI playback boundary | Host → facade → native owner | FFmpeg contexts, playback clock mechanics, EGL resources, or a second authoritative playback state | `VideoPreview.kt:25-211`; decision: preserve established facade. |
| JNI playback boundary | Existing, extended | Translate control values/results exactly, preserve opaque owner identity, manage callback global reference and worker-thread attach/detach. | Native playback coordinator | Facade → JNI → coordinator; terminal callback returns through bridge | Media policy, UI state, cached `JNIEnv*`, or duplicated playback state | `videolib.cpp:20-120,146-255`; native-boundary decision. |
| Native playback coordinator | Existing, extended | Be the single source of truth for attempt lifecycle, surface readiness, loop/speed configuration, pause disposition, newest seek identity, media position, cancellation, and terminal claim. Serialize controls with worker progress and lifecycle teardown. | Worker-confined media engine and existing renderer | JNI commands → coordinator → media/render participants | Host UI, Android lifecycle policy beyond explicit surface/release commands, or parallel player ownership | `video_playback.h:18-90`; architecture decision. |
| Worker-confined media engine | Existing responsibility, extended | Own FFmpeg format/codec/packet/frame/conversion resources for one attempt; perform ordered read/decode, reposition/reset, timestamp normalization, loop reset, and frame eligibility. | Playback clock and renderer handoff | Coordinator → media engine → clock/render | Cross-thread FFmpeg context access, persistent storage, audio playback, or host callbacks | `video_playback.cpp:300-494`; FFmpeg ownership decision. |
| Playback clock/control arbiter | Proposed semantic responsibility within the native owner | Relate monotonic wall time to media position and speed; account for pause duration, speed re-anchor, seek reset, loop reset, command wakeup, and late-frame eligibility. | Media engine scheduling | Coordinator/media timestamps → clock → presentation eligibility | EGL calls, FFmpeg resource ownership, or terminal callbacks | Current fixed clock at `video_playback.cpp:351-400`; required by FR-01–FR-05. |
| Existing preview renderer | Existing, reused unchanged in responsibility | Synchronously accept caller-owned RGBA data for the duration of the call, serialize it onto the dedicated render thread, present through current EGL context, and own ordered EGL/window teardown. | Existing render-thread executor and GL program | Media engine → renderer → EGL/GLES surface | Playback state, speed, looping, seeking, FFmpeg contexts, or host listener state | `preview_renderer.h:25-71`; `preview_renderer.cpp:77-190`. |
| Host application | Existing consumer, unchanged | Own surface and activity lifecycle, local-file preparation, and any future UI that invokes the public contract outside this ticket. | Public preview facade | Host → `:videolib` | Native state, FFmpeg/EGL mechanics, or reusable control behavior | `MainActivity2.kt:25-238`; clarified UI exclusion. |

Reuse decision: extend the existing playback coordinator and renderer seam. The renderer is already a reusable implementation for both progressive and sought RGBA frames because it copies/presents synchronously on its EGL thread; it requires no playback-control knowledge. The camera module's different global camera pipeline is not a reuse candidate.

DI/composition: no DI is introduced. `:videolib` currently constructs its per-preview native owner and renderer directly, which remains the proven composition boundary.

## 4. End-to-end data flow

### Flow A — pause and resume

| Step | Participant | Input/source | Decision/transformation | Output / side effect | Error propagation |
| --- | --- | --- | --- | --- | --- |
| 1 | Host → public facade | Pause or resume command | Check live handle/surface and forward semantic command. | Accepted/rejected result. | Rejection leaves state unchanged and emits no terminal event. |
| 2 | JNI boundary → coordinator | Opaque owner plus command | Preserve exact types and order command against lifecycle/control state. | AC-01 or AC-02 state transition. | Released/missing-active state rejects. |
| 3 | Playback clock | Retained media position and current speed | Pause invalidates the active deadline; resume creates a new monotonic anchor without changing media position. | Worker stops waiting/presenting or resumes scheduling. | Stop/detach/release cancellation takes precedence under AC-09. |
| 4 | Media engine → renderer | Next eligible decoded RGBA frame after resume | Continue forward from retained position. | Visible playback resumes on the same surface. | Render failure terminates once through existing error delivery. |

### Flow B — configure speed

| Step | Participant | Input/source | Decision/transformation | Output / side effect | Error propagation |
| --- | --- | --- | --- | --- | --- |
| 1 | Host → facade | Floating-point playback rate | Accept only finite values `≥0.1×`. | Accepted/rejected result. | Invalid input retains prior rate and playback state. |
| 2 | Coordinator | Valid rate, current lifecycle state | Retain per-instance setting; if playing, preserve media position and re-anchor clock; if paused/idle, update setting without progression. | New rate applies to future scheduling/current or next attempt. | Released preview rejects. |
| 3 | Clock/media engine | Media timestamp deltas and rate | Scale wall-clock pacing; identify frames already too late at high rates without reordering media time. | Ordered frames are presented; late intermediates may be omitted under AC-11. | Decode/render failures retain existing categories. |

### Flow C — seek and scrub

| Step | Participant | Input/source | Decision/transformation | Output / side effect | Error propagation |
| --- | --- | --- | --- | --- | --- |
| 1 | Host → facade | Target expressed in milliseconds | Validate active attempt/surface and forward the representable target; negative and beyond-duration values remain valid inputs for boundary clamping. | Accepted/rejected result. | No active attempt/surface or released state rejects. |
| 2 | Coordinator | Target and current Playing/Paused disposition | Clamp against known playable duration, assign newest seek identity, retain post-seek disposition, wake worker immediately. | State becomes Seeking under AC-05. | Lifecycle cancellation invalidates pending seek. |
| 3 | Media engine | Latest target | Reposition worker-owned demux/decode state, discard decoder history and pre-target decoded output, normalize timestamps from the sought media position. | First qualifying frame, or final available frame at end boundary. | Reposition/decode failure terminates once as decode error. |
| 4 | Renderer | Caller-owned RGBA frame valid for the synchronous handoff | Serialize presentation on existing EGL thread. | Requested-position frame becomes visible. | EGL/presentation failure terminates once as render error and follows existing surface recovery. |
| 5 | Coordinator/clock | Presented timestamp and retained disposition | Ignore stale settled results by request identity; re-anchor if originally playing or freeze if paused. | AC-06 or AC-07; latest seek owns final media position. | A newer seek repeats from step 2; no stale final overwrite. |

### Flow D — EOF and loop changes

| Step | Participant | Input/source | Decision/transformation | Output / side effect | Error propagation |
| --- | --- | --- | --- | --- | --- |
| 1 | Host → coordinator | Loop enabled/disabled, before or during attempt | Store latest per-instance setting in the ordered control state. | Setting applies to the current/next EOF decision and persists for later attempts. | Released preview rejects; repeated same value is accepted no-op. |
| 2 | Media engine → coordinator | Decoder drained at EOF | Read latest ordered loop setting after the last frame is presented. | Branch to AC-03 or AC-08. | Competing stop/detach/release/terminal claim wins. |
| 3a | Media engine/clock | Loop enabled | Reset worker-owned media/decode state and playback clock to media start, retain attempt/listener/speed. | Next pass begins; no completion callback. | Reset failure reports decode error once. |
| 3b | Coordinator → JNI bridge → facade | Loop disabled | Claim final completion for the attempt. | Existing main-thread `onPlaybackCompleted()` delivery occurs exactly once. | Stale or duplicate terminal delivery is suppressed by attempt identity. |

### Source of truth, ordering, and side effects

- The local input file remains caller/host-owned; this feature adds no persistence, cache, remote source, or audio side effect.
- The native playback coordinator is the control/state source of truth. FFmpeg resources never cross out of the playback worker, and RGBA memory remains caller-owned until the renderer's synchronous handoff returns.
- Control ordering is per `VideoPreview` instance. The latest accepted command wins only where explicitly defined (seek target and mutable speed/loop configuration); stop, surface loss, and release outrank pending media work.
- Observable side effects are surface presentation, accepted/rejected command results, and the existing single terminal callback. No new progress/state callback is required.

## 5. Boundary contracts

| Contract / boundary | Observed / proposed / blocked | Semantic input | Output / result | Invariants | Errors | Compatibility / versioning | Owner |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Host → public preview controls | Proposed additive | Pause, resume, loop value, finite rate, target milliseconds | Synchronous accepted/rejected; accepted work may complete asynchronously | Single caller/owner thread remains required; defaults are loop off and `1.0×`; speed/loop persist per instance | Invalid value/state rejects without terminal callback | Existing public symbols and behavior remain; no consumer migration required | `:videolib` facade |
| Existing start/terminal contract | Observed, preserved | Local file path, ready surface, listener | Positive accepted attempt semantics; exactly one final completion/error unless stopped/released | One active attempt; callbacks on main thread; no completion at intermediate loop EOF | Existing input/unsupported/decode/render categories | No listener or enum break; final loop completion uses existing callback | Facade + coordinator + JNI bridge |
| Facade ↔ JNI controls | Proposed additive | Native handle plus scalar control values | Exact native acceptance result | Kotlin declaration, generated JNI signature, and hand-mangled export stay paired; handle `0` never crosses as valid owner | Invalid handle rejects; JNI exceptions do not become hidden state changes | Additive native exports; existing exports/library name unchanged | `:videolib` JNI boundary |
| JNI terminal callback | Observed, preserved | Attempt identity plus optional existing error category | Kotlin private callback then main-thread listener delivery | Worker obtains thread-local `JNIEnv`, retained target is a global reference, attach/detach and release remain paired | Callback exception is contained as today; stale identity is ignored | Callback names/signatures unchanged | JNI bridge + facade |
| Coordinator control ordering | Proposed | Ordered lifecycle/control events and worker progress | One authoritative state transition/result | No command resurrects an ended attempt; terminal claim is single-winner; latest seek identity owns settled position | Rejection is non-terminal; accepted media failure is terminal once | Internal behavior change only | Native playback coordinator |
| Playback clock | Proposed internal | Media timestamps, monotonic time, pause/resume, seek, speed, loop reset | Eligibility/deadline for next frame | Media position is continuous across speed change; paused wall time does not advance media; seek/loop reset anchors | No independent public error | Internal; default `1.0×` reproduces current pacing | Native playback coordinator |
| FFmpeg media ownership | Observed, extended | Worker-owned local input and control decisions | Decoded frame/timestamp or EOF/error | Format/codec/packet/frame/conversion contexts are confined to one attempt worker; reposition/reset occurs there; no vendored artifact modification | Existing input/unsupported/decode mapping | Bundled FFmpeg 7.1 archives, link order, and configured ABIs remain unchanged | Worker-confined media engine |
| Media → renderer frame handoff | Observed, reused | RGBA pixels, dimensions, latest-request eligibility | Synchronous presentation success/failure | Pixel buffer remains valid through call; playback/render serialization prevents teardown races; all EGL/GL work stays on renderer thread with context current | Presentation failure maps to existing render error | Renderer public/native responsibility unchanged | Existing preview renderer |
| Surface/lifecycle | Observed, preserved | Attach, detach/loss, stop, release | Ready/absent surface and ordered quiescence | Decode/control work stops before EGL/window/native callback ownership is destroyed | Surface loss during active attempt reports render error; stop/release suppress stale terminal/frame | Existing host integration remains valid | Host + facade + coordinator + renderer |
| Native build/package | Observed, preserved | `:videolib` native target and bundled static archives | `libvideolib.so` in consuming artifact for `arm64-v8a` and `armeabi-v7a` | C++17 setting, NDK/CMake pins, 16 KB page alignment, FFmpeg/EGL/GLES linkage remain aligned | Link/load/package failures are verification failures | No ABI removal, library rename, dependency upgrade, or publication change | `:videolib` build boundary |

## 6. Conditional cross-cutting design

### Async and concurrency contract

- Host-facing calls retain the documented single-owner-thread constraint. Native synchronization still defends the decode/render workers and lifecycle boundary; it does not make the public facade generally thread-safe.
- FFmpeg demux, decoder mutation, seek/reset, packet/frame ownership, and timestamp normalization are serialized on the playback worker. JNI caller threads only publish ordered commands and receive acceptance.
- A control command wakes any timestamp wait immediately. Pause waits for any already-entered synchronous renderer handoff to finish before its accepted transition is observable; therefore no later progress frame appears after pause returns.
- The EGL context and GL objects remain created, used, and destroyed only through the existing render-thread executor. Seek does not create another GL path or retain a decoded RGBA buffer past synchronous presentation.
- Stop, detach, and release keep their current quiescence ordering. Release destroys callback ownership only after playback work can no longer call it.

### Performance contract

- Normal playback and speed changes use media timestamps as the media-time source and a monotonic clock as the wall-time source. Re-anchoring prevents accumulated paused time or the old speed from shifting later deadlines.
- Accepted seek requests bypass the prior frame deadline and begin latest-target work promptly. “Real time” remains qualitative because the source defines no latency number.
- At an unbounded high speed, ordered late-frame omission is permitted to avoid an ever-growing presentation backlog. The final EOF/loop semantics and latest seek result remain mandatory even when intermediates are omitted.
- Loop restart reuses the active attempt and media worker; it must not accumulate additional workers, JNI references, render queues, or terminal callbacks across passes.

### Failure, recovery, and rollback

| Risk | Blast radius | Severity | Mitigation / contract |
| --- | --- | --- | --- |
| Control/lifecycle race violates quiescence or terminal uniqueness | One preview instance; host callbacks/surface/native lifetime | High | AC-09 single ordering, lifecycle precedence, attempt identity, terminal single-claim, worker quiescence before teardown. |
| Seek/loop leaves stale decoder or clock state | Visible frame order, media position, completion behavior | High | Worker-confined reset, decoder-history discard, timestamp re-anchor, newest seek identity, explicit EOF decision. |
| High speed creates backlog or memory growth | Playback responsiveness and renderer queue | High | No unbounded presentation queue; permit ordered late-frame omission under AC-11. |
| JNI declarations/exports diverge | All control calls at runtime | High | Exact bidirectional signature/name review plus native build and device linkage verification. |
| Seek frame races surface teardown | EGL/window and RGBA lifetime | High | Existing renderer serialization and stop-before-release; lifecycle cancellation outranks pending seek. |
| Behavior regression for current consumers | `:app` and unknown external hosts | High | Additive controls, unchanged defaults, unchanged existing methods/callbacks/error categories, direct consumer build. |
| ABI/package regression | ARM consuming APK/AAR | High | Preserve FFmpeg/CMake linkage, both ABI filters, library name, and 16 KB alignment; verify build/package and supported-ABI load. |

There is no data migration. Rollback is behavioral/code rollback to the existing facade/native playback path because new controls are dormant under the preserved defaults; no stored state or external schema requires downgrade handling. Publishing or distributing any rollback or new artifact remains separately authorized.

### Architectural verification obligations

| Obligation | Evidence level | What it establishes |
| --- | --- | --- |
| `./gradlew :videolib:assembleDebug` | Native/Gradle compile and link | Kotlin/JNI/C++ consistency, FFmpeg/EGL link closure, configured ABI native build. |
| `./gradlew :app:assembleDebug` | Direct consumer compile/package | Existing host source compatibility and packaging of the changed native library. |
| Native symbol and APK/AAR inspection for both configured ABIs | Artifact inspection | Expected JNI exports, `libvideolib.so`, ABI presence, and alignment/linkage contract. |
| `./gradlew :videolib:connectedDebugAndroidTest` on supported ARM EGL/GLES 3.0 devices | Runtime integration | Real FFmpeg decoding, pause/resume, rate changes, loop EOF, playing/paused seek visibility, callbacks, surface lifecycle, and native worker/reference behavior. |
| Supported-ABI native load/control smoke coverage for `arm64-v8a` and `armeabi-v7a` | Device/ABI runtime | Native load and control behavior on every packaged ABI; compilation alone is insufficient. |

The benchmark device task is excluded from the minimal closure because its inspected scenario measures app startup and does not exercise playback controls; `:app` assembly covers the transitive package edge. No publication, signing, upload, distribution, commit, or push is part of verification.

## 7. Coverage audit

### Requirement and acceptance coverage

| FR-ID | SC-ID | AC-ID | Story-ID | Design decision / boundary | Status |
| --- | --- | --- | --- | --- | --- |
| FR-01 | SC-01 | AC-01, AC-02, AC-09 | Story-01 | Paused state, position-preserving clock re-anchor, idempotent duplicate commands, lifecycle precedence | Covered |
| FR-02 | SC-02 | AC-03, AC-12 | Story-02 | Same-attempt EOF restart with retained listener/config and no intermediate terminal event | Covered |
| FR-03 | SC-03 | AC-04, AC-11 | Story-03 | Finite `≥0.1×` validation, retained setting, media-position-preserving re-anchor, high-speed late-frame policy | Covered |
| FR-04 | SC-04 | AC-05, AC-10 | Story-04 | Millisecond clamping, worker-confined reposition/decode, prompt pacing interruption, latest-target settling | Covered |
| FR-05 | SC-05 | AC-06, AC-09 | Story-04 | Playing seek resumes from presented timestamp under lifecycle precedence | Covered |
| FR-05 | SC-06 | AC-07, AC-10 | Story-04 | Paused seek presents target and returns paused; newest request owns settled frame | Covered |
| FR-06 | SC-07 | AC-08, AC-12 | Story-02 | Latest loop setting decides EOF; final pass completes exactly once | Covered |

### DEV-SPEC open-item resolution

| Item | Design resolution | Contract |
| --- | --- | --- |
| Invalid speeds | Reject non-finite or `<0.1×`; preserve previous valid setting/state. | AC-04 |
| Rapid seeks | Newest accepted target supersedes pending older work and owns settled position/frame. | AC-10 |
| Seek before zero or beyond duration | Clamp to playable interval and present the qualifying boundary frame. | AC-05 |
| Controls without active playback/surface or after completion/release | Pause/resume/seek reject; speed/loop may configure any non-released instance and persist; released rejects all new controls. | AC-01, AC-02, AC-04, AC-05 |
| Surface loss/stop/release racing controls | Lifecycle cancellation takes precedence; no resurrection, stale callback, or post-return frame. | AC-09 |
| Loop disable racing EOF | EOF reads latest ordered setting; disable after restart makes the new pass final. | AC-08, AC-12 |
| Extremely high speed | Accept every finite value `≥0.1×`; allow ordered late-frame omission without a numeric SLA. | AC-11 |
| Undefined numeric “real time” | Keep qualitative prompt-wakeup/latest-result contract; do not invent latency threshold. | AC-05, AC-10 |

All FRs, SCs, stories, and DEV-SPEC open items map to a design decision. Unresolved inputs required to complete the design: none.
