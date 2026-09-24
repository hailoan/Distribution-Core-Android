AUTOMATION: CONTINUE

# SOLUTION-DESIGN — Preview adjustment and caller-defined filters

## 1. Decision ledger

### Sources and investigation

| ID | Status | Decision/evidence | Impact |
| --- | --- | --- | --- |
| D-01 | observed | `DEV-SPEC.md` defines FR-01–FR-06 and SC-01–SC-05; no Figma or other UI design source applies. | This design covers preview rendering behavior only. |
| D-02 | observed | `VideoPreview` is the public, instance-owned preview facade; its native handle owns `VideoPlayback`, which owns `PreviewRenderer` and its `GlProgram` (`VideoPreview.kt:25`, `videolib.cpp:147`, `video_playback.h:87`). | `videolib` is the sole production owner; the capability extends this existing seam. |
| D-03 | observed | Decoded and caller-provided RGBA frames converge on `PreviewRenderer::pushFrame`; all EGL/GLES work is synchronous on its dedicated render executor and playback presentation is guarded by `rendererMutex_` (`video_playback.cpp:113`, `:661`; `preview_renderer.cpp:110`). | Appearance processing belongs at the existing RGBA GLES draw boundary, after decode conversion and before presentation. |
| D-04 | observed | The current `GlProgram` is a fixed one-texture pass-through program whose GL objects are created and destroyed while the EGL context is current (`gl_program.cpp:30`; `preview_renderer.cpp:155`). | Program, uniforms, and auxiliary textures remain renderer-owned GL resources. No FFmpeg/decode contract changes. |
| D-05 | proposed | Add an immutable, public appearance configuration owned by each preview instance: the complete adjustment snapshot plus zero or one caller filter descriptor. Updates replace a complete accepted snapshot, rather than exposing independently owned mutable native values. | Gives Kotlin, JNI, lifecycle, and render code one source of truth and makes restoration/atomicity explicit. |
| D-06 | proposed | Adjustment math and defaults reproduce the referenced `VideoConfigure` and adjustment shader behavior, adapted from YUV input to the current RGBA input. Processing order is caller filter, then brightness, contrast, saturation, exposure, darks, levels, vignette, vibrance, temperature, hue, highlights, shadows, lights, and clarity. | Preserves the reference order in `AdjustType.kt`; vignette and clarity are activated at their declared positions because FR-01 requires them even though the reference enum currently emits no statement for either. |
| D-07 | proposed | Caller filters use version 1 of a library-owned GLES 3.0 fragment-snippet contract. The caller supplies filter-function source, opacity, and ordered immutable RGBA8888 auxiliary textures. The library owns the vertex shader, primary-frame sampling, adjustment functions, final output, and shader assembly. | A consumer can create LUT/overlay/curve filters without replacing protected renderer plumbing or depending on the sibling library. |
| D-08 | proposed | A filter candidate is validated, compiled, linked, and has all auxiliary textures uploaded before the active program/resources are replaced. Any failure rejects the candidate and retains the last good appearance. | A bad caller shader cannot destroy an active preview. |
| D-09 | proposed | Accepted appearance state survives surface detach/recreation for the lifetime of the `VideoPreview` instance. Adjustments and filter removal may be accepted without a surface. Installing a new non-empty filter requires an attached surface so shader/device validation can return synchronously. | Live updates have deterministic results; a failed retained-filter rebuild can be recovered by clearing the filter and attaching again. |
| D-10 | proposed | Public additions are additive. Existing playback, surface, frame-input, callback, library-loading, ABI, and build behavior remain compatible. | Existing `app` and unknown external consumers continue to compile and retain pass-through output unless they opt in. |
| D-11 | intentionally unspecified | Concrete Kotlin/C++ type names, JNI parameter packing, source-file placement, string/resource packaging, and internal uniform-location/cache mechanics. | These are implementation-local choices constrained by the semantic contracts below. |

Investigation depth: **Deep**. Fourteen focused lookup groups were used against the 20-lookup cap: the stage packet; DEV-SPEC; current public facade/GL program; renderer/playback lifecycle; JNI boundary/callers; Gradle/CMake/host consumer; reference configuration/interface; shader builder and GLSL sources; reference adjustment ordering/JNI surface; reference native setters/filter update; reference GL state/uniform binding; auxiliary texture upload; undocumented-range search; and ticket/status preflight. The project graph was unavailable as recorded by DEV-SPEC, so source and focused symbol search were used. The last searches confirmed already-known reference gaps and added no competing owner.

### Adjustment compatibility values

| Adjustment | Neutral default | Accepted range/constraint | Status/evidence |
| --- | --- | --- | --- |
| Brightness | `0` | `-0.5..0.5` | observed reference contract, `VideoConfigure.kt:14` |
| Exposure | `0` | `-1..1` | observed reference contract |
| Contrast | `1` | `0..2` | observed reference contract |
| Saturation | `1` | `0..2` | observed reference contract |
| Lights | `1` | `0..2` | observed reference contract |
| Highlights | `-2` | `-2..2` | default observed; bounded range proposed because the reference omits one |
| Darks | `1` | `0.5..1.5` | observed reference contract |
| Clarity | `0` | `-1..1` | default observed; bounded range proposed because the reference omits one |
| Shadows | `0` | `-1..1` | observed reference contract |
| Temperature | `0` | `-0.5..0.5` | observed reference contract |
| Hue | `0` | `-1..1` | observed reference contract |
| Vignette | `0` | `0..1` | default observed; bounded range proposed because the reference omits one |
| Levels | `(0, 1, 1)` | minimum input `-1..1`, gamma `0.5..1.5`, maximum input `0.5..1.5`, with minimum input strictly below maximum input | observed values plus proposed validity invariant preventing a zero/inverted level interval |
| Vibrance | `0` | `-1..1` | observed reference contract |

All values must be finite. A rejected value is not clamped and does not alter the last accepted snapshot. Neutral vignette and clarity are explicit bypasses, ensuring pass-through behavior despite undefined/omitted behavior in the reference chain.

## 2. Behavior and state transitions

### Behavior contract

| FR-ID | SC-ID | AC-ID | Story-ID | Design-Ref | Rule/trigger | Observable outcome | Failure/recovery |
| --- | --- | --- | --- | --- | --- | --- | --- |
| FR-01, FR-05 | SC-01 | AC-01 | Story-01 | D-03, D-05, D-06 | The owner submits a valid complete or single-control adjustment update during active playback. | The accepted value is used for every subsequently presented frame without restarting, seeking, or replacing playback. | Invalid input returns a validation rejection; the previous accepted appearance remains active. |
| FR-02 | SC-02 | AC-02 | Story-01 | D-06; adjustment table | A preview is created or its adjustments are reset. | All fourteen controls use the listed neutral defaults and reference numeric transforms; with no filter the output remains visually pass-through. | No recovery is needed; neutral clarity/vignette bypass their transforms. |
| FR-03, FR-05 | SC-03 | AC-03 | Story-02 | D-07, D-08 | The owner installs a valid version-1 caller filter while a surface is attached. | Compilation/resource preparation completes atomically; the filter affects the next frame admitted after the update returns. | Shader/link/resource failure is reported as a filter rejection with a bounded diagnostic; the prior filter and adjustments continue. |
| FR-04 | SC-04 | AC-04 | Story-02 | D-07 | A filter declares zero or more ordered RGBA8888 auxiliary textures and samples them through the versioned sampler bindings. | LUT, overlay, and curve-shaped data (including a `256x1` RGBA curve) can contribute to the preview pixel result. | Invalid count, dimensions, byte size, device limit, or allocation/upload rejects the whole candidate without partial bindings. |
| FR-05 | SC-01, SC-03 | AC-05 | Story-01, Story-02 | D-03, D-08 | Appearance update and frame presentation contend. | The renderer serializes them; a frame is rendered wholly with the old or new accepted snapshot, never a mixture. Calls made from the documented single owner thread are observed in call order. | Stop, detach, or release wins once admitted to lifecycle teardown; no queued appearance work may touch released GL/native state. |
| FR-01, FR-03 | SC-01, SC-03 | AC-06 | Story-01, Story-02 | D-09 | The surface detaches and a later surface attaches on the same preview instance. | CPU-side accepted appearance remains retained; GL program/textures are destroyed with the old context and reconstructed before frames use the new context. | Rebuild failure makes attachment fail cleanly with no frame presentation; clearing the filter permits recovery and a later attach. |
| FR-03, FR-04 | SC-03, SC-04 | AC-07 | Story-02 | D-07, D-09 | A new non-empty filter is submitted without an attached surface, or any appearance update is submitted after release. | The request is rejected synchronously and existing state is unchanged. Filter removal and valid adjustment changes remain retainable while merely detached. | Caller attaches a surface before retrying a filter, or creates a new preview after release. |
| FR-06 | SC-05 | AC-08 | Story-03 | D-02, D-10 | A host consumes the new capability. | All public types, shader assets, validation, JNI, and renderer behavior are supplied by `videolib`; no sibling-library dependency is needed. | Build/dependency verification rejects an accidental sibling edge. |

### State model

| State | Meaning/invariants | Permitted events | Prohibited/ignored events |
| --- | --- | --- | --- |
| Retained | Preview instance is alive without a usable surface. One valid CPU-owned appearance snapshot exists; no GL resource is assumed. | Valid adjustment update, filter removal/reset, surface attach, release. | Non-empty filter installation is rejected because device compilation/limits cannot be validated. |
| Applied | Surface/EGL context is usable and all frames use one accepted appearance snapshot with matching GL resources. | Frame presentation, valid adjustment update, atomic filter replacement/removal, detach, release, playback controls. | Invalid candidates are rejected without changing state; duplicate accepted values may be a semantic no-op. |
| Released | Native owner and GL resources are no longer usable. | Idempotent release only. | Frames, surface attach, adjustment, and filter operations are rejected or ignored consistently with existing inert-facade behavior. |

Filter-update rejection is an operation result, not a persistent renderer state; the prior Retained or Applied state remains authoritative.

### Transition contract

| From | Event/precondition | To | Side effect | Failure/cancellation/recovery |
| --- | --- | --- | --- | --- |
| Retained | Attach valid surface; retained filter rebuild succeeds | Applied | Create EGL-owned program/textures, apply retained uniforms, then admit frames. | Partial resources are destroyed with context current if initialization fails. |
| Retained | Attach valid surface; retained filter rebuild fails | Retained | No frame is admitted; surface attachment reports failure. | Clear filter, then retry attachment; adjustments remain retained. |
| Applied | Valid adjustment update | Applied | Serialize and publish the complete new adjustment snapshot before the next admitted draw. | Rejection leaves prior snapshot active. |
| Applied | Valid filter replacement | Applied | Build candidate program/textures off to the side, then swap the complete filter generation atomically. | Compile/link/upload/capability failure destroys only candidate resources. |
| Applied | Filter removal | Applied | Atomically restore the library pass-through filter while preserving adjustments. | Prior state remains if restoration cannot be prepared. |
| Applied | Surface detach | Retained | Block new presentations, complete/deny contending update, and destroy all GL resources on the render thread. | Existing playback surface-loss behavior remains authoritative. |
| Retained or Applied | Release | Released | Stop playback first, release EGL/program/textures, then destroy native ownership. | Idempotent; no callback or update survives teardown. |

## 3. Components and responsibilities

### Module Contract Matrix

| Module | Owner/consumer | Responsibility | Depends on | Crossed contract | Compatibility obligation | Verification obligation |
| --- | --- | --- | --- | --- | --- | --- |
| `videolib` | Primary owner; only changed production module | Own public appearance/filter semantics, validation, JNI translation, renderer serialization, shader assembly, and GL resources. | Android/JNI, EGL/GLES 3.0, existing internal playback/render path | Public Kotlin, Kotlin/JNI, native ownership/threading, shader ABI, GLES resources, AAR native ABI | Additive source/binary API; unchanged default pixels; exact JNI pairing; retained ARM ABI and 16 KB packaging | `:videolib:assembleDebug`; supported-device GLES behavior on both packaged ABI classes where hardware is available; inspect packaged AAR/native load. |
| `app` | Direct in-repository consumer; verification only | Continue hosting `VideoPreview` without production feature ownership. | `project(":videolib")` (`app/build.gradle.kts:55`) | Public source/runtime integration | Existing usage compiles and retains pass-through behavior without adopting new APIs. | `:app:assembleDebug`; existing preview smoke behavior. |
| External super-apps | Unknown public consumers | Supply trusted filter snippets/textures and adjustment intent through the public contract. | Published/embedded `videolib` | Public source/binary/behavior and versioned shader snippet contract | Existing clients remain compatible; new clients receive stable version-1 semantics and explicit rejection results. | Consumer compile plus supported-device preview integration; exact consumer inventory remains unknown. |

Dependency closure: `videolib → app` is a confirmed Gradle producer-to-consumer edge with compile/runtime impact. `videolib → external super-apps` is required by the ticket but cannot be enumerated; it remains an explicit public compatibility obligation. No other module receives production changes or a new dependency.

### Responsibility matrix

| Component role | Observed/proposed | Responsibility/owned state | Delegates to | Dependency direction | Must not own/know | Evidence/decision |
| --- | --- | --- | --- | --- | --- | --- |
| Public preview facade | observed, extended | Own instance lifecycle, single-owner-thread contract, accepted CPU appearance snapshot, input validation, and semantic operation results. | Native playback boundary | Consumer → public facade → native owner | EGL handles, GL object identities, FFmpeg frames, sibling-library types | `VideoPreview.kt:25`; D-05, D-10 |
| Appearance contract | proposed | Define adjustment defaults/ranges, versioned filter source ABI, opacity, ordered auxiliary texture payloads, and immutable ownership. | Public preview facade | Consumer data → facade validation | Surface, playback, JNI names, GL locations | D-06, D-07 |
| Native playback owner | observed, extended | Serialize appearance operations against presentation and lifecycle; retain native-safe snapshot for renderer reconstruction. | Preview renderer | JNI → playback owner → renderer | Shader compilation details or caller mutable buffers | `video_playback.h:87`; D-03, D-05 |
| Preview renderer | observed, extended | Own EGL-context affinity and transactional application/reconstruction of an appearance generation. | GLES appearance program | Playback owner → render executor → program | Public API representation, decode scheduling, host lifecycle | `preview_renderer.cpp:110`, `:155`; D-08, D-09 |
| GLES appearance program | observed, extended | Own primary RGBA texture, compiled library shell plus caller snippet, adjustment uniforms, auxiliary textures, draw-time binding, and candidate cleanup. | GLES driver | Renderer → program → GLES | Kotlin objects, ANativeWindow ownership, FFmpeg/YUV formats | `gl_program.h:19`; D-04, D-06, D-07 |
| Playback/decode worker | observed, unchanged | Produce RGBA frames and admit presentation under existing playback ordering. | Native playback owner/renderer | Decode → RGBA presentation | Adjustment policy, filter payload interpretation | `video_playback.cpp:650`, `:661` |

Reuse decisions: extend `VideoPreview`, `VideoPlayback`, `PreviewRenderer`, and `GlProgram`; use the sibling GLSL and configuration only as canonical behavioral evidence. Do not reuse its runtime module, its YUV shader shell, mutable global renderer, unsafe buffer/list JNI shape, or non-atomic program replacement.

## 4. End-to-end data flow

### Flow A — Adjustment update (`AC-01`, `AC-02`, `AC-05`)

| Step | Participant | Input/source | Decision/transformation | Output/side effect | Error propagation |
| --- | --- | --- | --- | --- | --- |
| 1 | Host | One adjustment value or complete snapshot | Express value in the reference numeric domain. | Appearance request. | None yet. |
| 2 | Public facade | Request plus current snapshot | Require alive instance, finite values, declared ranges, and level invariant; produce a new immutable snapshot. | Accepted CPU source of truth. | Synchronous validation rejection; previous snapshot retained. |
| 3 | Native playback owner | Accepted snapshot | Copy to native-owned value state and serialize against frame/lifecycle access. | One ordered appearance generation. | Released/lifecycle rejection reaches caller without queued work. |
| 4 | Render thread/program | Appearance generation | Bind corresponding uniforms in the fixed order in D-06. | Subsequent draws use the new values. | GL failure does not publish a partial generation and is reported as render/update failure. |
| 5 | Surface | Next RGBA frame | Filter/adjust once and present. | Visible updated preview; playback timing is unchanged. | Existing render failure propagation remains authoritative. |

### Flow B — Caller filter installation (`AC-03`, `AC-04`, `AC-05`)

| Step | Participant | Input/source | Decision/transformation | Output/side effect | Error propagation |
| --- | --- | --- | --- | --- | --- |
| 1 | Host | Version, filter-function GLSL, opacity, ordered texture descriptors | Provide trusted integration code and immutable payload content. | Candidate filter descriptor. | None yet. |
| 2 | Public facade | Candidate descriptor | Validate contract version, attached/alive state, source presence, opacity, positive dimensions, overflow-safe exact `width × height × 4` byte counts, and immutable ownership copy. | Validated candidate snapshot. | Typed rejection category with no native mutation. |
| 3 | Native playback owner | Validated candidate | Serialize with `rendererMutex_` so no presentation or teardown overlaps the transaction. | Ordered candidate handed to render thread. | Lifecycle loss rejects/cancels before GL mutation. |
| 4 | Render thread/program | Source and RGBA payloads | Check device texture-unit/size limits; assemble library shell; compile/link candidate; create/upload linear, clamp-to-edge auxiliary textures; resolve required bindings. | Complete candidate GL generation. | Bounded compile/link/capability/resource diagnostic; candidate resources destroyed. |
| 5 | Render thread/program | Complete candidate | Atomically swap it with the active generation, then release old resources while context is current. | Next admitted frame uses the new filter followed by current adjustments. | Swap is not attempted unless every candidate part succeeded. |
| 6 | Public facade | Native outcome | Publish applied or rejected result synchronously to the owner call. | Caller can correlate result with its request. | Previous accepted configuration remains the source of truth on rejection. |

### Flow C — Detach, reconstruction, and recovery (`AC-06`, `AC-07`)

| Step | Participant | Input/source | Decision/transformation | Output/side effect | Error propagation |
| --- | --- | --- | --- | --- | --- |
| 1 | Host/facade | Surface detach | Mark surface unavailable before native teardown; preserve CPU appearance snapshot. | No new frame/filter installation admitted. | Existing active playback receives existing surface-loss behavior. |
| 2 | Render thread/program | Current GL generation | Destroy auxiliary textures, program, primary texture, and geometry before EGL context destruction. | Retained state with no GL ownership. | Teardown is idempotent. |
| 3 | Host/facade | New surface attach | Recreate EGL, pass-through shell, retained filter, textures, and adjustment bindings before marking surface ready. | Applied state; later frames match the retained appearance. | Any retained-filter rebuild failure rejects attachment and cleans partial resources. |
| 4 | Host | Recovery after rebuild failure | Clear the retained filter while detached and attach again. | Neutral filter plus retained adjustments can be restored. | If base program/EGL still fails, existing attachment failure remains. |

Source of truth is the last accepted immutable appearance snapshot for the preview instance. GL state is a disposable projection of that snapshot; no filter data is read from mutable caller buffers after the public call returns. There is no persistence, network, offline, or cross-instance reconciliation.

## 5. Boundary contracts

| Contract/boundary | Status | Semantic input | Output/result | Invariants | Errors | Compatibility/versioning | Owner |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Host → preview appearance | proposed | Valid adjustment update or complete snapshot | Applied/retained or typed rejection | Single owner thread; finite and range-valid; last accepted snapshot is atomic | Released, invalid value/invariant | Additive public API; neutral defaults preserve existing behavior | `videolib` public facade |
| Host → filter descriptor | proposed | Contract version `1`, filter snippet, opacity `0..1`, ordered RGBA8888 textures | Applied filter, clear/reset, or typed rejection | Descriptor and payload content become library-owned immutable state; non-empty install requires surface | Invalid version/source/opacity/texture; no surface; compile/link/capability/resource failure | Version field gates future shader ABI changes; version 1 remains supported within this delivery's public compatibility line | `videolib` public facade |
| Version-1 shader snippet ABI | proposed | GLES 3.0 fragment source without `#version` or `main`, defining `vec4 addFilter(vec4 inputColor, vec2 uv)`; optional samplers use ordered names `u_filterTexture0`, `u_filterTexture1`, … | Filtered RGBA color returned to the library adjustment chain | Library owns precision, varying/output, primary sampler, main, and final adjustment order; snippet must not redeclare reserved symbols; texture index equals descriptor order | Missing/wrong entry point, reserved collision, unsupported GLSL, compile/link failure | Exact entry point, argument meaning, UV range/origin, sampler naming, RGBA byte order, linear filtering, and clamp-to-edge behavior are stable for version 1 | `videolib` shader contract |
| Kotlin → JNI | observed, extended | Opaque preview handle plus validated, owned appearance/filter data | Synchronous native acceptance/rejection | Exact Kotlin method/export pairing; strings and texture bytes copied before JNI call returns or retained only in explicitly owned storage; no `JNIEnv*` crosses threads | Null/released handle, conversion/allocation failure | Existing exports unchanged; new exports additive and exact under `Java_com_cii_videolib_VideoPreview_*` | `videolib` Kotlin/native boundary |
| Playback owner → renderer | observed, extended | Complete appearance generation | Transaction result | `rendererMutex_` orders update, presentation, detach, and release; state mutex is not held during blocking GL work | Lifecycle invalidation or render failure | Internal behavior must preserve pause/seek/stop semantics | Native playback owner |
| Renderer → GLES resources | observed, extended | Candidate program, uniforms, primary RGBA input, auxiliary RGBA inputs | One presented frame or candidate rejection | Every GL create/update/draw/delete runs on render executor with correct context current; old generation survives candidate failure | EGL, GL, device-limit, compile/link, allocation/upload | GLES 3.0/minSdk 21 device capability remains required as today | Preview renderer/program |
| `videolib` AAR → host | observed | Kotlin bytecode plus `libvideolib.so` | Loadable public library on packaged ABIs | Existing load name, min SDK 21, `arm64-v8a`/`armeabi-v7a`, C++17, and 16 KB page alignment retained | Unsupported ABI/device remains outside current package contract | No dependency on `DistributationLibrary/videogl`; no publication-coordinate change | `videolib` build boundary |

The version-1 caller snippet is trusted app integration code, not a security sandbox. The library prevents memory-unsafe payload shapes and preserves renderer state on compilation/resource failure, but cannot guarantee that an arbitrary valid GPU program is fast or visually meaningful. Diagnostics identify category and a bounded driver message; shader source and payload bytes are not logged.

## 6. Conditional cross-cutting design

### Concurrency and lifecycle

- Public calls retain `VideoPreview`'s documented single-owner-thread requirement. Native decode still presents from its worker, so the existing renderer serialization boundary orders frames, appearance transactions, detach, and release.
- An accepted adjustment generation must be visible as one complete snapshot. A filter transaction may block presentation while compiling/uploading, but may not allow a frame to observe a new program with old/missing textures or vice versa.
- Detach/release prevents new transactions before waiting for in-flight renderer work. GL objects are destroyed before EGL teardown; native playback is stopped/joined before renderer destruction, preserving current stop-before-release ownership.
- Configuration has preview-instance lifetime only. It is retained across surface recreation, not process death or construction of a new preview.

### Performance and resource safety

- Per-frame work remains one RGBA upload and one screen draw; appearance adjustments are GPU operations at the existing draw boundary. The filter path must not introduce a second decode or CPU frame-copy path.
- Shader compilation and auxiliary upload occur only when the filter changes or a surface is reconstructed, never per frame. Adjustment-only changes reuse the active program/textures.
- Texture dimensions must fit runtime `GL_MAX_TEXTURE_SIZE`; total samplers must fit runtime fragment texture units after reserving the primary frame sampler. Size arithmetic is overflow-safe, allocations are fail-closed, and the library releases superseded/partial resources on the render thread.
- Clarity's reference math samples neighboring image data and is more expensive than uniform-only adjustments. It remains a single-pass part of the appearance program and must be a true bypass at neutral `0`; device verification must include sustained playback rather than treating compilation as performance proof.

### Compatibility, risk mitigation, and architectural verification

| Risk | Evidence/blast radius | Severity | Introduced by change | Owner | Mitigation/verification obligation |
| --- | --- | --- | --- | --- | --- |
| Invalid shader destroys preview | Current `GlProgram::init` has a failure path; external consumers supply source | High | yes | GLES program boundary | Candidate-first atomic swap; supported-device invalid/valid replacement behavior. |
| Update races frame or teardown | Decode worker and render executor meet under `rendererMutex_` | High | yes | Native playback owner | One serialization boundary; device lifecycle stress covering play/pause/seek/detach/release while updating. |
| Malformed/short texture crosses JNI | Reference API trusts parallel buffers/dimensions; new public contract is external | High | yes | Facade/JNI boundary | Immutable descriptor, exact overflow-safe size validation, native-side defensive validation, device-limit rejection. |
| RGBA/YUV shader mismatch | Current `videolib` samples one RGBA texture; reference shell samples Y/U/V | High | yes if copied blindly | Shader assembly owner | Reuse adjustment math only; library-owned RGBA shell and versioned snippet ABI; do not touch FFmpeg conversion. |
| Public/shader ABI drift | Unknown external super-app consumers | High | yes | `videolib` public owner | Additive API, explicit contract version, stable v1 symbols/semantics, consumer compile and AAR inspection. |
| Neutral output changes | Existing program is pass-through; reference omits clarity/vignette invocation | Medium | yes | Appearance contract | Exact defaults, explicit neutral bypass, visual baseline on representative frames. |
| ABI/device-only GL failure | Two ARM ABIs and driver-dependent GLSL | High | yes | Native/build owner | `:videolib:assembleDebug`, package/native-load inspection, and supported-device runs for both ABI classes where available. Compilation alone is not runtime proof. |

Rollback is behavioral and additive: consumers can clear the filter and reset the immutable adjustment snapshot to the documented neutral defaults without recreating playback. A library rollback must leave existing public additions source/binary compatible or be handled as a separately approved public-API migration; no build/publication change is part of this design.

## 7. Coverage audit

| Source item | Covered by | Resolution |
| --- | --- | --- |
| FR-01 / Story-01 | AC-01, AC-02, AC-05; D-05, D-06 | All fourteen adjustments, live application, complete snapshot, order, defaults, and validation defined. |
| FR-02 / Story-01 | AC-02; adjustment table; D-06 | Reference defaults/ranges and math preserved; missing ranges and neutral bypasses explicitly resolved. |
| FR-03 / Story-02 | AC-03, AC-05, AC-06, AC-07; D-07–D-09 | Caller-created filter, active-playback replacement, rejection, and lifecycle retention defined. |
| FR-04 / Story-02 | AC-04; version-1 shader and texture contracts | Shader source plus LUT/overlay/curve-style RGBA texture data defined without sibling types. |
| FR-05 / Story-01, Story-02 | AC-01, AC-03, AC-05 | Subsequent-frame visibility and old-or-new atomicity defined. |
| FR-06 / Story-03 | AC-08; Module Contract Matrix; D-10 | `videolib` owns all production behavior; `app` and external hosts are consumers only. |
| SC-01 | AC-01, AC-05 | Live adjustment outcome covered. |
| SC-02 | AC-02 | Neutral/reference semantics covered. |
| SC-03 | AC-03, AC-05, AC-07 | Live filter outcome and invalid lifecycle handling covered. |
| SC-04 | AC-04 | Auxiliary texture input and pixel contribution covered. |
| SC-05 | AC-08 | Independent module delivery covered. |
| DEV-SPEC question: public filter representation/lifetime | D-05, D-07, D-09; boundary contracts | Immutable versioned descriptor; library-owned copied payload; preview-instance/surface-restorable lifetime. |
| DEV-SPEC question: invalid shader/texture behavior | D-08; AC-03, AC-04 | Typed rejection, bounded diagnostic, candidate cleanup, last-good retention. |
| DEV-SPEC question: filter/adjustment order | D-06 | Filter first, then reference adjustment order with required vignette/clarity positions. |
| DEV-SPEC question: surface recreation | D-09; AC-06 | Retain CPU state and transactionally reconstruct GL projection. |
| DEV-SPEC question: missing ranges | Adjustment table | Proposed explicit ranges for highlights, clarity, and vignette; deterministic reject rather than clamp. |
| DEV-SPEC non-material context conflict | Module Contract Matrix | Current Gradle evidence controls: `app → videolib`. |

No unresolved input remains that can change behavior, data integrity, the public/shader contract, or architecture. Design-Ref coverage is not applicable because no design source was supplied.
