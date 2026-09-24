AUTOMATION: CONTINUE

# DEV-SPEC — Implement preview adjustment and filters

## 0. Analysis Control

- Outcome: `CONTINUE` — final DEV-SPEC validation passed; no blocking product gap remains. `[fact:code]`
- Scope classification: deep, existing-code, single owning module with public Kotlin, JNI, native-threading, and EGL/GLES boundaries. `[fact:code]`
- Lookup depth: approximately 18 focused evidence sources within the deep cap of 22. The project knowledge graph had no registered repository, so focused symbol/text search was used and consequential findings were validated against source. `[fact:code]`
- Lookup ledger: ticket and five clarifications; `VideoPreview`; JNI exports; `VideoPlayback`; `PreviewRenderer`; `GlProgram`; `app` consumer/Gradle edge; `videolib` Gradle/CMake configuration; and the ticket-linked shader/reference API files. `[fact:ticket, clarification, code]`
- Context reconciliation: the compact packet's module-topology table reports `app` as a `videolib` consumer, while its project-context prose says the app depends only on `network`; current `app/build.gradle.kts:55` confirms the `videolib` edge. `[conflict:code]`

| Focus area | Applicability |
| --- | --- |
| Requirements | Applicable — the ticket and clarifications define preview adjustment/filter capabilities. `[fact:ticket, clarification]` |
| Edge cases | Applicable — live native rendering and caller-supplied shader/texture inputs expose boundary behavior. `[assumption:code]` |
| Feature impact | Applicable — existing `:videolib` public and native preview paths are affected. `[fact:code]` |
| Risk | Applicable — public API, JNI, thread ownership, and EGL/GLES resources are crossed. `[fact:code]` |
| API docs | N/A — no remote/backend/API documentation or network surface is supplied or reached. `[fact:ticket, code]` |
| Figma/design | N/A — no visual design source was supplied. `[fact:ticket]` |

## 1. Sources

| Source | Location | Revision/read status |
| --- | --- | --- |
| Feature ticket | `output/implement-adjust-filter.md:2` | Local Markdown; read. `[fact:ticket]` |
| Clarifications | Current `/study` conversation | Adjustments: all referenced controls; filters: super-app-created; auxiliary texture data: supported; values/defaults: preserve reference; updates: during active playback. `[fact:clarification]` |
| Current public preview API | `videolib/src/main/java/com/cii/videolib/VideoPreview.kt:25` | Local source; read. `[fact:code]` |
| Current GLES program | `videolib/src/main/cpp/gl_program.cpp:30` | Local source; read. `[fact:code]` |
| Current render/playback path | `videolib/src/main/cpp/preview_renderer.cpp:102`; `videolib/src/main/cpp/video_playback.cpp:650` | Local source; read. `[fact:code]` |
| Kotlin/JNI boundary | `videolib/src/main/java/com/cii/videolib/VideoPreview.kt:239`; `videolib/src/main/cpp/videolib.cpp:146` | Local source; read. `[fact:code]` |
| In-repository consumer edge | `app/build.gradle.kts:55`; `app/src/main/java/com/chiistudio/library/MainActivity2.kt:30` | Local source; read. `[fact:code]` |
| Referenced shader base | `../DistributationLibrary/videogl/src/main/assets/glsl/base_fragment_shade_top.glsl:41` | Ticket-linked local reference; read. `[fact:ticket, code]` |
| Referenced adjustment functions | `../DistributationLibrary/videogl/src/main/assets/glsl/frag_base_shader_adjust_v2.glsl:1` | Ticket-linked local reference; read. `[fact:ticket, code]` |
| Referenced blend functions | `../DistributationLibrary/videogl/src/main/assets/glsl/frag_base_shader_blend_v2.glsl:1` | Ticket-linked local reference; read. `[fact:ticket, code]` |
| Referenced adjustment defaults | `../DistributationLibrary/videogl/src/main/java/com/library/chiistudio/videogl/model/VideoConfigure.kt:14` | Ticket-linked implementation evidence; read. `[fact:clarification, code]` |
| Referenced consumer/filter API | `../DistributationLibrary/videogl/src/main/java/com/library/chiistudio/videogl/view/IAdjustVideo.kt:6`; `../DistributationLibrary/videogl/src/main/java/com/library/chiistudio/videogl/view/GLPreviewVideo.kt:200` | Ticket-linked implementation evidence; read. `[fact:ticket, code]` |

Design index: N/A — no design source was supplied. `[fact:ticket]`

Converted files: N/A — the supplied source was already Markdown. `[fact:ticket]`

## 2. Overview & Business Goal

Enable a super-app consuming `:videolib` to adjust the appearance of preview video and create, pass, and apply its own filters without depending on the referenced sibling implementation. Adjustments and filter changes must be usable during active playback. `[fact:ticket, clarification]`

The first delivery is limited to production changes owned by `:videolib`; changes to the top-level sample/super-app are outside this requested implementation scope. `[fact:ticket]`

## 3. Functional Requirements

| FR-ID | Requirement | Status | Evidence/source |
| --- | --- | --- | --- |
| FR-01 | `:videolib` shall apply the full referenced `IAdjustVideo` adjustment surface to preview video: brightness, exposure, contrast, saturation, lights, highlights, darks, clarity, shadows, temperature, hue, vignette, levels, and vibrance. | `[fact]` | Clarification (“all”); referenced interface at `IAdjustVideo.kt:6`. `[clarification, code]` |
| FR-02 | Adjustment values and neutral defaults shall preserve the numeric semantics/defaults of the referenced implementation. | `[fact]` | Clarification; `VideoConfigure.kt:14`. `[clarification, code]` |
| FR-03 | A super-app shall be able to create and pass a new filter into `:videolib` for application to preview video. | `[fact]` | Ticket `output/implement-adjust-filter.md:10`; clarification. `[ticket, clarification]` |
| FR-04 | Caller-defined filters shall support both shader source and auxiliary texture data, including LUT/overlay/curve-style texture inputs. | `[fact]` | Clarification (“both”); referenced texture uniforms at `base_fragment_shade_top.glsl:46` and reference filter ingestion at `GLPreviewVideo.kt:200`. `[clarification, code]` |
| FR-05 | Adjustment and filter changes shall apply while video is actively playing. | `[fact]` | Clarification. `[clarification]` |
| FR-06 | The initial production implementation shall be confined to `:videolib` and shall not depend on the sibling `DistributationLibrary/videogl` implementation or another feature module. | `[fact]` | Ticket `output/implement-adjust-filter.md:8` and `:11`. `[ticket]` |

## 4. Actors & User Stories

| Story-ID | FR-ID | Story |
| --- | --- | --- |
| Story-01 | FR-01, FR-02, FR-05 | As a super-app integrator, I can change any referenced preview adjustment during playback using familiar value semantics so the visible preview updates without restarting. `[fact:clarification]` |
| Story-02 | FR-03, FR-04, FR-05 | As a super-app integrator, I can construct a custom shader filter with any required auxiliary textures, pass it to `:videolib`, and have it affect active preview playback. `[fact:ticket, clarification]` |
| Story-03 | FR-06 | As a library consumer, I can use the capability through `:videolib` without taking a dependency on the referenced sibling video-GL library. `[fact:ticket]` |

## 5. Observable Success Conditions

| SC-ID | FR-ID | Explicit/clarified outcome | Design-Ref | Evidence/source |
| --- | --- | --- | --- | --- |
| SC-01 | FR-01, FR-05 | During active video playback, changing any adjustment named in FR-01 changes subsequent preview frames without requiring playback restart. `[fact]` | N/A | Clarification. `[clarification]` |
| SC-02 | FR-02 | At the referenced neutral/default values, adjustment behavior preserves the reference implementation's numeric semantics/defaults. `[fact]` | N/A | Clarification; `VideoConfigure.kt:14`. `[clarification, code]` |
| SC-03 | FR-03, FR-05 | During active playback, a filter created and passed by the super-app is applied to subsequent preview frames. `[fact]` | N/A | Ticket `output/implement-adjust-filter.md:10`; clarification. `[ticket, clarification]` |
| SC-04 | FR-04 | A caller-defined filter can consume its supplied shader source and auxiliary LUT/overlay/curve-style texture data in the preview result. `[fact]` | N/A | Clarification. `[clarification]` |
| SC-05 | FR-06 | The capability is delivered through `:videolib` without a runtime or build dependency on the referenced sibling implementation. `[fact]` | N/A | Ticket `output/implement-adjust-filter.md:8` and `:11`. `[ticket]` |

### 5a. Proposed edge cases & boundary behavior

| FR-ID | Edge/boundary case | Expected handling | Status | Source |
| --- | --- | --- | --- | --- |
| FR-01, FR-02 | An adjustment receives a non-finite or out-of-reference-range value. | The public contract should define deterministic validation or normalization and must not destabilize the native renderer. | `[unknown:deferrable]` | Reference ranges/defaults exist for most controls in `VideoConfigure.kt:14`, but invalid-input behavior is not specified. `[code]` |
| FR-03, FR-04 | Caller-provided shader source fails compilation/linking or refers to missing/incompatible uniforms. | The preview should remain in a defined state and expose a deterministic failure outcome to the caller. | `[assumption:deferrable]` | Current shader compilation can fail at `gl_program.cpp:50`; caller-defined shaders introduce that reachable condition. `[code]` |
| FR-04 | Texture counts, dimensions, formats, or data sizes do not match the filter declaration. | Reject or safely ignore invalid texture payloads without out-of-bounds access or corrupted GL state. | `[assumption:deferrable]` | The reference accepts texture buffers plus dimensions at `NativeRenderer.kt:46`; exact validation is not specified. `[code]` |
| FR-05 | A filter or adjustment changes concurrently with frame presentation, pause/seek, surface detach, or release. | The change should be serialized with the renderer lifecycle and must not present stale work after stop/release. | `[assumption:deferrable]` | Current playback serializes presentation with `rendererMutex_` at `video_playback.cpp:661` and owns GL work on the render executor at `preview_renderer.cpp:110`. `[code]` |
| FR-03, FR-04 | A new filter is installed before a surface exists, after release, or while EGL is being recreated. | The public contract should define whether the filter is retained for the next valid surface or rejected. | `[unknown:deferrable]` | Current surface lifecycle gates rendering in `VideoPreview.kt:44` and `PreviewRenderer::surfaceAvailable` at `preview_renderer.cpp:77`. `[code]` |
| FR-01, FR-03 | No custom adjustment/filter is supplied. | Preserve an unadjusted/pass-through preview at neutral defaults. | `[assumption:deferrable]` | Current shader is pass-through at `gl_program.cpp:40`; clarification preserves referenced neutral defaults. `[code, clarification]` |

## 6. Engineering Evidence — Non-normative

### Module impact hypothesis

| Module | Owner/consumer | Dependency evidence | Likely contract | Status/confidence |
| --- | --- | --- | --- | --- |
| `videolib` | Primary owner and only requested changed module | Public `VideoPreview` owns the current preview facade (`VideoPreview.kt:25`); the GLES program is owned under `videolib/src/main/cpp` (`gl_program.h:19`). | Public Kotlin API; Kotlin/JNI signatures; C++ ownership/threading; EGL/GLES shader and texture behavior; native ABI packaging. | `[fact:code]` / high |
| `app` | Direct in-repository consumer; verification-only under current scope | `implementation(project(":videolib"))` at `app/build.gradle.kts:55`; constructs `VideoPreview` at `MainActivity2.kt:34`. | Source/runtime compatibility and host integration. No production app change is authorized by FR-06. | `[fact:code, ticket]` / high |
| External super-apps | Out-of-repository consumers | `videolib` is a consumer-facing Android library and the ticket explicitly names a top-level/super-app consumer. | Public source/binary/runtime behavior; consumer set and versions are unavailable. | `[unknown:deferrable]` / medium |

Dependency closure: `videolib` → `app` through the Gradle project dependency is confirmed and can be affected at compile/runtime if the public preview API changes. `videolib` → external super-app consumers is required by product intent but cannot be enumerated in-repository. `[fact:ticket, code]`

Changed-module hypothesis: `videolib` only. `app` remains a verification consumer, not a requested production-change owner. `[fact:ticket, code]`

### Verification implications

| Module/consumer | Candidate command or device/manual check | Reason | Status |
| --- | --- | --- | --- |
| `videolib` | `./gradlew :videolib:assembleDebug` | Covers Kotlin/JNI/CMake compilation and both configured ABI packages. | `[fact:code]` |
| `videolib` | Relevant `:videolib:connectedDebugAndroidTest` subset on supported ARM device(s) | Rendering behavior, live updates, shader compilation, textures, EGL ownership, and native loading require a real GLES/device path. | `[assumption:code]` |
| `app` | `./gradlew :app:assembleDebug` | Confirms direct consumer source/package compatibility without requiring an app production change. | `[fact:code]` |
| Super-app/public contract | Consumer-level compile and preview smoke integration using a caller-defined shader plus auxiliary texture | External consumers are not inspectable in this repository; the ticket requires this contract. | `[unknown:deferrable]` |

### Entry points

| Symbol | Role | File:line |
| --- | --- | --- |
| `VideoPreview` | Public preview lifecycle/playback facade used by the app and intended super-app consumers. `[fact:code]` | `videolib/src/main/java/com/cii/videolib/VideoPreview.kt:25` |
| `GlProgram::drawFrame` | First meaningful GLES boundary: uploads the RGBA frame and draws it with the current program. `[fact:code]` | `videolib/src/main/cpp/gl_program.cpp:194` |

### Current behavior

| Behavior | Status | Evidence/source |
| --- | --- | --- |
| `VideoPreview` exposes playback, speed/seek/loop controls, direct RGBA frame input, and a test pattern, but exposes no adjustment or custom-filter API. | `[fact]` | `VideoPreview.kt:60`, `:114`, `:121`, `:128`, `:142`, `:151`. `[code]` |
| Decoded video frames are converted to RGBA and presented through the same `PreviewRenderer::pushFrame` path used by caller-supplied frames. | `[fact]` | `video_playback.cpp:650`, `:674`; `video_playback.cpp:113`. `[code]` |
| The current GLES fragment shader samples one 2D texture unchanged; it has no adjustment uniforms, auxiliary filter textures, or caller-supplied shader composition. | `[fact]` | `gl_program.cpp:40`; `gl_program.h:47`. `[code]` |
| All EGL/GLES work is marshalled onto a dedicated native render thread with the context current, while playback also serializes renderer access with `rendererMutex_`. | `[fact]` | `preview_renderer.h:4`; `preview_renderer.cpp:110`; `video_playback.cpp:661`. `[code]` |

### Affected boundaries

| Boundary | Why it matters | Status | Evidence/source |
| --- | --- | --- | --- |
| Public Kotlin library API | The super-app needs a new consumer-facing capability; external consumers are not visible in-repository. | `[fact]` | Ticket `:10`; `VideoPreview.kt:25`. `[ticket, code]` |
| Kotlin/JNI contract | Current external methods and exported native names are a strict 1:1 contract; any new native-facing operation must remain synchronized across both sides. | `[fact]` | `VideoPreview.kt:239`; `videolib.cpp:146`. `[code]` |
| Native ownership/threading | Live updates can race playback presentation and lifecycle unless they respect existing renderer serialization and executor affinity. | `[fact]` | `video_playback.cpp:661`; `preview_renderer.h:4`. `[code]` |
| EGL/GLES resource state | Shader programs, uniforms, and auxiliary textures require a current EGL context and bounded lifecycle cleanup. | `[fact]` | `gl_program.cpp:117`, `:229`; `preview_renderer.cpp:155`. `[code]` |
| ABI/build packaging | The shared library is built only for `arm64-v8a` and `armeabi-v7a`, with CMake/JNI and 16 KB-page constraints. | `[fact]` | `videolib/build.gradle.kts:18`; `CMakeLists.txt:28`. `[code]` |
| Direct app consumer | Public source/runtime behavior can affect the sample host even though it is not a requested production-change module. | `[fact]` | `app/build.gradle.kts:55`; `MainActivity2.kt:34`. `[code]` |

### Reuse candidates

| Candidate | Location | Apparent fit | Confidence |
| --- | --- | --- | --- |
| Referenced fragment shader base and adjustment/blend functions | Ticket-linked GLSL files under `../DistributationLibrary/videogl/src/main/assets/glsl/` | Canonical behavior reference for shader uniforms, adjustment math, and blend functions; cannot be a runtime dependency under FR-06. | `[fact:ticket, code]` / high |
| Referenced `ShaderBuilder` composition pattern | `../DistributationLibrary/videogl/src/main/java/com/library/chiistudio/videogl/utils/ShaderBuilder.kt:7` | Pattern candidate for composing base, blend, adjustment, caller filter, and main-function source; fit is behavioral evidence only, not a reuse decision. | `[fact:code]` / medium |
| Referenced `IAdjustVideo`/`IFilter` surface | `../DistributationLibrary/videogl/src/main/java/com/library/chiistudio/videogl/view/IAdjustVideo.kt:6` | Contract vocabulary and full adjustment inventory confirmed by clarification; exact new API shape remains for solution design. | `[fact:clarification, code]` / high |
| Current `GlProgram` | `videolib/src/main/cpp/gl_program.h:19` | Existing reusable GLES ownership and frame draw boundary, presently limited to a fixed pass-through program and one texture. | `[fact:code]` / high |

## 7. Non-functional / Technical Constraints

- Production code changes are owned by `:videolib` only for this first delivery; do not introduce a runtime/build dependency on the ticket-linked sibling library. `[fact:ticket]`
- Preserve `videolib`'s public-library compatibility obligations for unknown external consumers. `[fact:code]`
- Preserve exact Kotlin/JNI name/signature synchronization and native owner lifetime. `[fact:code]`
- Preserve render-thread/EGL-context affinity for shader program and texture creation, update, use, and destruction. `[fact:code]`
- Preserve active playback, pause/seek, stop/release, and surface lifecycle behavior while live appearance changes are applied. `[fact:clarification, code]`
- Preserve the configured `arm64-v8a` and `armeabi-v7a` packaging and 16 KB-page native linkage constraints. `[fact:code]`
- The exact public representation for shader source plus auxiliary texture payloads is a solution-design responsibility; it must satisfy FR-03/FR-04 without importing the sibling library's types. `[unknown:deferrable:ticket, clarification]`

API docs: N/A — no remote/backend API surface exists in the requirement or reached code. `[fact:ticket, code]`

## 8. Open Questions, Assumptions & Conflicts

| Item | Classification | Owner | Consequence |
| --- | --- | --- | --- |
| Exact public type/ownership/lifetime contract for shader source and auxiliary texture buffers | `[unknown:deferrable]` | Solution design / library owner | Must be decided before implementation planning; scope and observable outcome are already fixed by FR-03/FR-04. `[clarification, code]` |
| Invalid shader and invalid texture-payload behavior | `[unknown:deferrable]` | Product/library owner during solution design | Determines caller-visible failure reporting and renderer fallback, without changing the feature's required capability. `[code]` |
| Filter-versus-adjustment composition order | `[assumption:deferrable]` | Solution design / library owner | The reference applies the filter before adjustments (`ShaderBuilder.kt:20`); a different order changes pixels but was not explicitly confirmed. Preserve the reference order unless deliberately specified otherwise. `[code]` |
| Whether filter configuration survives surface recreation | `[unknown:deferrable]` | Product/library owner during solution design | Changes lifecycle persistence but not the confirmed ability to update active playback. `[code]` |
| Reference defaults omit explicit documented ranges for highlights, clarity, and vignette. | `[unknown:deferrable]` | Product/library owner during solution design | Neutral defaults can be preserved, but valid range/validation semantics for these controls require an explicit contract. `[code]` |
| The compact packet's module-topology table lists `app → videolib`, while its project-context prose says `app` depends only on `network`; current Gradle source confirms `app → videolib`. | `[conflict:non-material]` | Project context maintainer | No product clarification is required; use current Gradle/source for this analysis and refresh stale project-context prose separately. `[code]` |

## 9. Risk Analysis

| Risk | Likelihood/impact | Affected FR/area | Status | Source |
| --- | --- | --- | --- | --- |
| A caller-supplied shader can fail compile/link or violate expected symbols, leaving the preview unusable if program replacement is not atomic. | Medium / High | FR-03, FR-05; GLES program lifecycle | `[assumption]` | Current compile/link failure paths are visible at `gl_program.cpp:50` and `:75`; live replacement is newly required. `[code, clarification]` |
| Live configuration updates can race decode-frame presentation, surface teardown, or release across the playback worker and render executor. | Medium / High | FR-01, FR-03, FR-05; native threading/lifecycle | `[assumption]` | Renderer access is serialized at `video_playback.cpp:661`; GL work is executor-bound at `preview_renderer.cpp:110`. `[code]` |
| Auxiliary texture buffers can cause native memory-safety or GL-state faults when counts, sizes, formats, or lifetimes are inconsistent. | Medium / High | FR-04; JNI/native texture boundary | `[assumption]` | The referenced contract crosses `List<ByteBuffer>` and dimension lists at `NativeRenderer.kt:46`; the requested module currently has no equivalent validation. `[code, clarification]` |
| Porting the referenced shader text unchanged may not compile or behave identically because the current pipeline consumes a single RGBA texture while the reference base expects Y/U/V and multiple auxiliary samplers. | High / High | FR-01–FR-04; shader/input contract | `[fact]` | Current shader uses `u_texture` at `gl_program.cpp:40`; reference base declares `texY/texU/texV` and filter samplers at `base_fragment_shade_top.glsl:42`. `[code]` |
| A public API addition can create source/binary/runtime obligations for super-app consumers that cannot be enumerated in this repository. | Medium / High | FR-03, FR-04, FR-06; public library contract | `[fact]` | `VideoPreview` is public and the ticket names super-app consumption; external consumers are unavailable. `[ticket, code]` |
| Filter/adjust ordering or non-neutral defaults could alter pixels even when consumers expect pass-through preview. | Medium / Medium | FR-01, FR-02, FR-03; visual behavior | `[assumption]` | Current renderer is pass-through (`gl_program.cpp:40`); reference composition order is filter then adjustments (`ShaderBuilder.kt:20`). `[code]` |
| Native changes can pass compilation but fail on one supported ABI/device/GLES driver. | Medium / High | FR-01–FR-05; ABI/runtime | `[fact]` | `videolib` packages two ARM ABIs (`build.gradle.kts:18`) and executes EGL/GLES 3.0 natively (`preview_renderer.cpp:19`). `[code]` |
