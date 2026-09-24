AUTOMATION: CONTINUE

# IMPLEMENT-PLAN — Reuse video-filter component functions

## 1. Planning control

- Source design: `SOLUTION-DESIGN.md` with `AUTOMATION: CONTINUE`, inspected against the working tree at commit `1670c7ff7eaa87c86fbfea1d7856e5b3d5bb87fe`.
- Outcome: make the public version-1 `VideoFilter` environment supply the reusable RGB/HSL, `vec3` vibrance, hue-pulse, and selective-color declarations so the sample and external consumers can provide only their filter-specific `addFilter` body, while preserving existing full snippets and atomic appearance replacement.
- Primary owner: `videolib`; changed-module closure: `videolib` (public/native provider and contract coverage) → `app` (direct sample consumer). `benchmark` is not behaviorally reached and remains outside the change and verification closure.
- Crossed contracts: public version-1 GLSL source behavior, Kotlin-to-native source transport (shape unchanged), native GLES 3.0 runtime compilation, GL-context/thread ownership, packaged ARM ABIs, and the `app` project dependency on `videolib`.
- Assumptions: no Kotlin/JNI/CMake/Gradle signature or configuration change is required; the approved mutually exclusive provider rule applies to version 1 only; the existing `PlaybackSurfaceProbe` can be reused for supported-device render observation without adding production observability.
- Existing working-tree constraint: `videolib/src/main/cpp/gl_program.cpp` and `app/src/main/java/com/chiistudio/library/MainActivity2.kt` already contain uncommitted work. Their current inspected symbols are the task baseline; implementation must preserve unrelated edits and review the focused diff before each task completes.
- Blockers: none.
- Unresolved planning inputs: none. External consumers cannot be enumerated, so representative legacy-source runtime coverage is mandatory rather than inferred from in-repository callers.

### Bounded investigation ledger

| Lookup | Scope | Planning evidence |
| --- | --- | --- |
| 1 | Compact `implementation-plan` stage packet | Guarded continuation, module topology, native/public integration gate, and default verification closure. |
| 2 | `SOLUTION-DESIGN.md` | Approved provider-selection rule, exact behavior, compatibility boundary, and atomic recovery contract. |
| 3 | `videolib/src/main/cpp/gl_program.cpp` and `.h` | `kFragmentPrefix` owns existing built-ins; `GlProgram::buildGeneration` assembles and compiles the single translation unit; `applyAppearance` replaces a generation only after success. No header change is required. |
| 4 | `VideoFilter.kt` and `VideoPreview.kt` | Version-1 entry point and public validation/result model remain unchanged; source crosses JNI as one string. |
| 5 | `MainActivity2.kt` | `DEMO_VIDEO_FILTER` is the direct consumer and currently duplicates the reusable declarations before its exact `addFilter` calculation. |
| 6 | `videolib`/`app` Gradle and `videolib` CMake | Confirmed `app` → `videolib`, C++ source inclusion, GLESv3 linkage, and packaged `arm64-v8a`/`armeabi-v7a` closure without build-file changes. |
| 7 | Existing `videolib` Android tests and `PlaybackSurfaceProbe` | A real consumable Surface/EGL fixture exists; no current test covers filter component composition, compatibility selection, or failed-candidate retention. |
| 8 | Focused symbol/caller search and working-tree status | `GlProgram::buildGeneration` is the sole source composer; `MainActivity2.DEMO_VIDEO_FILTER` is the only in-repository non-test filter source; both planned production files already have local edits. |

## 2. Change-surface inventory

| Change-ID | existing/new | action | exact path | symbol/resource/config key | Design-Ref(s) | responsibility | evidence/design decision | shared/collision key |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| C-01 | existing | extend | `videolib/src/main/cpp/gl_program.cpp` | anonymous-namespace fragment component declarations (`kFragmentPrefix` plus implementation-local shared-component text/detection) | none — requirement/code-driven | Own the complete reusable version-1 component set and recognize whether the consumer already declares any newly shared component/global name. | D-2, D-3, D-5; existing built-ins are at lines 26–55. | `videolib-glsl-component-environment` |
| C-02 | existing | modify | `videolib/src/main/cpp/gl_program.cpp` | `GlProgram::buildGeneration` | none — requirement/code-driven | Select exactly one provider mode and assemble prefix, optional texture uniforms, selected component environment, consumer source, and suffix before the existing compile/link path. | D-3, D-6; sole composition point at lines 158–169. | `videolib-glsl-composer` |
| C-03 | existing | extend | `videolib/src/main/java/com/cii/videolib/VideoFilter.kt` | `VideoFilter` KDoc, version-1 source contract only | none — requirement/code-driven | Document that consumers may rely on the library-owned reusable component environment and that declaring any newly shared name selects consumer-owned compatibility mode; do not change class shape, constants, or validation. | D-3, D-4 and the public component-environment boundary. | `videolib-public-filter-contract` |
| C-04 | existing | modify | `app/src/main/java/com/chiistudio/library/MainActivity2.kt` | companion object `DEMO_VIDEO_FILTER` | none — requirement/code-driven | Remove all reusable declarations from the sample source and retain only the exact filter-specific `addFilter` body/order/arguments. | AC-1, AC-2; current duplicated declarations are at lines 500–652. | `app-demo-filter-source` |
| C-05 | new | create | `videolib/src/androidTest/java/com/cii/videolib/VideoFilterComponentsInstrumentedTest.kt` | `VideoFilterComponentsInstrumentedTest` | none — requirement/code-driven | Exercise short-source acceptance/rendering, representative pre-change full-source compatibility, and failed-candidate atomic recovery through the public API on a real Surface/EGL context. | AC-1 through AC-4; existing `PlaybackSurfaceProbe` is reusable test infrastructure. | `videolib-filter-device-tests` |
| C-06 | existing | verify only | `videolib/src/androidTest/java/com/cii/videolib/PlaybackSurfaceProbe.kt` | `PlaybackSurfaceProbe`, `PlaybackEventLog` | none — requirement/code-driven | Reuse without modification as the consumable Surface and presented-frame observation boundary. | Existing device fixture records frames produced by `eglSwapBuffers`. | `videolib-playback-probe` |

No generated files are in scope. `videolib/src/main/cpp/CMakeLists.txt`, both module Gradle files, Kotlin/JNI signatures, ABI filters, and packaged FFmpeg inputs are verified boundaries and must not be edited for this change.

## 3. Work-item backlog

| FR-ID | SC-ID | AC-ID | Work-ID/Story-ID | outcome | module/screen | depends on |
| --- | --- | --- | --- | --- | --- | --- |
| FR-1, FR-2 | SC-1, SC-2 | AC-1, AC-2, AC-3, AC-4 | Story-1 | A version-1 consumer supplies only its exact `addFilter` behavior, receives reusable library components without duplicate declarations, and retains existing atomic failure recovery. | `videolib` public/native filter contract; `app` `MainActivity2` sample | none |
| FR-1, FR-2 | SC-1, SC-2 | AC-1, AC-2, AC-3, AC-4 | Work-INT-1 | The producer, direct consumer, external compatibility representative, GLES runtime, and packaged ABI closure are verified at their appropriate proof levels. | `videolib` → `app`; supported ARM device(s) | Story-1 production and test-source tasks |

## 4. Task backlog

| Task-ID | Work-ID/Story-ID | owning module | owner stage | objective | Change-IDs/exact path-symbol scope | Design-Ref(s) | preconditions/inputs | invariants | done condition | verification/Test-ID/Check-ID | depends on | collision key |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| TASK-01 | Story-1 | `videolib` | android-dev | Extend the existing native shader environment and composer with the approved mutually exclusive provider selection. | C-01, C-02: `gl_program.cpp` anonymous-namespace fragment component declarations/detection and `GlProgram::buildGeneration` | none — requirement/code-driven | Approved D-2/D-3/D-5; current consumer helper formulas in C-04 are the behavioral source; existing generation path and render context remain in place. | Library mode provides `FILTER_HUE_RANGE`, `current_hue`, `hueToRgb`, `HSLtoRGB`, `RGBtoHSL`, the distinct `vec3 vibranceAdjust`, `cubicPulse`, and all six `mixColor*` functions with demonstrated formulas. Consumer mode is selected when the source declares any newly shared component/global name and receives no new bundle. Existing `vec4` built-ins remain unchanged. No hybrid-provider promise, JNI change, new runtime abstraction, or GL work outside the current context/thread. | Focused diff contains only provider/composition behavior; a composed short source has one provider for every reference and a representative full source has no duplicate new bundle. Linked TEST-01, TEST-02, TEST-03; CHECK-01, CHECK-03. | none | `videolib-glsl-component-environment`; `videolib-glsl-composer` |
| TASK-02 | Story-1 | `videolib` | android-dev | Update the public version-1 source documentation to describe the additive reusable environment and compatibility mode. | C-03: `VideoFilter.kt` KDoc on `VideoFilter` | none — requirement/code-driven | TASK-01 provider contract; approved D-3/D-4. | Preserve constructor, `VERSION_1`, opacity/textures semantics, required `addFilter` signature, and all Kotlin binary/source shapes; documentation must not promise mixed partial ownership. | KDoc matches the implemented two-mode contract and names the callable shared declarations/signatures without exposing implementation mechanics. Source-only inspection plus CHECK-01. | TASK-01 | `videolib-public-filter-contract` |
| TASK-03 | Story-1 | `app` | android-dev | Convert the direct sample to the public short-source contract. | C-04: `MainActivity2.kt` companion object `DEMO_VIDEO_FILTER` | none — requirement/code-driven | TASK-01 library provider; exact AC-2 calculation in the approved design/current source. | Shader source declares only `vec4 addFilter(vec4 color, vec2 uv)`; preserve the operation order, every numeric argument, `color.rgb` use of the `vec3` vibrance overload, HSL round trip, returned alpha, opacity, and existing `setFilter` rejection handling. Remove declarations, not calls. | Focused diff shows no reusable component/global declaration in `DEMO_VIDEO_FILTER`, while the exact `addFilter` behavior remains. Linked TEST-01 and TEST-04; CHECK-02, CHECK-04. | TASK-01 | `app-demo-filter-source` |
| TASK-04 | Story-1 | `videolib` | testing | Add public-boundary device coverage for both provider modes and atomic rejection recovery. | C-05 create `VideoFilterComponentsInstrumentedTest.kt`; C-06 reuse `PlaybackSurfaceProbe`/`PlaybackEventLog` without editing | none — requirement/code-driven | TASK-01 complete; existing public `VideoPreview`, `VideoFilter`, `AppearanceUpdateResult`, `requestPattern`, and probe fixture. | Use a supported real Surface/EGL path; exercise the exact short body, a representative pre-change full source declaring newly shared names, and a Kotlin-valid but runtime-invalid candidate after an accepted filter. Do not add production hooks or infer rendering from acceptance alone. | Test source asserts TEST-01/02/03 outcomes and always releases `VideoPreview`/probe resources. | TEST-01, TEST-02, TEST-03; CHECK-03. | TASK-01 | `videolib-filter-device-tests` |
| TASK-05 | Work-INT-1 | `videolib` | integration-testing | Compile/package the native provider and its Android test source for configured variants/ABIs. | C-01, C-02, C-03, C-05 (verification only) | none — requirement/code-driven | TASK-01, TASK-02, TASK-04 complete. | Compilation proves C++/Kotlin/linkage/package consistency only; it does not prove runtime GLSL validity. Do not edit CMake, Gradle, ABI filters, or FFmpeg inputs to make the check pass without returning to planning/design scope. | CHECK-01 commands pass and evidence is recorded; failures are attributed to touched scope before repair. | CHECK-01 | TASK-01, TASK-02, TASK-04 | `videolib-build-gate` |
| TASK-06 | Work-INT-1 | `app` | integration-testing | Compile the direct sample against the changed public/native library contract. | C-04 plus C-01/C-02 producer dependency (verification only) | none — requirement/code-driven | TASK-03 and TASK-05 complete. | Preserve the existing `implementation(project(":videolib"))` edge and make no benchmark, publication, signing, or distribution change. | CHECK-02 passes and records the `app` → `videolib` source/build closure. | CHECK-02 | TASK-03, TASK-05 | `app-videolib-build-gate` |
| TASK-07 | Work-INT-1 | `videolib` | integration-testing | Prove runtime shader acceptance, rendering, compatibility selection, and rejection recovery on supported packaged ABI hardware. | C-01, C-02, C-05, C-06 (verification only) | none — requirement/code-driven | TASK-04, TASK-05; connected supported GLES 3.0 device with one packaged ABI. | Run real EGL/GLES compilation; short and legacy filters must both present frames; invalid candidate must return the existing rejection category and leave the prior accepted generation usable. Record actual device/ABI. | TEST-01, TEST-02, and TEST-03 pass under CHECK-03; repeat on both packaged ABIs when hardware is available, otherwise explicitly record the uncovered ABI. | TEST-01, TEST-02, TEST-03; CHECK-03 | TASK-04, TASK-05 | `videolib-gles-device-gate` |
| TASK-08 | Work-INT-1 | `app` | integration-testing | Smoke the exact sample filter and visually confirm the requested calculation is active. | C-04 with C-01/C-02 provider (verification only) | none — requirement/code-driven | TASK-03, TASK-06; supported device and sample video/input available. | `DEMO_VIDEO_FILTER` must be accepted by existing attach handling and render without a blank/failed surface; inspection of C-01/C-04 must confirm exact formulas/order because visual observation alone cannot prove numeric equivalence. | CHECK-04 records filter acceptance, visible filtered output, source-order audit, and actual device/ABI. | TEST-04; CHECK-04 | TASK-03, TASK-06 | `app-video-filter-device-smoke` |

## 5. Dependency map (DAG)

| From | To | edge type | reason |
| --- | --- | --- | --- |
| TASK-01 | TASK-02 | contract | Public documentation must describe the implemented provider contract. |
| TASK-01 | TASK-03 | behavior | The short sample cannot compile at GLES runtime until the library owns the removed declarations. |
| TASK-01 | TASK-04 | test | Device tests target the provider selection/composition behavior. |
| TASK-01, TASK-02, TASK-04 | TASK-05 | test | Native/Kotlin/test compilation follows producer and test-source completion. |
| TASK-03, TASK-05 | TASK-06 | wiring | The direct sample build follows producer packaging and consumer simplification. |
| TASK-04, TASK-05 | TASK-07 | test | Runtime tests require authored coverage and a successfully assembled native test APK/library. |
| TASK-03, TASK-06 | TASK-08 | wiring | Sample smoke follows the exact consumer source and compiled producer-consumer closure. |
| TASK-01 | TASK-03 | ownership serialization | These files differ, but the consumer deletion is deliberately ordered after provider completion to avoid an intermediate uncompilable runtime state. |

Cycle check: **pass**. All edges flow from TASK-01 toward documentation/consumer/test work and then compile/device verification; no task depends on a successor. C-01 and C-02 are intentionally atomic in TASK-01 because they share `gl_program.cpp` and one translation-unit contract.

## 6. Execution waves

| Wave | Tasks | concurrency / serialization | prerequisites satisfied |
| --- | --- | --- | --- |
| 1 | TASK-01 | Serialized owner of `gl_program.cpp` and the provider/composer collision keys. | Approved design and current-source baseline. |
| 2 | TASK-02, TASK-03, TASK-04 | May run concurrently: ownership is disjoint (`VideoFilter.kt`, `MainActivity2.kt`, and the new Android-test file). Each consumes the completed provider contract. | TASK-01. |
| 3 | TASK-05 | Serialized producer compile/package gate after all `videolib` production, docs, and test-source edits. | TASK-01, TASK-02, TASK-04. |
| 4 | TASK-06, TASK-07 | May run concurrently when build infrastructure/device allocation permits: TASK-06 owns the app build gate; TASK-07 owns the `videolib` device test gate. | TASK-03/TASK-05 for TASK-06; TASK-04/TASK-05 for TASK-07. |
| 5 | TASK-08 | Serialized final sample behavior check so it observes the same compiled producer/consumer state recorded by prior gates. | TASK-03, TASK-06. |

## 7. Test scope and verification matrix

| Test-ID | AC-ID/risk | level | target component/contract | behavior/transition/error scope | fake/fixture boundary | production Task-ID | depends on | execution expectation |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| TEST-01 | AC-1, AC-2; missing-symbol/wrong-overload risk | Android instrumentation on supported GLES device | Public `VideoFilter` short source → `GlProgram::buildGeneration` → presented frame | The exact addFilter-only source is accepted, resolves all built-ins including the distinct `vec3` vibrance overload, and presents a frame through real GLES compilation/rendering. | Real `VideoPreview`, EGL context, and C-06 Surface probe; no fake composer/GLES. | TASK-01, TASK-03 | TASK-04 | Authored in testing; runnable only on a supported connected ARM GLES 3.0 device. |
| TEST-02 | AC-3; duplicate-symbol/external-consumer risk | Android instrumentation on supported GLES device | Version-1 compatibility provider selection | A representative pre-change full source declaring newly shared component/global names is accepted and presents a frame without receiving duplicate library declarations. | Real `VideoPreview`, EGL context, C-06 probe; source fixture represents an unknown external consumer. | TASK-01 | TASK-04 | Authored in testing; device/environment-dependent. |
| TEST-03 | AC-4; failed-candidate state-corruption risk | Android instrumentation on supported GLES device | `VideoPreview.setFilter` / native candidate generation replacement | After a valid short filter is active, a Kotlin-valid but GLSL-invalid changed source is rejected through the existing public result and the prior generation continues presenting frames; later valid use remains possible. | Real public API/EGL/probe; no native test hook. | TASK-01 | TASK-04, TEST-01 | Authored in testing; device/environment-dependent. |
| TEST-04 | AC-2; formula/order regression | source inspection plus app device smoke | C-01 shared formulas and C-04 `DEMO_VIDEO_FILTER` | Confirm the demonstrated formulas and exact operation order/arguments are unchanged, the sample declares only `addFilter`, and the accepted sample visibly alters/renders output. | Existing sample activity and supported-device video input; source audit is authoritative for numeric/order fidelity. | TASK-01, TASK-03 | TASK-06 | Source inspection is runnable without device; visual half is device/environment-dependent. |

### Module integration matrix

| Check-ID | changed module | affected consumer/external contract | boundary | command or device/manual check | why required | owner integration-testing task |
| --- | --- | --- | --- | --- | --- | --- |
| CHECK-01 | `videolib` | Public AAR/native ABI and Android-test source | Kotlin/C++/CMake/GLES linkage and configured ABI packaging | `./gradlew :videolib:assembleDebug :videolib:compileDebugAndroidTestKotlin` | Proves the native provider compiles/links/packages for configured ABIs and test source compiles; cannot prove GLSL runtime. | TASK-05 |
| CHECK-02 | `app` | Direct `app` consumer | Gradle project dependency and Kotlin source contract | `./gradlew :app:assembleDebug` | Proves the addFilter-only sample compiles against the unchanged public Kotlin API and packages the producer. | TASK-06 |
| CHECK-03 | `videolib` | Representative short and external legacy version-1 consumers | Public API → JNI/native composer → real EGL/GLES runtime | `./gradlew :videolib:connectedDebugAndroidTest -Pandroid.testInstrumentationRunnerArguments.class=com.cii.videolib.VideoFilterComponentsInstrumentedTest`; record `adb shell getprop ro.product.cpu.abi`; repeat on `arm64-v8a` and `armeabi-v7a` hardware when available | Runtime shader compile/link, provider collision behavior, rendering, and atomic generation retention are not established by assembly. | TASK-07 |
| CHECK-04 | `app` | Sample/user-visible integration | `MainActivity2.DEMO_VIDEO_FILTER` → `VideoPreview.setFilter` → displayed output | Inspect focused C-01/C-04 diff for formula/order fidelity, then launch `MainActivity2` on a supported device, attach/select a video, and record successful filter acceptance plus visible nonblank filtered output and device ABI. | Covers the exact direct-consumer snippet and visible calculation; visual inspection alone is paired with source audit. | TASK-08 |

## 8. Shared infrastructure and risk constraints

| Risk / constraint | affected Change/Task/AC IDs | required serialization or verification | owning task |
| --- | --- | --- | --- |
| Newly injected declarations collide with existing external full sources in one GLSL translation unit. | C-01, C-02; TASK-01, TASK-04, TASK-07; AC-3 | Keep provider modes mutually exclusive in the single composer task; run representative legacy-source TEST-02 through real GLES. | TASK-01 implementation; TASK-07 verification |
| The existing `vec4 vibranceAdjust` and demonstrated `vec3 vibranceAdjust` compile as overloads but have different behavior. | C-01, C-04; TASK-01, TASK-03; AC-2 | Preserve both exact signatures/formulas; source-audit overload use and execute TEST-01/TEST-04. | TASK-01 and TASK-03; TASK-08 verification |
| Build success can hide shader compile/link failures. | C-01, C-02; TASK-05, TASK-07; AC-1, AC-3 | Do not close on CHECK-01 alone; CHECK-03 is the runtime integration gate on supported GLES hardware. | TASK-07 |
| A rejected candidate could replace the working generation. | C-02, C-05; TASK-01, TASK-04; AC-4 | Preserve candidate-first replacement and verify rejection plus continued frame presentation in TEST-03. | TASK-01 implementation; TASK-07 verification |
| Public version-1 behavior has unknown external consumers. | C-01 through C-03; TASK-01, TASK-02, TASK-07; AC-1, AC-3 | Preserve Kotlin/JNI/binary shapes, document the two-mode source contract, retain rollback at the component-environment extension, and record legacy runtime evidence. | TASK-02 contract; TASK-07 verification |
| `gl_program.cpp` and `MainActivity2.kt` are already modified in the working tree. | C-01, C-02, C-04; TASK-01, TASK-03 | Serialize by collision key, inspect focused diffs, and preserve unrelated user work; no reset or broad reformat. | TASK-01 and TASK-03 |
| GLES resources and compilation require the renderer's current EGL context/thread. | C-02; TASK-01, TASK-07; AC-4 | Keep composition/compile in the existing `buildGeneration` call path; no new JNI or off-thread GL path; validate on-device. | TASK-01 implementation; TASK-07 verification |

Handoff status: ready for `android-dev`. The DAG is acyclic, every production touchpoint and test boundary is concrete, and no design choice is deferred to implementation.
