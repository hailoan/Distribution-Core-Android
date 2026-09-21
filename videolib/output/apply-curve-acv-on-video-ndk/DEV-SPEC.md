AUTOMATION: CONTINUE

# DEV-SPEC — apply-curve-acv-on-video-ndk

## 0. Analysis Control

| Field | Value |
|---|---|
| Outcome | `CONTINUE` |
| Ticket kind | `feature` |
| Scope classification | existing-code, native/JNI-adjacent, public-library contract, cross-module reuse evidence |
| Path taken | full path (existing native + public API surface) |
| Depth selected | standard (14) |
| Approx. lookups used | 13 |
| Escalation | none |
| Clarification asked | 1 (ownership of `.acv` parsing — answered, see FR-3 / A-1) |

Focus-area applicability:

| # | Focus area | Status |
|---|---|---|
| 1 | Requirements | applicable — §2, §3, §4, §5 |
| 2 | Edge cases | applicable — §5a (all proposed, `[assumption]`/`[unknown]`) |
| 3 | Feature impact | applicable — §6 |
| 4 | Risk | applicable — §9 |
| 5 | API docs | N/A — no remote/backend/network surface; the only "API" here is the in-process `videolib` public Kotlin + GLSL contract, covered in §6/§7 |
| 6 | Figma/design | N/A — no design source supplied and none required for a shader-level colour transform |

Lookup ledger:

| # | Lookup | Material evidence gained |
|---|---|---|
| 1 | `videolib/output/apply-curve-acv-on-video-ndk.md` | ticket text; referenced `.acv` path |
| 2 | `xxd` of `be_art_free_blog_tone.acv` | file exists, 110 bytes, version 4, 5 curves |
| 3 | `videolib/src/main/cpp` + `src/main/java` listing | current native/Kotlin inventory |
| 4 | `VideoFilter.kt` | `VideoFilter` / `VideoFilterTexture` contract; version-1 component list |
| 5 | `VideoAppearance.kt` | appearance snapshot shape |
| 6 | `appearance.h` | native mirror + `AppearanceError` codes |
| 7 | `gl_program.h` | render-thread-only contract; generation/uniform model |
| 8 | grep `acv|applyCurve|toneCurve` across repo | `camera` precedent found (`CurveTone.java`, `frag_base_shader_adjust.glsl`) |
| 9 | `CurveTone.java` + `AssetReader.kt` + `GLPreview.kt:77` | three existing ACV implementations, three different LUT conventions |
| 10 | `frag_base_shader_adjust.glsl:151` | canonical `applyCurve(vec4, sampler2D)` body |
| 11 | `gl_program.cpp` (texture + preamble + regex) | `u_filterTextureN` plumbing, `GL_LINEAR`/`GL_CLAMP_TO_EDGE`, compatibility-mode regex at :280 |
| 12 | `VideoPreview.kt:427-473` + `:553-562` | filter validation, `FILTER_ENTRY_POINT`, `RESERVED_FILTER_SOURCE` |
| 13 | `offscreen_renderer.h:14`, `VideoExporter.kt`, `MainActivity2.kt:878` | export shares `GlProgram`; app is the one in-repo consumer |

## 1. Sources

| Source | Type | Location | Revision / read status |
|---|---|---|---|
| Ticket | markdown | `videolib/output/apply-curve-acv-on-video-ndk.md` | read; 2 lines |
| Sample ACV | binary asset | `../CameraVintage/app/src/main/assets/filters/acv/be_art_free_blog_tone.acv` | read (hexdump); 110 bytes; **outside this repository** |
| Clarification | user answer | this `/study` run | recorded as `[fact:clarification]` |

Design index: N/A — no design source supplied.
API docs: N/A — no remote/backend surface.
Converted files: none — the ticket was already Markdown.

## 2. Overview & Business Goal

Define an `applyCurve` function and make a Photoshop `.acv` tone curve applicable to video rendered
through the `videolib` NDK pipeline. `[fact:ticket]`

The sample curve is a real Photoshop ACV: version 4, 5 curves (RGB composite, R, G, B, plus a
trailing 2-point identity curve), each curve a `(output, input)` short pair series in 0..255.
`[fact:ticket]` The composite and per-channel curves are all non-identity, so the intended visual
result is a combined composite+per-channel tone mapping, not a single-channel tweak.
`[assumption:code]` — derived from the byte layout, not stated in the ticket.

## 3. Functional Requirements

| FR-ID | Requirement | Status | Evidence / source |
|---|---|---|---|
| FR-1 | A function named `applyCurve` must exist and be callable when authoring a `videolib` video filter. | `[fact:ticket]` | ticket: "define a function applyCurve" |
| FR-2 | A Photoshop `.acv` curve must be applicable to video rendered by the `videolib` NDK pipeline. | `[fact:ticket]` | ticket: "apply file acv on video ndk" |
| FR-3 | `videolib` provides the `applyCurve` GLSL component only; parsing the `.acv` and producing the LUT texture stays with the consumer, which supplies it through the existing `VideoFilterTexture` / `u_filterTextureN` path. | `[fact:clarification]` | user answer, this run |
| FR-4 | `applyCurve` must be usable from a version-1 `VideoFilter` source without the consumer redeclaring it. | `[assumption:clarification+code]` | follows from FR-3 + the version-1 component contract at `VideoFilter.kt:27-39`; the ticket does not state a version |
| FR-5 | The curve must apply identically in preview and in export. | `[assumption:code]` | `offscreen_renderer.h:14` states export "reuses the exact GlProgram appearance+filter pipeline as preview"; the ticket says "video", not "preview" |

FR-5 is marked `[assumption]` because the ticket says only "video". It is classified
**non-material** in §8: preview/export parity is a property of the existing shared `GlProgram`, so
FR-5 holds by construction unless a design deliberately breaks it.

## 4. Actors & User Stories

| Story-ID | FR-ID | Story |
|---|---|---|
| S-1 | FR-1, FR-3, FR-4 | As a filter author using `videolib`, I can call `applyCurve(color, u_filterTexture0)` inside `addFilter` without declaring the function myself. |
| S-2 | FR-2, FR-3 | As the consuming app, I can load a `.acv` file, produce a LUT texture, and hand it to `videolib` through the existing `VideoFilter` texture list. |
| S-3 | FR-5 | As the consuming app, the exported MP4 carries the same curve as the on-screen preview. |

Actors: **filter author** (writes GLSL passed as `VideoFilter.source`) and **consumer app**
(`app`, and unknown out-of-repo consumers of the `videolib` library).

## 5. Observable Success Conditions

| SC-ID | FR-ID | Outcome | Evidence / source |
|---|---|---|---|
| SC-1 | FR-1, FR-4 | A version-1 `VideoFilter` whose source calls `applyCurve(...)` compiles and links; the current shader build rejects it because the symbol is undefined. | `[fact:ticket+code]` — ticket names the function; `gl_program.cpp:212-220` shows the preamble/suffix that would have to contain it |
| SC-2 | FR-2, FR-3 | With a curve LUT bound, playback is visibly tone-mapped relative to the same video with no curve. | `[fact:ticket]` |
| SC-3 | FR-3 | No new public Kotlin type and no `android.content.Context` / `AssetManager` dependency is added to `videolib`. | `[fact:clarification+code]` — confirmed `videolib/src/main/java` currently has zero `Context`/`assets` references |

No further success conditions are stated by the ticket. Everything in §5a is proposed, not
normative.

### 5a. Proposed edge cases & boundary behavior

| FR-ID | Edge / boundary case | Expected handling | Status | Source |
|---|---|---|---|---|
| FR-3 | Composite curve vs. per-channel curves — the sample has both non-identity. The three in-repo ACV readers disagree: `CurveTone.getCurveData()` sums composite + channel (`CurveTone.java:45-47`), `AssetReader.parseACV` **discards the composite entirely** (returns only R/G/B, `AssetReader.kt:88`). | Pick one composition rule and state it; the two give visibly different output for this file. | `[unknown]` | code — the ticket does not say |
| FR-3 | Channel packing — `CurveTone` emits **BGRA** ("BGRA for upload to texture", `CurveTone.java:44`); `AssetReader.generateACVLUT` emits **RGB**, 3 bytes/px (`AssetReader.kt:124-131`); `videolib` validation requires exactly `w*h*4` RGBA bytes (`VideoPreview.kt:465-472`). | Only an RGBA8888 `256x1` LUT passes `videolib` validation today; the other two conventions are rejected or channel-swapped. | `[assumption]` | code — inferred from the validator |
| FR-3 | Interpolation — `CurveTone` uses natural cubic spline (`createSecondDerivative`, `CurveTone.java:213`); `AssetReader.interpolateCurve` uses **linear** (`AssetReader.kt:92`). | Spline and linear give different midtones for the sample's 5-point curves. | `[unknown]` | code |
| FR-1 | Texture filtering — filter textures are created `GL_LINEAR` + `GL_CLAMP_TO_EDGE` (`gl_program.cpp:364-367`). A 256x1 LUT sampled with `GL_LINEAR` interpolates between adjacent entries. | Likely desirable (smooth), but it means the LUT is not sampled exactly; `GL_NEAREST` would need a per-texture option that does not exist. | `[assumption]` | code |
| FR-1 | Half-texel sampling — the `camera` body samples `texture(curve, vec2(color.r, 0.0))` (`frag_base_shader_adjust.glsl:151-156`). At `u=0` and `u=1` with `GL_LINEAR` on a 256-wide texture this reads the edge texels, slightly compressing the ends. | Accept the `camera` behavior, or offset by half a texel. | `[assumption]` | code |
| FR-4 | Reserved-name collision — `RESERVED_FILTER_SOURCE` blocks `#version`, `void main`, `u_texture`, `v_texCoord`, `fragColor` (`VideoPreview.kt:556-562`), and the compatibility regex at `gl_program.cpp:280` forces a consumer that redeclares a library component into providing the whole environment. `applyCurve` is in neither list today. | If `applyCurve` becomes a library component, decide whether a consumer redeclaring it triggers compatibility mode like the other components. | `[unknown]` | code |
| FR-2 | Curve with no texture bound — a source calling `applyCurve(c, u_filterTexture0)` with `textures = emptyList()` leaves `u_filterTexture0` undeclared, so the shader fails to compile. | Surfaces as `SHADER_COMPILATION` / `PROGRAM_LINK` rejection rather than a silent no-op. | `[assumption]` | code — `gl_program.cpp:326` only emits `u_filterTextureN` uniforms for supplied textures |
| FR-2 | `.acv` variants — the sample is version 4 with 5 curves; ACV also allows 2..19 points per curve (`CurveTone.java:67`) and files with fewer than 4 curves. `CurveTone.setFromCurveFileInputStream` indexes `curves.get(1..3)` unconditionally. | Consumer-side concern under FR-3, but worth stating if any parsing helper is offered. | `[assumption]` | code |
| FR-5 | Per-segment curves in a timeline export — `VideoExporter.exportTimeline` takes per-segment appearance including per-segment filter textures (`VideoExporter.kt:116-135`). | Each segment can carry its own curve; no new plumbing implied. | `[assumption]` | code |

## 6. Engineering Evidence — Non-normative

### Module impact hypothesis

| Module | Owner / consumer | Dependency evidence | Likely contract | Status / confidence |
|---|---|---|---|---|
| `videolib` | primary owner | GLSL preamble is built in `gl_program.cpp:150-220`; filter validation in `VideoPreview.kt:427`, mirrored in `VideoExporter.kt:245` | GLSL component contract (version-1 source environment); native-only change if FR-3 holds | `[fact:code]` high |
| `app` | direct consumer | `app/build.gradle.kts` project dependency; `MainActivity2.kt:878` constructs a `VideoFilter` | would own `.acv` parsing + LUT construction under FR-3 | `[fact:code]` high |
| `camera` | reuse source only | `CurveTone.java`, `CurveFilter.kt`, `AssetReader.kt`, `frag_base_shader_adjust.glsl:151` | **no dependency edge exists** between `camera` and `videolib`; registry confirms `videolib depends on: —` | `[fact:code]` high |
| external consumers | unknown | `videolib` contract is "public; external consumers unknown" per registry; the module does **not** apply the publication plugin (`videolib/build.gradle.kts` plugins block) | out-of-repo consumers cannot be enumerated | `[unknown]` |

`videolib` currently declares no dependency on `camera` and no `Context`/`AssetManager` usage.
Reusing `camera`'s ACV code inside `videolib` would create a new module edge; under FR-3 it does
not arise, because the parsing stays in `app`. `[fact:code]`

### Verification implications

| Module / consumer | Candidate command or check | Reason | Status |
|---|---|---|---|
| `videolib` | `:videolib:assembleDebug` | registry default; native + Kotlin compile | `[fact:registry]` |
| `videolib` | on-device shader compile of a filter calling `applyCurve` | GLSL is compiled at runtime, not build time — a malformed preamble cannot be caught by `assembleDebug` | `[assumption:code]` |
| `videolib` | both declared ABIs (`arm64-v8a`, `armeabi-v7a`, `videolib/build.gradle.kts:21`) | registry risk tag `abi`; native-boundary gate | `[fact:registry+code]` |
| `app` | `:app:assembleDebug` + manual preview/export comparison | only in-repo consumer; SC-2 and FR-5 are visual | `[assumption:code]` |

### Entry points

| Symbol | Role | File:line |
|---|---|---|
| `VideoFilter` | public filter contract; documents the version-1 component environment | `videolib/src/main/java/com/cii/videolib/VideoFilter.kt:40` |
| `VideoFilterTexture` | RGBA8888 auxiliary texture carrier | `videolib/src/main/java/com/cii/videolib/VideoFilter.kt:4` |
| `VideoPreview.setFilter` / `setAppearance` | public install path | `VideoPreview.kt:259`, `VideoPreview.kt:208` |
| `VideoPreview.validate` | filter/texture validation gate | `VideoPreview.kt:427` |
| `GlProgram::buildGeneration` | assembles preamble + consumer source, uploads filter textures | `videolib/src/main/cpp/gl_program.cpp:321` |
| `applyCurve` (existing, `camera`) | canonical body to reuse | `camera/src/main/assets/glsl/frag_base_shader_adjust.glsl:151` |
| `VideoExporter.export` / `exportTimeline` | export-side appearance path | `VideoExporter.kt:48`, `VideoExporter.kt:87` |

### Current behavior

| Behavior | Status | Evidence / source |
|---|---|---|
| `videolib` has no curve/LUT/ACV concept anywhere in Kotlin or C++. | `[fact:code]` | repo-wide grep for `acv|applyCurve|curveLut|toneCurve` returns only `camera` hits |
| A version-1 filter source is concatenated after a fixed GLSL preamble of colour components and before a fixed `main()` suffix. Consumer source cannot contain `#version`, `void main`, `u_texture`, `v_texCoord`, or `fragColor`. | `[fact:code]` | `gl_program.cpp:212-220`; `VideoPreview.kt:556-562` |
| Auxiliary textures are uploaded as `GL_RGBA8`, bound from `GL_TEXTURE1` upward, exposed as `u_filterTexture0..N`, with `GL_LINEAR` and `GL_CLAMP_TO_EDGE`. | `[fact:code]` | `gl_program.cpp:326`, `:348-375`, `:465-468` |
| Texture validation requires `width>0`, `height>0`, and exactly `width*height*4` bytes. A `256x1` RGBA8888 LUT (1024 bytes) satisfies it. | `[fact:code]` | `VideoPreview.kt:465-472`; mirrored `VideoExporter.kt:276-281` |
| Installing a *new* filter requires an attached surface; otherwise `SURFACE_UNAVAILABLE`. | `[fact:code]` | `VideoPreview.kt:211-217` |
| Export reuses the same `GlProgram`, so appearance+filter behavior is shared with preview. | `[fact:code]` | `offscreen_renderer.h:14`, `:56` |
| `GlProgram` methods are render-thread-only and require the EGL context current. | `[fact:code]` | `gl_program.h:1-2` |
| `camera` contains **three** ACV implementations with mutually inconsistent conventions (composite handling, channel order, interpolation). | `[fact:code]` | `CurveTone.java`, `CurveFilter.kt`, `AssetReader.kt` |
| `camera`'s `CLAUDE.md` describes `AssetReader.parseACV`/`interpolateCurve`/`CurveFilter` as the curve support; both files exist, so that description is current. | `[fact:code]` | verified by `find` |

### Affected boundaries

| Boundary | Why it matters | Status | Evidence / source |
|---|---|---|---|
| `videolib` version-1 GLSL component contract | `VideoFilter`'s KDoc enumerates the provided components and states that a source redeclaring any of them enters compatibility mode. Adding `applyCurve` changes that enumerated contract for every consumer. | `[fact:code]` | `VideoFilter.kt:27-39`; regex `gl_program.cpp:280` |
| Shader preamble ↔ compatibility regex | The provided-component list exists in two places that must agree: the C++ regex and the Kotlin KDoc. | `[fact:code]` | `gl_program.cpp:280`; `VideoFilter.kt:32-38` |
| Preview ↔ export duplication | Filter validation is written twice, in `VideoPreview.validate` and `VideoExporter.isAppearanceValid`. Any validation change must land in both. | `[fact:code]` | `VideoPreview.kt:427`; `VideoExporter.kt:245` |
| Native / ABI | `videolib` is an `android-native-library` built for `arm64-v8a` and `armeabi-v7a`; registry risk tags include `jni`, `abi`, `new-contract`. | `[fact:registry+code]` | `videolib/build.gradle.kts:20-22` |
| External consumers | `videolib` is registry-classified public with unknown external consumers, though it does not apply the publication plugin today. | `[unknown]` | registry; `videolib/build.gradle.kts` |
| Cross-repo asset | The sample `.acv` lives in `CameraVintage`, a different repository. It is not in this workspace. | `[fact:ticket]` | path resolution |

### Reuse candidates

| Candidate | Location | Apparent fit | Confidence |
|---|---|---|---|
| `applyCurve` GLSL body | `camera/src/main/assets/glsl/frag_base_shader_adjust.glsl:151-156` | direct — 6 lines, no `camera`-specific dependencies, matches the signature the ticket names | high |
| `AssetReader.parseACV` + `interpolateCurve` | `camera/.../utils/AssetReader.kt:62`, `:92` | consumer-side parsing under FR-3; **discards the composite curve** and emits 3-byte RGB | medium |
| `CurveTone` | `camera/.../utils/CurveTone.java` | consumer-side parsing; spline interpolation and composite summing, but emits **BGRA** | medium |
| `CurveFilter` | `camera/.../utils/CurveFilter.kt` | third variant of the same logic; parses via `BufferedReader` | low — reachable legacy duplication, do not propagate |
| existing `u_filterTextureN` plumbing | `gl_program.cpp:326`, `:348-375` | direct — a `256x1` LUT needs no new native texture path | high |

These are candidates only. No reuse, placement, or extension decision is made here. The three
`camera` ACV implementations are flagged as existing duplication; consolidating them is **not** in
this ticket's scope.

## 7. Non-functional / Technical Constraints

| Constraint | Status | Source |
|---|---|---|
| GLSL ES 3.0; all render work is render-thread-only with the EGL context current. | `[fact:code]` | `gl_program.h:1-2` |
| Filter source must define `vec4 addFilter(vec4, vec2)` and must not contain the reserved tokens. | `[fact:code]` | `VideoPreview.kt:553-562` |
| Auxiliary textures must be RGBA8888 with an exact `w*h*4` byte length. | `[fact:code]` | `VideoPreview.kt:465-472` |
| `videolib`: min SDK 21, compile SDK 36, JVM 11, NDK 29.0.14206865, ABIs `arm64-v8a` + `armeabi-v7a`. | `[fact:code]` | `videolib/build.gradle.kts` |
| Per project ground rules, the change belongs in the narrowest owning module and must not introduce new layers or a `camera → videolib` dependency. | `[fact:packet]` | stage packet §0 |
| A per-frame `applyCurve` costs three dependent texture fetches per pixel. No performance budget is stated. | `[unknown]` | — |

API contract constraints: N/A — focus area 5 does not apply.

## 8. Open Questions, Assumptions & Conflicts

| ID | Item | Classification | Owner | Consequence |
|---|---|---|---|---|
| A-1 | `videolib` provides `applyCurve` GLSL only; the consumer parses `.acv` and supplies the LUT. | resolved `[fact:clarification]` | user | scope is a native/GLSL change plus app-side parsing; no new public Kotlin type |
| Q-1 | Composite curve handling — sum composite into each channel (`CurveTone`) or discard it (`AssetReader`)? | **deferrable** | solution-design + product | different visible output for this exact file; consumer-side under A-1, so it does not block `videolib` scope |
| Q-2 | Spline vs. linear interpolation between control points. | **deferrable** | solution-design | midtone differences; consumer-side under A-1 |
| Q-3 | Does `applyCurve` join the compatibility-mode component list (`gl_program.cpp:280`) and the `VideoFilter` KDoc? | **deferrable** | solution-design | affects whether a consumer redeclaring `applyCurve` is forced to supply the full component environment |
| Q-4 | Is a `videolib` unit test possible for a GLSL-only change? | **non-material** | testing stage | GLSL compiles at runtime; verification is likely device-based |
| C-1 | **Conflict** — three in-repo ACV readers disagree on channel order (BGRA vs. RGB), composite handling (summed vs. discarded), and interpolation (spline vs. linear). Each is preserved independently; none is authoritative. | **deferrable** | solution-design | picking a source silently picks a visual result; only RGBA8888 passes `videolib` validation regardless |
| U-1 | Out-of-repo `videolib` consumers cannot be enumerated. | **non-material** | — | the change is additive to the GLSL preamble under A-1; it removes nothing |
| U-2 | The sample `.acv` is in `CameraVintage`, not this repo. No `.acv` exists in this workspace. | **deferrable** | implementation | a test fixture would need to be copied in or supplied |

No blocking gap remains. Q-1/Q-2/C-1 are deferrable because A-1 places `.acv` parsing outside
`videolib`, so they shape the consumer's LUT rather than this module's contract.

## 9. Risk Analysis

| Risk | Likelihood / impact | Affected FR / area | Introduced by change | Status | Source |
|---|---|---|---|---|---|
| Adding `applyCurve` to the shared GLSL preamble changes the enumerated version-1 component contract for **every** existing filter, including out-of-repo consumers. A consumer that already declares its own `applyCurve` would newly collide or be pushed into compatibility mode. | medium / high | FR-4, §6 boundaries | yes | `[fact:code]` | `VideoFilter.kt:27-39`; `gl_program.cpp:280` |
| The provided-component list lives in two places (C++ regex at `gl_program.cpp:280`, Kotlin KDoc at `VideoFilter.kt:32-38`). Updating one and not the other yields a silently wrong compatibility decision. | medium / medium | FR-4 | yes | `[fact:code]` | both sites |
| Filter validation is duplicated between `VideoPreview.validate` and `VideoExporter.isAppearanceValid`; a change applied to one only would make preview and export disagree, breaking FR-5. | low / high | FR-5 | uncertain | `[fact:code]` | `VideoPreview.kt:427`; `VideoExporter.kt:245` |
| Choosing a LUT convention from the wrong `camera` source yields a channel-swapped (BGRA) or composite-less image that still passes validation — a visual-only failure invisible to compilation. | medium / medium | SC-2, C-1 | yes | `[fact:code]` | `CurveTone.java:44-47` vs. `AssetReader.kt:124-131` |
| A GLSL preamble error is a **runtime** shader-compilation failure on device, not a build failure; `:videolib:assembleDebug` cannot catch it. | medium / medium | SC-1 | yes | `[assumption:code]` | `gl_program.cpp:222+` compiles at runtime |
| Native change in a module tagged `jni`, `abi`, `new-contract`; both declared ABIs need validation. | low / medium | §6 verification | yes | `[fact:registry]` | registry; `videolib/build.gradle.kts:20-22` |
| Reusing `camera` code by adding a `camera → videolib` dependency would violate the project's narrowest-owning-module rule and create a new module edge. Avoided under A-1, but a plausible mis-step. | low / high | §6 module impact | no (avoided) | `[fact:packet+code]` | stage packet §0; registry shows no such edge |
| Three per-pixel dependent texture fetches added to the fragment path, with no stated performance budget, on a min-SDK-21 device range. | low / low | §7 | yes | `[assumption:code]` | `frag_base_shader_adjust.glsl:151-156` |

---

**Validation**

```yaml
validation:
  status: PASS
  failures: []
  warnings:
    - "Q-1/Q-2/C-1 deferred to solution-design: consumer-side LUT convention is outside videolib scope under A-1."
    - "Sample .acv lives outside this repository; no .acv fixture exists in this workspace (U-2)."
    - "videolib external consumers are registry-declared unknown and cannot be enumerated (U-1)."
  coverage:
    requirements_with_evidence: "5/5"
    stories_mapped: "3/3"
    success_conditions_mapped: "3/3"
    material_unknowns_resolved: "1/1"
```

Next stage: **solution-design** (`/design`).
