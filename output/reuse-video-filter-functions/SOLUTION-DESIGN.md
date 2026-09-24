AUTOMATION: CONTINUE

# SOLUTION-DESIGN — Reuse video-filter component functions

## 1. Decision ledger

| ID | Status | Decision / evidence | Impact |
| --- | --- | --- | --- |
| D-1 | observed | `videolib` owns the public `VideoFilter` source contract and the native GLES composer; `app` is its direct in-repository sample consumer (`VideoFilter.kt:27-37`, `VideoPreview.kt:130-159,181-183`, `gl_program.cpp:158-169`, `app/build.gradle.kts:55`). | Primary owner is `videolib`; changed-module closure is `videolib` plus the `app` sample. External consumers remain unknown and therefore compatibility-sensitive. |
| D-2 | proposed | Extend the proven library-owned fragment-shader component environment used by `GlProgram::buildGeneration`; do not add another Kotlin, JNI, module, or runtime abstraction. | Consumers can keep the existing version-1 `VideoFilter` entry point while library-owned GLSL components satisfy the short source. |
| D-3 | proposed | A version-1 source uses exactly one provider for the newly shared component declarations. A source declaring none of those component/global names receives the library-owned component set. A source declaring any of them retains the pre-change consumer-owned environment and does not receive the new set. | The requested `addFilter`-only source compiles without duplication, while existing full version-1 snippets do not acquire duplicate top-level declarations. A hybrid source that both declares a newly shared name and depends on other newly shared names is not a promised contract. |
| D-4 | proposed | Preserve `VideoFilter.VERSION_1`, its required `vec4 addFilter(vec4, vec2)` entry point, default version, opacity/textures, validation rules, Kotlin/JNI shapes, and accepted/rejected result model. | No Kotlin source or binary API break, JNI signature change, CMake/linkage change, or ABI packaging change. The behavior change is an additive version-1 shader-source capability with compatibility selection from D-3. |
| D-5 | proposed | The library-owned set reuses the demonstrated component behavior: RGB/HSL conversion, the `vec3` vibrance calculation, hue pulse logic, selective red/orange/yellow/green/cyan/blue adjustments, and their supporting constant/per-fragment hue value from `MainActivity2.kt:500-652`. The existing `vec4` adjustment components in `gl_program.cpp:38-54` remain the provider for exposure, contrast, shadows, saturation, hue, and temperature. | The short body resolves every reference and preserves the requested visual calculation. The new `vec3` vibrance overload must not be substituted with the existing, behaviorally different `vec4` overload. |
| D-6 | observed / retained | Appearance application compiles a candidate program before replacing the active generation; a rejected candidate leaves both native and Kotlin appearance state unchanged (`VideoPreview.kt:130-159`, `gl_program.cpp:244-259`). | Runtime shader failure remains recoverable and cannot remove a working filter. |
| D-7 | intentionally unspecified | Internal storage/formatting of the shared GLSL text, the implementation technique used to recognize consumer declarations, and diagnostic wording beyond the existing public rejection category. | These choices do not alter the component-provider rule, rendered calculation, or public compatibility contract. |
| D-8 | observed | No visual design, remote API, storage, permissions, UI-state, navigation, or publication contract is involved (`DEV-SPEC.md` §§1, 7). | No additional architectural participant is warranted. |

Investigation depth: **Deep**, because a public native/GLES contract has unknown external consumers. Ten grouped lookups were used against a cap of 20, covering the stage packet, required/triggered guidance, DEV-SPEC, source composition, public API/validation/JNI, native application state, CMake/Gradle ownership, focused callers, and the working-tree diff. No material blocker remains.

## 2. Behavior and state transitions

### Behavior contract

| FR-ID | SC-ID | AC-ID | Story-ID | Design-Ref | Rule / trigger | Observable outcome | Failure / recovery |
| --- | --- | --- | --- | --- | --- | --- | --- |
| FR-1 | SC-1 | AC-1 | Story-1 | N/A | A consumer supplies the requested source containing only `vec4 addFilter(vec4 color, vec2 uv)` and no component declarations. | Version 1 resolves all referenced adjustment, RGB/HSL, vibrance, hue-pulse, selective-color, constant, and hue-state symbols from the library-owned shader environment and accepts the filter when the composed GLES program is valid. | Existing validation or GLES compile/link failures remain rejected through `AppearanceUpdateResult`; the last accepted appearance remains active. |
| FR-2 | SC-2 | AC-2 | Story-1 | N/A | The accepted short `addFilter` executes for a frame. | Operations execute in the supplied order with the supplied values: exposure `0.18`, contrast `0.755`, shadows `0.29`, saturation `0.84`, `vec3` vibrance `0.24`, hue `-0.15`, temperature `0.25`, then RGB→HSL, red `(1.46,1.38,1.0)`, orange `(1.2,0.79,1.11)`, yellow `(1.0,0.1,1.12)`, green `(1.0,0.09,0.89)`, cyan `(1.29,1.21,1.0)`, blue `(0.42,1.26,1.0)`, and HSL→RGB. Alpha follows the existing operations and returned `vec4`. | A candidate that cannot preserve a valid composed shader is rejected rather than partially applied. |
| FR-1 | SC-1 | AC-3 | Story-1 | N/A | An existing version-1 source declares one or more names from the newly shared component set. | It is evaluated with the pre-change component environment, so its consumer-owned declarations are not duplicated by the library. | If that consumer-owned source is itself incomplete or invalid, normal validation/compile rejection applies; the library does not combine partial consumer ownership with the new bundle. |
| FR-1 | SC-1 | AC-4 | Story-1 | N/A | A valid candidate source is applied while a surface is available. | Application remains synchronous and atomic: acceptance makes the candidate the active appearance; rejection preserves the previous appearance. | Surface-unavailable, released, validation, allocation, device-capability, shader-compilation, program-link, or render failures retain their existing result semantics and recovery by a later valid call. |

### State model

| State | Meaning / invariants | Permitted events | Prohibited / ignored events |
| --- | --- | --- | --- |
| No active surface | No GLES candidate requiring a filter change can be compiled; retained appearance remains the source for later surface initialization. | Attach a surface; adjustment-only updates allowed by the existing contract. | A changed non-null filter is rejected as surface unavailable. |
| Active appearance | The last accepted filter/program and adjustments are the rendering source of truth. | Submit an unchanged filter/adjustment update; submit a new short or legacy source; detach/release. | A failed candidate must not mutate this state. |
| Candidate evaluation | One proposed appearance is validated, assigned a component-provider mode, composed, and compiled/linked on the native render context. The prior generation remains active until success. | Accept candidate; reject candidate. | Partial replacement or combining library and consumer definitions of the new component set. |
| Released | Native owner is inert and has no future appearance transition. | None within the same instance. | All appearance changes are rejected as released. |

### Transition contract

| From | Event / precondition | To | Side effect | Failure / cancellation / recovery |
| --- | --- | --- | --- | --- |
| No active surface | Surface attachment initializes successfully with the retained appearance. | Active appearance | GLES program is created on the native render context. | Initialization failure leaves no active surface; later attachment may retry. |
| Active appearance | Submit requested short source; source declares none of the new component/global names; surface ready. | Candidate evaluation | Select library-owned components and compose one GLES 3.0 translation unit. | Validation failure returns rejection without compilation or state change. |
| Active appearance | Submit legacy source declaring any new component/global name; surface ready. | Candidate evaluation | Select the pre-change component environment and consumer-owned declarations. | Same atomic rejection behavior. |
| Candidate evaluation | Composed shader compiles/links and resources are valid. | Active appearance | Candidate generation and Kotlin/native appearance snapshot become current. | None. |
| Candidate evaluation | Compile, link, allocation, capability, surface, or render failure. | Active appearance | Return the existing rejection result. | Previous accepted generation remains usable; a later valid submission retries from it. |
| Active appearance / No active surface | Release. | Released | Existing native release semantics apply. | Later submissions remain rejected. |

## 3. Components and responsibilities

### Module Contract Matrix

| Module | Owner / consumer | Responsibility | Depends on | Crossed contract | Compatibility obligation | Verification obligation |
| --- | --- | --- | --- | --- | --- | --- |
| `videolib` | primary owner | Own version-1 filter validation, component-provider selection, final shader composition, runtime compilation, and atomic appearance application. | GLES 3.0/EGL native runtime | Public Kotlin shader-source behavior; Kotlin/JNI/native transport; runtime GLES program | Preserve Kotlin/JNI/source/binary contracts and existing full version-1 sources; add short-source behavior without ABI or packaging change. | `:videolib:assembleDebug`; supported-device shader application/rendering on packaged ARM ABIs. |
| `app` | direct sample consumer | Demonstrate the public short-source contract by supplying only filter-specific behavior. | `videolib` via `implementation(project(":videolib"))` | Public `VideoFilter` consumer behavior | Preserve the exact requested body/order/values and existing surface-attach failure handling. | `:app:assembleDebug`; device smoke observation of accepted filter and rendered output. |
| External `videolib` consumers | unknown public consumers | Continue supplying version-1 filter snippets under the documented contract. | Published/consumed `videolib` artifact, if any | Effective built-in GLSL namespace and runtime behavior | Existing consumer-owned helper declarations must not become duplicate declarations; no recompilation against a changed Kotlin API is required. | Compatibility evidence must include a representative legacy full source as well as the short source at GLES runtime. |

Dependency closure: `videolib` producer → `app` direct runtime/source consumer. `benchmark` targets the app package but does not exercise or own the filter-source contract, so it is outside the changed verification closure. Unknown external `videolib` consumers remain part of compatibility validation.

| Component role | Observed / proposed | Responsibility / owned state | Delegates to | Dependency direction | Must not own / know | Evidence / decision |
| --- | --- | --- | --- | --- | --- | --- |
| Filter consumer | observed, simplified | Own only the filter-specific `addFilter` algorithm, its constants as literal arguments, opacity, and optional textures. | Public `VideoFilter`/`VideoPreview` boundary | `app` or external host → `videolib` | Implementations or supporting declarations of reusable component functions. | FR-1; `MainActivity2.DEMO_VIDEO_FILTER`. |
| Public appearance facade | observed, unchanged | Own Kotlin-side validation, synchronous accepted/rejected result, and last accepted public appearance snapshot. | JNI appearance boundary | Kotlin API → native owner | GLSL helper implementation details or provider-selection mechanics. | `VideoPreview.kt:130-159,296-342,383-392`; D-4. |
| Shader component environment | proposed extension of existing seam | Own the reusable built-in GLSL adjustment/conversion/selective-color behavior and complete supporting declarations needed by the short body. It exposes that environment only during composition. | GLES compiler through the composer | Native composer → component text | Consumer-specific parameter values, filter ordering, opacity, textures, or active appearance state. | Existing prefix at `gl_program.cpp:26-55`; demonstrated components at `MainActivity2.kt:500-652`; D-5. |
| Shader composer / generation owner | observed, changed responsibility | Own component-provider selection, final single-translation-unit assembly, candidate program/resources, and atomic generation replacement. | GLES compile/link and resource allocation | Native appearance owner → GLES | Kotlin public-state ownership or filter-specific business choices. | `gl_program.cpp:158-169,244-259`; D-2, D-3, D-6. |
| Native appearance owner / render executor | observed, unchanged | Serialize appearance application with rendering, require a ready surface for changed filters, retain the accepted native snapshot, and execute GL work with the EGL context current. | Shader generation owner | JNI/native playback → render executor → shader composer | Parsing or altering consumer filter calculations. | `video_playback.cpp:176-210`; `preview_renderer.cpp:109-124`. |

## 4. End-to-end data flow

### Flow A — requested short filter

| Step | Participant | Input / source | Decision / transformation | Output / side effect | Error propagation |
| --- | --- | --- | --- | --- | --- |
| 1 | Filter consumer | Only the requested `addFilter` definition | Wrap as the existing version-1 `VideoFilter`; no helper source is copied. | Immutable public filter value. | Kotlin validation may reject existing invalid version/opacity/source/texture conditions. |
| 2 | Public appearance facade | Candidate filter plus current adjustments | Require a live owner and ready surface for a changed filter; transport the unchanged semantic fields across JNI. | Native appearance candidate. | Existing public rejection reason is returned synchronously. |
| 3 | Native appearance owner | Candidate snapshot | Revalidate the existing contract and serialize application against renderer state. | Candidate reaches the current EGL render context. | Native validation/surface/release errors map to the existing result. |
| 4 | Shader composer | Consumer source and library shader environment | Because no new component is declared by the source, select the library-owned complete component set; concatenate prefix, auxiliary uniforms, consumer `addFilter`, and suffix. | One GLES 3.0 fragment shader translation unit satisfying AC-1/AC-2. | Composition/allocation failure rejects the candidate. |
| 5 | GLES runtime | Composed fragment shader | Compile/link a candidate program and resolve/upload existing resources. | Valid candidate generation. | Shader, link, capability, or allocation failure rejects without replacing active state. |
| 6 | Generation owner and facade | Accepted candidate | Atomically replace native generation, then update retained native/Kotlin appearance snapshots. | Subsequent frames render the requested calculation; caller receives `Accepted`. | If acceptance cannot complete, AC-4 retains the prior source of truth. |

Source of truth is the last accepted appearance: the Kotlin snapshot represents public state and the matching native generation renders it. Selection and compilation occur synchronously in established render-thread order; no cache, persistence, offline behavior, or reconciliation source exists.

### Flow B — existing full version-1 filter

| Step | Participant | Input / source | Decision / transformation | Output / side effect | Error propagation |
| --- | --- | --- | --- | --- | --- |
| 1 | Existing consumer | Version-1 source declaring one or more of the newly shared component/global names | Preserve consumer ownership mode. | Candidate retains the pre-change library prefix plus its own declarations. | Existing validation remains unchanged. |
| 2 | Shader composer / GLES runtime | Legacy candidate | Do not add the new component set; compose and compile through the established candidate path. | Previously valid full snippets avoid duplicate declarations and remain eligible for acceptance. | Existing compile/link rejection and atomic recovery apply. |

## 5. Boundary contracts

| Contract / boundary | Observed / proposed / blocked | Semantic input | Output / result | Invariants | Errors | Compatibility / versioning | Owner |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Consumer → `VideoFilter` | observed, unchanged | Trusted GLES 3.0 source defining `vec4 addFilter(vec4, vec2)`, opacity, ordered optional textures | Immutable filter value | Existing reserved-token and texture rules remain; short source contains only `addFilter`. | Existing Kotlin rejection reasons | Remains version 1 and default version; no public signature change. | `videolib` public API |
| Version-1 source → component environment | proposed | Consumer source declarations and calls | Either the complete new library-owned component set or the pre-change environment | Exactly one provider of newly shared declarations; no mixed/partial provider guarantee; requested short source resolves every reference. | Invalid composed source is rejected through existing shader/program failure semantics. | Additive capability; legacy declaring sources retain old namespace environment. | Native shader composer |
| Shared component behavior | proposed | Per-pixel color/UV and filter-specific numeric arguments | Adjustment, conversion, hue-pulse, and selective-color results | Demonstrated formulas and overload semantics are preserved; the `vec3` vibrance implementation remains distinct from existing `vec4` vibrance; support state is per fragment evaluation and cannot leak between frames/fragments. | Non-finite/undefined consumer calculations remain GLSL consumer responsibility as before. | Names used by the requested body become available in library-component mode; consumer need not declare or maintain them. | `videolib` shader component environment |
| Kotlin → JNI → native snapshot | observed, unchanged | Version, source, opacity, texture dimensions/bytes, adjustments | Native appearance candidate and public result | Field order/types and JNI export remain exact; no new parameter crosses JNI. | Existing conversion/released/validation errors | Binary/native ABI preserved. | `VideoPreview` and native JNI facade |
| Native composer → GLES runtime | observed, extended | One final GLES 3.0 vertex/fragment program and resources | Linked candidate generation or failure | Compilation/link occurs with the EGL context current; candidate replaces active generation only after full success. | Existing generic shader compilation/program link/resource/capability errors | Runtime support remains GLES 3.0 on the module's packaged `arm64-v8a` and `armeabi-v7a` ABIs. | Native render boundary |

## 6. Conditional cross-cutting design

### Compatibility, risk, and rollback

| Risk | Evidence / blast radius | Severity | Mitigation contract |
| --- | --- | --- | --- |
| Duplicate GLSL symbols break existing external version-1 filters at runtime. | Prefix and consumer source share one translation unit (`gl_program.cpp:158-169`); external consumers are unknown. | High | D-3/AC-3 preserve the pre-change environment whenever the consumer declares a new shared name. Compatibility evidence covers both source modes. |
| The wrong vibrance overload changes rendered output while still compiling. | `MainActivity2.kt:549-557` is `vec3` with a distinct formula; `gl_program.cpp:45` is `vec4`. | Medium | AC-2 and the shared-component contract require the demonstrated `vec3` behavior for `color.rgb`. |
| Build success is mistaken for shader correctness. | Shader compile/link happens at runtime (`gl_program.cpp:66-117`). | High | Assembly proves C++/JNI/linkage only; supported-device runs prove short/legacy shader acceptance and rendering. Both packaged ARM ABIs remain in scope where hardware is available. |
| Failed candidate replaces a working appearance. | Public and native state update only after acceptance (`VideoPreview.kt:154-158`, `gl_program.cpp:252-259`). | High | Retain candidate-first atomic replacement and old-generation ownership until success. |

The behavioral rollback boundary is the component-environment extension: removing it restores the pre-change shader namespace without a Kotlin/JNI/API migration. No publication, artifact distribution, or version migration is part of this design.

### Architectural verification obligations

| Obligation | Evidence established |
| --- | --- |
| Owning native library compiles and links for its configured variants/ABIs. | `:videolib:assembleDebug` covers the CMake target packaged by the AAR, but not runtime GLSL behavior. |
| Direct consumer compiles using only the short source. | `:app:assembleDebug` covers the `app` → `videolib` source/build edge. |
| Short-source public behavior is real on GLES. | A supported-device run must accept the exact AC-1 source and visibly render AC-2; runtime compilation cannot be inferred from assembly. |
| Version-1 compatibility mode avoids duplicate declarations. | A supported-device run must continue accepting a representative pre-change full source that declares the shared components. |
| Failure remains atomic. | A runtime-invalid candidate must be rejected while the prior accepted appearance continues rendering. |

## 7. Coverage audit

| Source item | Design resolution |
| --- | --- |
| FR-1 / SC-1 / Story-1 | AC-1 defines addFilter-only acceptance; D-2 and the component environment place reusable behavior in `videolib`; D-3/AC-3 resolve legacy source collisions; AC-4 preserves public failure/recovery. |
| FR-2 / SC-2 / Story-1 | D-5, AC-2, Flow A, and the shared-component boundary preserve the exact operation order, arguments, formulas, overload, and alpha behavior. |
| Edge: consumer symbol collision | D-3, AC-3, Flow B, and compatibility mitigation define mutually exclusive provider modes without breaking existing declaring sources. |
| Edge: Kotlin/native textual validation | D-4 and the consumer/JNI boundary preserve the required signature, reserved-token behavior, and result types. |
| Edge: device/GPU variance | GLES boundary and verification obligations require runtime evidence on supported packaged ABIs; build evidence is explicitly limited. |
| Risk: simplifying only `app` | Module matrix fixes `videolib` as owner and `app` as consumer, so the capability exists at the public library boundary. |
| Open assumption: every supporting declaration should disappear from consumer source | AC-1 confirms this interpretation; the requested body remains the only consumer shader declaration. |

Unresolved inputs: none. Unknown external consumers are handled by an explicit compatibility contract rather than requiring their enumeration.
