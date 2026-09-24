AUTOMATION: CONTINUE

# SOLUTION-DESIGN — apply-curve-acv-on-video-ndk

## 1. Decision ledger

**Investigation depth:** standard (12) · **lookups used:** 11 · DEV-SPEC read as the authoritative
input; guard satisfied (`AUTOMATION: CONTINUE` on its first line).

### Sources and evidence

| # | Source | Used for |
|---|---|---|
| E-1 | `videolib/src/main/cpp/gl_program.cpp:321-332` | single shader-assembly site; three-block concatenation order |
| E-2 | `gl_program.cpp:57-211` (`kSharedFilterComponents`) | the conditional component block |
| E-3 | `gl_program.cpp:27-56` (`kFragmentPrefix`) | the unconditional prefix block |
| E-4 | `gl_program.cpp:277-281` (`consumerOwnsSharedFilterComponents`) | compatibility-mode regex and its component list |
| E-5 | `gl_program.cpp:326` | `u_filterTextureN` uniforms are emitted **only** for supplied textures |
| E-6 | `gl_program.cpp:410-425` | atomic generation swap; previous generation survives a failed build |
| E-7 | `gl_program.cpp:362-375` | filter textures: `GL_RGBA8`, `GL_LINEAR`, `GL_CLAMP_TO_EDGE`, bound from `GL_TEXTURE1` |
| E-8 | `offscreen_renderer.cpp:58,82-90` + `offscreen_renderer.h:14` | export reuses the same `GlProgram` |
| E-9 | `VideoPreview.kt:427-473`, `:553-562` | filter validation, entry-point regex, reserved tokens |
| E-10 | `VideoExporter.kt:245-281` | the duplicated validation site |
| E-11 | `camera/src/main/assets/glsl/frag_base_shader_adjust.glsl:151-156` | canonical `applyCurve` body |
| E-12 | `VideoFilter.kt:27-39` | the published version-1 component contract (KDoc) |
| E-13 | `app/build.gradle.kts:55`, `MainActivity2.kt:877` | the one in-repo consumer and its filter install site |

### Decisions

| ID | Decision | Status | Impact |
|---|---|---|---|
| D-1 | `applyCurve` is added to the **conditional** shared-component block (E-2), not the unconditional prefix (E-3). | decided | Preserves the existing rule that a consumer redeclaring any library component owns the whole component environment. Putting it in the prefix would make redeclaration an unavoidable duplicate-definition compile error for existing filters. |
| D-2 | `applyCurve` joins the compatibility-mode component list (E-4) **and** the published KDoc list (E-12) in the same change. | decided | Resolves DEV-SPEC Q-3. The two lists are one logical contract expressed twice; divergence produces a silently wrong compatibility decision (DEV-SPEC risk 2). |
| D-3 | The curve LUT travels through the **existing** `VideoFilterTexture` → `u_filterTextureN` path. No new texture kind, no new native plumbing, no new uniform. | decided | Follows DEV-SPEC FR-3/A-1. A 256×1 RGBA8888 LUT already satisfies the `w*h*4` validator (E-9) and the existing upload path (E-7). |
| D-4 | No change to `VideoFilter`, `VideoFilterTexture`, `VideoAppearance`, `AppearanceRejectionReason`, or any JNI signature. | decided | Satisfies SC-3. The change is confined to the GLSL string content plus the two component lists. |
| D-5 | `applyCurve` adopts the `camera` body semantics (E-11): per-channel dependent LUT fetch, alpha preserved. | decided | Reuses a proven body; the ticket names the same function. |
| D-6 | Curve **composition** (which curves the LUT encodes), **interpolation**, and **channel packing** are consumer concerns, outside the `videolib` contract. `videolib` specifies only how the LUT texture is sampled. | decided | Follows A-1. Resolves DEV-SPEC Q-1/Q-2/C-1 as out-of-contract rather than unanswered. |
| D-7 | Preview and export inherit the change through the shared `GlProgram` (E-8); no export-side design is added. | decided | FR-5 holds by construction. |
| D-8 | No validation-rule change, so the duplicated validators (E-9/E-10) are not touched. | decided | Neutralises DEV-SPEC risk 3 for this change; the duplication itself remains, out of scope. |

### Assumptions

| ID | Assumption | Status | Impact if wrong |
|---|---|---|---|
| AS-1 | Adding one function to the conditional block does not exceed any device's fragment-shader limits. | proposed | The block is already ~150 lines (E-2); one 6-line function is marginal. Would surface as `SHADER_COMPILATION`. |
| AS-2 | No existing out-of-repo consumer declares its own `applyCurve` **and** relies on not being in compatibility mode. | proposed | Such a consumer would newly enter compatibility mode and must then supply the full component environment. This is the accepted behavior of the existing contract, not a new failure mode. |

### Explicitly unspecified (implementation-local)

Exact placement within the component block; identifier names for the sampler parameter; log/diagnostic
wording; whether the KDoc lists `applyCurve` inline or in a separate sentence.

### Blockers

None. DEV-SPEC's deferrable items Q-1, Q-2, C-1 and U-2 are resolved as **out of the `videolib`
contract** by D-6 — they shape the consumer's LUT bytes, which `videolib` treats as opaque.

## 2. Behavior and state transitions

### Behavior contract

| FR-ID | SC-ID | AC-ID | Story-ID | Rule / trigger | Observable outcome | Failure / recovery |
|---|---|---|---|---|---|---|
| FR-1, FR-4 | SC-1 | AC-1 | S-1 | A version-1 filter source calls `applyCurve(color, u_filterTextureN)` without declaring it. | Shader assembles and links; filter installs (`Accepted`). | If the named sampler was not supplied as a texture, the uniform is never emitted (E-5) and linking fails → `SHADER_COMPILATION` / `PROGRAM_LINK`. |
| FR-4 | SC-1 | AC-2 | S-1 | A filter source **declares** its own `applyCurve`. | Compatibility mode engages; the library component block is omitted and the source must supply every component it uses. | A source that redeclares `applyCurve` but still calls e.g. `RGBtoHSL` without declaring it fails to compile → `SHADER_COMPILATION`. Same as today's behavior for the other components. |
| FR-2, FR-3 | SC-2 | AC-3 | S-2 | A 256×1 RGBA8888 LUT is supplied as a filter texture and sampled by `applyCurve`. | Rendered video is tone-mapped by the LUT; visibly different from the same video without the curve. | Malformed texture is rejected before any GL work → `INVALID_FILTER_TEXTURE` (E-9). |
| FR-3 | SC-3 | AC-4 | S-2 | Any consumer installs a curve. | No new public Kotlin type is required; the existing `VideoFilter(source, textures)` shape carries it. `videolib` acquires no `Context`/`AssetManager` dependency. | — |
| FR-5 | — | AC-5 | S-3 | The same appearance is used for preview and for export. | Export output carries the identical curve, because both paths build the shader through one `GlProgram` (E-8). | A failure in either path is the same `AppearanceError`; export surfaces it through its own listener. |
| FR-1 | SC-1 | AC-6 | S-1 | A filter that previously compiled is installed after this change. | Still compiles and renders identically — `applyCurve` is additive and unreferenced by existing sources. | An existing source that already declared `applyCurve` now enters compatibility mode (AS-2) — the contract's defined behavior, not a regression path. |

### State model

The feature introduces no new state. The relevant existing states of the appearance program:

| State | Meaning / invariants | Permitted events | Prohibited / ignored |
|---|---|---|---|
| `no-program` | `GlProgram` not initialised; `isReady()` false. | init | apply-appearance → `RenderFailure` |
| `generation-active` | One linked program plus its filter textures and resolved uniform locations are current. | apply-appearance, draw, release | — |
| `generation-rebuilding` | A candidate generation is being built; the active generation is still bound. | — (render-thread-serial) | concurrent apply — all `GlProgram` work is render-thread-only |

### Transition contract

| From | Event / precondition | To | Side effect | Failure / recovery |
|---|---|---|---|---|
| `generation-active` | apply-appearance where the filter is **unchanged** | `generation-active` | Adjustment uniforms updated; **no** shader rebuild (E-6). | — |
| `generation-active` | apply-appearance with a **new or changed** filter (including first curve install) | `generation-rebuilding` | Candidate shader assembled with the component block decided by D-1/D-2. | — |
| `generation-rebuilding` | candidate links successfully | `generation-active` | Candidate swapped in atomically; previous generation released (E-6). | — |
| `generation-rebuilding` | candidate fails to compile or link | `generation-active` (unchanged) | Candidate discarded; **the previous generation remains bound and rendering continues** (E-6). | Caller receives `SHADER_COMPILATION` / `PROGRAM_LINK`; no visual interruption. |

This last row is the material correctness property for AC-1/AC-2: a bad curve filter degrades to a
rejection, never to a black or torn frame.

## 3. Components and responsibilities

### Module contract matrix

| Module | Owner / consumer | Responsibility | Depends on | Crossed contract | Compatibility obligation | Verification obligation |
|---|---|---|---|---|---|---|
| `videolib` | **primary owner** | Provides `applyCurve` as a version-1 GLSL component; samples a consumer-supplied LUT texture. | — | version-1 GLSL component contract (native block + published KDoc) | **Additive only.** No existing filter source may change behavior; no Kotlin/JNI signature changes. Redeclaration semantics must match the other components. | `:videolib:assembleDebug` + on-device shader compile + both declared ABIs (`arm64-v8a`, `armeabi-v7a`) |
| `app` | consumer | Owns `.acv` parsing, curve composition, and LUT byte layout (D-6); installs via existing `VideoFilter`. | `videolib` | none newly crossed — uses the existing public filter API | none; existing filters keep working (AC-6) | `:app:assembleDebug` + visual preview/export comparison |
| `camera` | evidence source only | — | — | none | **No dependency edge is created** — the `applyCurve` body is reused as source-level precedent, not as a module dependency. | — |
| external `videolib` consumers | unknown consumer | — | `videolib` | version-1 component contract | Additive change; only a consumer that already declared `applyCurve` sees a behavior change (AS-2). | not enumerable (DEV-SPEC U-1) |

### Components

| Component role | Observed / proposed | Responsibility / owned state | Delegates to | Dependency direction | Must not own / know | Evidence / decision |
|---|---|---|---|---|---|---|
| Shader component library (the conditional GLSL block) | observed, **extended** | Owns the set of reusable colour functions available to a version-1 filter source; gains `applyCurve`. | — | depended upon by every filter source | Must not know how a LUT was produced, which curves it encodes, or where it came from. | E-2, D-1, D-5 |
| Compatibility-mode detector | observed, **extended** | Owns the decision of whether a consumer source has taken ownership of the component environment; its component list must equal the block's. | — | consulted by shader assembly | Must not diverge from the published contract. | E-4, D-2 |
| Published filter contract (KDoc on the public type) | observed, **extended** | Owns the consumer-facing statement of which components exist and what redeclaration implies. | — | read by consumers | — | E-12, D-2 |
| Shader assembly | observed, unchanged | Owns concatenation order and the emission of `u_filterTextureN` uniforms for supplied textures. | component block, detector | — | Must not special-case curves. | E-1, E-5 |
| Filter texture pipeline | observed, unchanged | Owns upload, sampler parameters, and unit binding of auxiliary textures. | — | — | Must not interpret texture content. | E-7, D-3 |
| Curve source (consumer-side) | proposed, **outside `videolib`** | Owns `.acv` decoding, curve composition, interpolation, and packing into RGBA8888. | — | depends on `videolib` public API | Must not assume `videolib` validates or interprets curve semantics. | D-6, A-1 |

## 4. End-to-end data flow

### Flow 1 — installing a curve filter (preview)

| Step | Participant | Input / source | Decision / transformation | Output / side effect | Error propagation |
|---|---|---|---|---|---|
| 1 | consumer | `.acv` bytes (consumer-owned origin) | decode + compose + interpolate into a 256×1 RGBA8888 LUT (D-6, opaque to `videolib`) | LUT byte array | consumer-owned; never reaches `videolib` |
| 2 | consumer | LUT + filter source calling `applyCurve` | construct the appearance | appearance snapshot | — |
| 3 | public appearance API | appearance snapshot | structural validation: entry point present, reserved tokens absent, each texture exactly `w*h*4` (E-9); surface attached for a new filter | accepted or `Rejected(reason)` | rejection returns synchronously; no GL work performed |
| 4 | JNI boundary | validated snapshot | marshal adjustments, source, and texture bytes | native snapshot | marshalling failure → `INVALID_FILTER_TEXTURE` |
| 5 | shader assembly | native snapshot | emit `u_filterTextureN` per supplied texture (E-5); include the component block unless the source redeclares a component (E-4, D-2); concatenate prefix + components + source + suffix | candidate shader | — |
| 6 | GL program | candidate shader | compile + link on the render thread with the context current | candidate generation, or failure | `SHADER_COMPILATION` / `PROGRAM_LINK`; **active generation retained** (E-6) |
| 7 | GL program | candidate generation | upload LUT as `GL_RGBA8`, `GL_LINEAR`, `GL_CLAMP_TO_EDGE`, bind from `GL_TEXTURE1` (E-7) | textures resident | capability/allocation failure → `DEVICE_CAPABILITY` / `RESOURCE_ALLOCATION` |
| 8 | GL program | candidate generation | atomic swap; release previous | curve active | — |
| 9 | render | frame + active generation | `addFilter` runs, `applyCurve` performs three dependent LUT fetches, result mixed by filter opacity, then the adjustment chain | tone-mapped frame (AC-3) | — |

### Flow 2 — export

Identical through steps 1–8, entered from the export appearance path and executed by the offscreen
renderer, which holds its own `GlProgram` instance built from the same assembly (E-8). No separate
design; see AC-5. Per-segment timeline export carries per-segment textures, so each segment may hold
a different curve — an existing capability, not a new one.

## 5. Boundary contracts

| Contract / boundary | Status | Semantic input | Output / result | Invariants | Errors | Compatibility / versioning | Owner |
|---|---|---|---|---|---|---|---|
| Version-1 GLSL component environment | **observed, extended** | a filter source | the set of functions callable without declaration | The component block, the compatibility regex, and the published KDoc list the **same** components. `applyCurve` is added to all three. | a source calling an unavailable component fails to compile | Additive within version 1; no version bump. Existing sources unaffected (AC-6). | `videolib` |
| `applyCurve` semantics | **proposed** | a colour and a sampler holding a LUT | a colour whose R, G, B are independently remapped through the LUT's corresponding channels; alpha unchanged | The LUT is sampled by **channel value as coordinate**; content is not interpreted. Sampling uses the pipeline's existing `GL_LINEAR` + `GL_CLAMP_TO_EDGE` parameters, so adjacent LUT entries are interpolated and out-of-range coordinates clamp to the edge entries. | none of its own | new symbol; no prior meaning to preserve | `videolib` |
| Auxiliary filter texture | observed, unchanged | ordered RGBA8888 textures | ordered `u_filterTextureN` samplers | exactly `w*h*4` bytes; a sampler exists only if its texture was supplied (E-5) | `INVALID_FILTER_TEXTURE` | unchanged | `videolib` |
| Curve encoding (composition, interpolation, channel packing) | **intentionally outside the contract** | — | — | `videolib` treats LUT bytes as opaque (D-6) | — | Consumers may change their encoding freely without a `videolib` change. | consumer |
| Appearance install result | observed, unchanged | appearance | `Accepted` / `Rejected(reason, diagnostic)` | Rejection leaves the previously active appearance rendering (E-6) | existing reason set; **no new reason added** (D-4) | unchanged | `videolib` |
| Preview ↔ export parity | observed, unchanged | same appearance | same rendered result | Both build through one shader assembly (E-8) | — | must not diverge — the reason D-8 keeps validation untouched | `videolib` |
| JNI / ABI surface | observed, unchanged | — | — | No signature, marshalling, or export-name change (D-4) | — | binary-compatible | `videolib` |

## 6. Conditional cross-cutting design

**Concurrency / threading.** All `GlProgram` work is render-thread-only with the EGL context
current (`gl_program.h:1-2`). This design adds no new call site, no new thread interaction, and no
new GL resource; the curve rides the existing texture path. The generation swap is already
serialised on that thread (E-6).

**Failure behavior.** The correctness-critical property is in §2's last transition row: a filter
that fails to build leaves the active generation rendering. Because this change adds a callable
symbol rather than a new failure mode, an incorrect curve filter is rejected exactly like any other
malformed filter today.

**Performance.** `applyCurve` costs three dependent texture fetches per pixel, only for sources that
call it. Sources that do not call it are byte-identical after assembly except for the added
declaration in the component block. DEV-SPEC records no performance budget; min SDK is 21.

**Contract duplication (risk mitigation).** The single highest risk in DEV-SPEC is the component
list existing in two places (native regex E-4, published KDoc E-12). D-2 makes updating both a
contract obligation of this design rather than an implementation detail. The architectural
verification obligation is: a filter source that redeclares `applyCurve` must be observed to enter
compatibility mode — this is the only check that proves both lists agree.

**Verification obligations (architectural, not a test plan).**

| Obligation | Why it is architectural |
|---|---|
| On-device shader compile of a source calling `applyCurve` | GLSL compiles at runtime; `assembleDebug` cannot prove the component block is well-formed (DEV-SPEC risk 5). |
| On-device observation that a redeclaring source enters compatibility mode | The only evidence that the regex and the published contract agree (D-2). |
| An existing filter that does **not** call `applyCurve` still renders identically | Proves the additive-only obligation (AC-6). |
| Both declared ABIs | Module carries `abi`/`jni` risk tags; native artifact changes. |
| Preview and export compared for the same appearance | Proves AC-5 empirically rather than by inheritance. |

No design conformance contract: no design source exists (DEV-SPEC §0, focus area 6 `N/A`).

No DI, navigation, storage, migration, permission, or security concern is implicated — `videolib`
has no DI framework, no persistence, and this change adds no data at rest and no new input channel.

## 7. Coverage audit

| Input | Resolved by |
|---|---|
| FR-1 (define `applyCurve`) | D-1, D-5; AC-1 |
| FR-2 (apply `.acv` to NDK video) | D-3, D-6; AC-3; Flow 1 |
| FR-3 (videolib provides GLSL only) | D-3, D-4, D-6; AC-4; §5 "intentionally outside the contract" |
| FR-4 (callable without redeclaration) | D-1, D-2; AC-1, AC-2 |
| FR-5 (preview/export parity) | D-7, D-8; AC-5; Flow 2 |
| SC-1 | AC-1, AC-2, AC-6 |
| SC-2 | AC-3 |
| SC-3 | AC-4, D-4 |
| S-1, S-2, S-3 | AC-1/AC-2/AC-6, AC-3/AC-4, AC-5 |
| Q-1 (composite handling) | D-6 — out of `videolib` contract |
| Q-2 (spline vs linear) | D-6 — out of `videolib` contract |
| Q-3 (compatibility list membership) | **D-2 — yes, both lists** |
| Q-4 (unit-testability of GLSL) | §6 verification obligations — device-observed, not host-testable |
| C-1 (three inconsistent `camera` ACV readers) | D-6 — consumer-side; `videolib` specifies only sampling semantics. Consolidating `camera`'s duplication is out of scope. |
| U-1 (external consumers unknown) | §3 matrix + AS-2 — additive-only obligation |
| U-2 (no `.acv` fixture in this repo) | D-6 — no fixture is needed to satisfy the `videolib` contract; a consumer-side curve is required only for the visual obligation in §6 |
| DEV-SPEC risk 1 (component contract change) | D-1 (conditional block, not prefix) + AS-2 |
| DEV-SPEC risk 2 (two-place list) | D-2 + §6 verification obligation |
| DEV-SPEC risk 3 (duplicated validators) | D-8 — no validation change, so no divergence introduced |
| DEV-SPEC risk 4 (wrong LUT convention) | D-6 — moved out of contract; §5 states the sampling semantics the consumer must target |
| DEV-SPEC risk 5 (runtime-only shader failure) | §6 verification obligations |
| DEV-SPEC risk 6 (ABI) | §3 matrix verification obligation |
| DEV-SPEC risk 7 (`camera → videolib` edge) | §3 matrix — explicitly no dependency edge |
| DEV-SPEC risk 8 (per-pixel cost) | §6 performance |

**Unresolved inputs:** none.
