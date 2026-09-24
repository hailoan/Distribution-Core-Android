AUTOMATION: CONTINUE

# DEV-SPEC — Reuse video-filter component functions

## 0. Analysis Control

- Outcome: `CONTINUE` — [fact; source: ticket, code] the requested consumer behavior is explicit and the current shader-composition boundary is confirmed.
- Scope: existing-code, cross-module, public-contract, native/GLES boundary — [fact; source: code] `app` supplies a public `VideoFilter` source string to `videolib`, which composes and compiles that string in native GLES code.
- Evidence depth: deep cap (22) selected because the packet identifies `videolib` as a public native library with unknown external consumers; approximately 12 focused lookups were used, with no escalation — [fact; source: code, stage packet].
- Lookup ledger: user request; current `MainActivity2.DEMO_VIDEO_FILTER`; `VideoFilter` contract; `VideoPreview.setFilter` and validation/JNI declaration; native appearance validation; `GlProgram` shader prefix/composition; CMake ownership; `app` Gradle dependency; focused caller search; working-tree diff — [fact; source: ticket, code].
- Requirements: applicable — [fact; source: ticket].
- Edge cases: applicable — [assumption; source: code] shader symbol availability and collisions can change runtime compilation behavior.
- Feature impact: applicable — [fact; source: code] existing `app` and `videolib` surfaces are involved.
- Risk: applicable — [fact; source: stage packet, code] the behavior crosses a public Kotlin-to-native GLES contract.
- API docs: N/A — no remote, backend, network, authentication, or sync API is involved — [fact; source: ticket, code].
- Figma/design: N/A — no design source was supplied and the request has no visual-layout scope — [fact; source: ticket].

## 1. Sources

- User request in this conversation, 2026-09-10 — [fact; source: ticket].
- `app/src/main/java/com/chiistudio/library/MainActivity2.kt:498-674` — current demo filter and its component functions — [fact; source: code].
- `videolib/src/main/java/com/cii/videolib/VideoFilter.kt:27-37` — public version-1 shader-source contract — [fact; source: code].
- `videolib/src/main/java/com/cii/videolib/VideoPreview.kt:181-183,322-340,374-392,407-416` — public entry point, validation, and JNI boundary — [fact; source: code].
- `videolib/src/main/cpp/gl_program.cpp:26-64,158-169` — built-in shader functions and final-source composition — [fact; source: code].
- `videolib/src/main/cpp/video_playback.cpp:95-123` — native filter validation — [fact; source: code].
- `videolib/src/main/cpp/CMakeLists.txt:28-37` and `app/build.gradle.kts:55` — native ownership and direct consumer dependency — [fact; source: code].
- Design source: none — [fact; source: ticket].
- API docs: none — [fact; source: ticket].
- Converted files: none — [fact; source: ticket].

## 2. Overview & Business Goal

The library consumer should be able to supply the shown `vec4 addFilter(vec4 color, vec2 uv)` filter definition as the filter source without also copying the component-function implementations and supporting declarations above it. This removes consumer-side duplication while retaining the shown filter operations and parameter values — [fact; source: ticket].

## 3. Functional Requirements

| FR-ID | Requirement | Status | Evidence/source |
| --- | --- | --- | --- |
| FR-1 | A consumer applying the shown filter shall pass a filter source containing only the shown `addFilter(vec4 color, vec2 uv)` definition; the component functions and supporting declarations referenced by that body shall not need to be declared by the consumer at top level. | fact | ticket |
| FR-2 | The consumer-only `addFilter` body shall retain the exact operation sequence and parameter values shown in the request, including exposure, contrast, shadows, saturation, vibrance, hue, temperature, RGB/HSL conversion, and red/orange/yellow/green/cyan/blue adjustments. | fact | ticket |

## 4. Actors & User Stories

| Story-ID | FR-ID | Story |
| --- | --- | --- |
| Story-1 | FR-1, FR-2 | As a `VideoFilter` consumer, I want to provide only the filter-specific `addFilter` function so that I do not have to copy the reusable component implementations into each filter source. — [fact; source: ticket] |

## 5. Observable Success Conditions

| SC-ID | FR-ID | Explicit/clarified outcome | Design-Ref | Evidence/source |
| --- | --- | --- | --- | --- |
| SC-1 | FR-1 | Supplying only the `addFilter` definition shown in the request is accepted as the filter source without requiring the preceding helper functions, constants, or global declarations in the app-provided source. | N/A | fact; ticket |
| SC-2 | FR-2 | The shortened filter source performs the same listed operations in the same order with the same numeric values as the supplied `addFilter` body. | N/A | fact; ticket |

### 5a. Proposed edge cases & boundary behavior

| FR-ID | Edge/boundary case | Expected handling | Status | Source |
| --- | --- | --- | --- | --- |
| FR-1 | A consumer source declares a function or global with the same signature/name as a library-provided shader component. | Compatibility behavior is not specified by the request; solution design must account for existing version-1 filter sources that may define their own helpers. | unknown; deferrable | code: the prefix and consumer source share one GLSL translation unit |
| FR-1 | The shortened source is checked by Kotlin/native textual validation before GLES compilation. | The required `addFilter` signature remains recognizable and the source remains free of currently reserved tokens. | assumption; deferrable | code: `VideoPreview.kt:329-332,407-416`; `video_playback.cpp:103-110` |
| FR-2 | The shortened shader compiles on one supported ABI/device GPU but not another GLES 3.0 implementation. | The same shortened filter should be exercised on supported ABI/device coverage because compilation occurs at runtime on the device. | assumption; deferrable | code: `gl_program.cpp:66-101`; stage packet native/ABI risk |

## 6. Engineering Evidence — Non-normative

### Module impact hypothesis

| Module | Owner/consumer | Dependency evidence | Likely contract | Status/confidence |
| --- | --- | --- | --- | --- |
| `videolib` | primary owner | `VideoFilter` is declared in `videolib`; `GlProgram` composes and compiles its source | Public shader-source behavior plus native GLES runtime behavior | fact; high; code |
| `app` | direct in-repository consumer/sample | `app/build.gradle.kts:55`; `MainActivity2.kt:138,498-674` | Demo source supplied through `VideoPreview.setFilter` | fact; high; code |
| External `videolib` consumers | possible consumers | module registry marks the library public and external consumers unknown | Source/runtime compatibility of version-1 shader snippets | unknown; deferrable; stage packet |

Dependency closure: `videolib` producer → `app` direct consumer through `implementation(project(":videolib"))`; only these in-repository modules are implicated by focused source/Gradle evidence — [fact; source: code]. External consumer exposure remains unknown because absence of repository callers does not prove absence of published consumers — [unknown; source: stage packet, code].

### Verification implications

| Module/consumer | Candidate command or device/manual check | Reason | Status |
| --- | --- | --- | --- |
| `videolib` | `./gradlew :videolib:assembleDebug` | Compile and native-link the owning Android library for its supported build configuration. | fact; source: stage packet, code |
| `app` | `./gradlew :app:assembleDebug` | Compile the direct consumer using the shortened filter source. | fact; source: stage packet, code |
| `app` + `videolib` native renderer | Supported-device filter smoke check on `arm64-v8a` and `armeabi-v7a` where available | GLES shader acceptance/linking and rendered behavior occur at runtime and are not proven by APK/AAR assembly. | assumption; source: code, `videolib/build.gradle.kts` |

### Entry points

| Symbol | Role | File:line |
| --- | --- | --- |
| `MainActivity2.surfaceCreated` / `DEMO_VIDEO_FILTER` | Direct sample consumer that installs the filter and currently embeds all helper definitions. — [fact; source: code] | `app/src/main/java/com/chiistudio/library/MainActivity2.kt:135`, `:498` |
| `VideoPreview.setFilter` | Public library entry point accepting `VideoFilter`. — [fact; source: code] | `videolib/src/main/java/com/cii/videolib/VideoPreview.kt:181` |
| `GlProgram::buildGeneration` | Native boundary that concatenates library prefix, consumer source, and library suffix before GLES compilation. — [fact; source: code] | `videolib/src/main/cpp/gl_program.cpp:158` |

### Current behavior

| Behavior | Status | Evidence/source |
| --- | --- | --- |
| `VideoFilter.VERSION_1` requires an app-supplied `addFilter` function and permits other non-reserved GLSL declarations in the same source string. | fact | code: `VideoFilter.kt:27-37`; `VideoPreview.kt:322-340` |
| The library prefix currently provides `vec4` adjustment functions such as exposure, contrast, saturation, vibrance, hue, temperature, and shadows. | fact | code: `gl_program.cpp:26-55` |
| The demo source separately declares RGB/HSL conversion, a different `vec3 vibranceAdjust` overload, pulse/color-mix helpers, and shared globals before `addFilter`. | fact | code: `MainActivity2.kt:498-673` |
| A source containing only the requested `addFilter` body is currently incomplete: it references RGB/HSL/color-mix components and `current_hue` not declared by the current library prefix, and its `vec3` vibrance call does not match the prefix's `vec4` overload. | fact | code: `MainActivity2.kt:549-671`; `gl_program.cpp:45` |

### Affected boundaries

| Boundary | Why it matters | Status | Evidence/source |
| --- | --- | --- | --- |
| Public Kotlin `VideoFilter.source` contract | Consumers provide trusted GLSL text under version 1; built-in symbol availability changes the effective public contract. | fact | code: `VideoFilter.kt:27-37` |
| Kotlin/JNI/native appearance path | The source crosses JNI and is validated on both Kotlin and native sides before rendering. | fact | code: `VideoPreview.kt:383-392`; `video_playback.cpp:95-123` |
| GLES shader composition/runtime compilation | Library and consumer declarations occupy one final shader source, and acceptance ultimately depends on runtime compile/link. | fact | code: `gl_program.cpp:66-124,158-169` |
| ABI packaging | `videolib` packages native code only for `arm64-v8a` and `armeabi-v7a`. | fact | code: `videolib/build.gradle.kts` |

### Reuse candidates

| Candidate | Location | Apparent fit | Confidence |
| --- | --- | --- | --- |
| Existing built-in adjustment functions and shader prefix | `videolib/src/main/cpp/gl_program.cpp:26-55` | Reusable implementation already visible to every composed `addFilter`; it establishes the current component-function pattern. | fact; high; code |
| Demo RGB/HSL, vibrance, pulse, and selective-color component implementations | `app/src/main/java/com/chiistudio/library/MainActivity2.kt:500-652` | These are the concrete duplicated components the user wants consumers not to declare; their compatibility/placement decision is deferred. | fact; high; ticket, code |

## 7. Non-functional / Technical Constraints

- The filter contract remains GLES 3.0 and continues to require `vec4 addFilter(vec4 inputColor, vec2 uv)` for version 1 — [fact; source: code: `VideoFilter.kt:27-37`, `gl_program.cpp:26`].
- The final source is a single GLSL translation unit formed from the library prefix, optional auxiliary-texture uniforms, consumer filter source, and library suffix — [fact; source: code: `gl_program.cpp:158-166`].
- The existing public `VideoFilter` version, source validation, auxiliary texture naming, opacity behavior, and rejection result types are surrounding contracts — [fact; source: code].
- Those surrounding contracts are outside the requested behavior change — [assumption; non-material; source: ticket].
- Shader compile/link diagnostics are surfaced only as generic appearance failures, so device rendering evidence is required to distinguish source compatibility from build success — [fact; source: code: `gl_program.cpp:66-124`; `VideoPreview.kt:364-368`].
- API docs: N/A — no remote/backend contract exists in this scope — [fact; source: ticket, code].

## 8. Open Questions, Assumptions & Conflicts

| Item | Classification | Owner | Consequence |
| --- | --- | --- | --- |
| Existing external version-1 filter snippets may declare names that overlap the requested reusable components. — [unknown; source: stage packet, code] | deferrable | Solution design/review | Compatibility and versioning implications must remain visible when the component surface is defined. |
| The user intends “only pass `addFilter`” to remove every supporting declaration shown above the function, including constants/global state, while retaining calls to the named components inside `addFilter`. — [assumption; source: ticket] | non-material | Feature analysis | This reading matches the supplied desired source and does not change the requested observable outcome. |
| No conflict was found between the user request and current source; current source instead demonstrates the duplication the request seeks to remove. — [fact; source: ticket, code] | non-material | Feature analysis | No clarification is required. |

## 9. Risk Analysis

| Risk | Likelihood/impact | Affected FR/area | Status | Source |
| --- | --- | --- | --- | --- |
| Adding shared GLSL names to the same translation unit can collide with helper definitions in existing consumer snippets and cause runtime shader compilation failure. | likelihood unknown / high impact for affected filters | FR-1; public filter contract | unknown; introduced by change uncertain; needs design decision | code: `gl_program.cpp:158-169`; external consumers unknown in stage packet |
| The requested `vec3 vibranceAdjust` behavior differs in signature and formula from the existing built-in `vec4 vibranceAdjust`; treating them as interchangeable could change the requested filter output. | medium likelihood / medium impact | FR-2; rendered appearance | fact; introduced by change uncertain; needs design decision | code: `MainActivity2.kt:549-557`; `gl_program.cpp:45` |
| Assembly can pass while the final composed shader fails or renders differently on a device GPU because GLES compilation/linking is runtime behavior. | medium likelihood / high impact | FR-1, FR-2; native/GLES boundary | assumption; introduced by change no | code: `gl_program.cpp:66-124,158-169` |
| Simplifying only the sample source without making the components available at the library boundary would leave external consumers unable to use the requested short form. | medium likelihood / high impact | FR-1; owner/consumer boundary | assumption; introduced by change yes | ticket; code: `app` depends on public `videolib` |
