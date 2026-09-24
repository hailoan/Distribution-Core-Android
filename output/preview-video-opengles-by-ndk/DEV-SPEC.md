AUTOMATION: CONTINUE

# DEV-SPEC — preview-video-opengles-by-ndk

## 0. Analysis Control

- **Outcome:** CONTINUE — validation PASS, no blocking gap. One material ambiguity (preview frame-content
  source) is classified **deferrable** and left to solution-design; best-effort assumptions cover understanding.
- **Kind:** feature (ticket-reading → `kind: feature`, `route: none`).
- **Scope classification:** existing-code, single-module, native-boundary (JNI/CMake/ABI), public-library contract.
  Owning module: `:videolib`. No cross-module code change intended.
- **Depth:** standard (~14 focused lookups; graph unavailable — used source inspection). Entry points and current
  behavior confirmed on both contract sides; stopped at first meaningful native/build boundary.
- **Reference note:** the ticket points at `…/DistributationLibrary/videogl/src/main/cpp`, which is **outside this
  repository**. It is a pattern-donor scaffold, not an in-repo reusable module; treat it as copy/adapt evidence only.
- **Registry note:** loader failed on a stale `app → network` edge; corrected `.aidlc/modules.json` to the
  source-verified `app → videolib` edge (`app/build.gradle.kts:55`, `implementation(project(":videolib"))`). Context
  prose in the packet still says "app depends only on `:network`" — that is stale; source is authoritative.

### Focus-area applicability

| # | Focus area | Applies | Where |
|---|---|---|---|
| 1 | Requirements | yes | §2–§5 |
| 2 | Edge cases | yes (best-effort; ticket is thin) | §5 + §5a |
| 3 | Feature impact | yes (touches existing `:videolib` native/build surface) | §6 |
| 4 | Risk | yes | §9 |
| 5 | API docs | **N/A — no remote/backend/network surface; local NDK rendering only** | — |
| 6 | Figma/design | **N/A — no design source supplied** | — |

### Lookup ledger

| # | Evidence | Purpose |
|---|---|---|
| 1 | `videolib/output/preview-video-opengles-by-ndk.md` | ticket text |
| 2 | `.aidlc/modules.json`, `app/build.gradle.kts` | ownership + dependency edge (registry fix) |
| 3 | `videolib/build.gradle.kts`, `videolib/src/main/cpp/CMakeLists.txt`, `videolib/src/main/cpp/videolib.cpp` | current native/build contract |
| 4 | `videolib/src/main/java/com/cii/videolib/NativeLib.kt`, `videolib/src/main/AndroidManifest.xml` | current Kotlin/JNI surface + empty manifest |
| 5 | Reference `egl_renderer.cpp/.h`, `gl_unit.cpp`, `video_gl.cpp`, `texture_loader.cpp`, `single_thread_executor.h`, `native_video_player.cpp`, reference `CMakeLists.txt` | preview-pipeline pattern shape |
| 6 | `app/.../MainActivity2.kt` | current consumer of `NativeLib` |

## 1. Sources

- **Ticket:** `videolib/output/preview-video-opengles-by-ndk.md` (in-repo Markdown; read directly; no conversion needed).
- **Referenced code (external, read-only):** `/Users/loannth20/AndroidStudioProjects/DistributationLibrary/videogl/src/main/cpp` — donor scaffold for the EGL/GLES preview pipeline.
- **API docs:** none (N/A).
- **Design source:** none (N/A). No Figma/`.tsx`/`.png`/`data.json` supplied.

## 2. Overview & Business Goal

Give `:videolib` a working **OpenGL ES preview rendering path** that draws frames onto an Android `Surface` using the
NDK (EGL + GLES via JNI/CMake), advancing the module from its current "Hello-from-C++ + FFmpeg-linkage-proof" scaffold
toward a usable video library. This ticket delivers **only the OpenGL ES preview source path**; acquiring frames from a
video decoder or the camera is explicitly out of scope. [fact: ticket]

## 3. Functional Requirements

| FR-ID | Requirement | Status | Evidence/Source |
|---|---|---|---|
| FR-1 | Implement an OpenGL ES preview that renders a frame onto an Android `Surface` using the Android NDK. | fact | ticket ("implement preview frame using opengles preview on surface of android ndk") |
| FR-2 | Confine all changes to module `:videolib`. | fact | ticket ("Scope code: just update into module `:videolib`") |
| FR-3 | Exclude frame acquisition from video or camera; only the OpenGL ES preview rendering path is in scope. | fact | ticket ("by pass frame was from video or camera, this feature only include source opengles preview") |

## 4. Actors & User Stories

| Story-ID | FR-ID | Story |
|---|---|---|
| ST-1 | FR-1, FR-2 | As a consumer of the `:videolib` library, I can have a frame rendered onto a provided Android `Surface` through an OpenGL ES (NDK) preview path, so the module can display GPU-rendered video frames. |

Actor: **library consumer / host app** (e.g., `app`, which already instantiates `com.cii.videolib.NativeLib`,
`MainActivity2.kt:21`). The library ships the native rendering; the host owns the `Surface`/view lifecycle and any UI.

## 5. Observable Success Conditions

| SC-ID | FR-ID | Explicit/clarified outcome | Evidence/Source |
|---|---|---|---|
| SC-1 | FR-1, FR-2 | An OpenGL ES–rendered frame is visible on the Android `Surface` supplied to `:videolib`. | fact: ticket (terse — the only explicit outcome) |
| SC-2 | FR-3 | The preview path produces output without depending on a video-decode or camera capture source. | fact: ticket |

> The ticket states no acceptance criteria, pixel format, frame rate, shader behavior, or teardown expectations. Those
> are **not** invented here; candidate behaviors are listed in §5a as labelled assumptions for solution-design.

### 5a. Proposed edge cases & boundary behavior (best-effort — non-normative)

| FR-ID | Edge/boundary case | Expected handling (proposed) | Status | Source |
|---|---|---|---|---|
| FR-1 | `Surface` destroyed / recreated (rotation, background) mid-preview | EGL surface + context torn down and `ANativeWindow` released in order; re-init on next valid surface | [assumption] | code: reference `cleanup()` releases context/surface/display and `ANativeWindow_release` (`egl_renderer.cpp:130-148`); current `:videolib` has no lifecycle path |
| FR-1 | Null / invalid native window from `Surface` | Guard and abort init cleanly (no crash) | [assumption] | code: reference guards `if (!egl->nativeWindow)` (`egl_renderer.cpp:21-24`) |
| FR-1 | Shader compile / program link failure | Fail safe, log, do not render | [assumption] | code: reference `createShader/createProgram` log + return 0 (`gl_unit.cpp:19-93`) |
| FR-1 | Render calls arrive off the EGL thread | All GL work serialized onto one render thread with the context current | [assumption] | code: reference `SingleThreadExecutor` + `runInThread` around every GL op (`egl_renderer.cpp:96-128`) |
| FR-3 | What content is drawn when there is no video/camera feed | A self-contained/synthetic frame source (e.g., test texture/pattern or caller-supplied pixel buffer) drives the preview | [assumption] | ticket excludes external sources; content mechanism unspecified — see §8 OQ-1 |
| FR-1 | Non-supported ABI (x86/x86_64) | Not delivered; only `arm64-v8a`/`armeabi-v7a` built | [assumption] | code: `build.gradle.kts` `abiFilters += ["arm64-v8a","armeabi-v7a"]`; CMake links per-ABI FFmpeg archives |

## 6. Engineering Evidence — Non-normative

### Module impact hypothesis

| Module | Owner/consumer | Dependency evidence | Likely contract | Status/confidence |
|---|---|---|---|---|
| `:videolib` | primary owner | ticket names it; native stub lives here (`videolib/src/main/cpp`) | new native rendering source(s), new JNI-exported names, new/extended Kotlin API, CMake link additions (`EGL`, `GLESv3`) | fact / high |
| `:app` | direct consumer | `app/build.gradle.kts:55` `project(":videolib")`; `MainActivity2.kt:9,21` uses `NativeLib` | consumes new preview API (additive); no change required by this ticket | fact / high |
| external consumers | unknown | `:videolib` is `publicContract: true`, `externalConsumers: unknown` (registry) | additive public API; keep existing `NativeLib` names stable | unknown / medium |

### Verification implications

| Module/consumer | Candidate command or device/manual check | Reason | Status |
|---|---|---|---|
| `:videolib` | `:videolib:assembleDebug` | native compile + per-ABI link of new EGL/GLES + existing FFmpeg archives; 16 KB alignment | candidate |
| `:videolib` | On-device preview visual check on both `arm64-v8a` and `armeabi-v7a` | GL rendering onto a real `Surface` cannot be asserted by JVM unit tests | candidate (device-required) |
| `:app` | `:app:assembleDebug` | confirm consumer still builds against the additive public API | candidate |

### Entry points (smallest confirmed set)

| Symbol | Role | File:line |
|---|---|---|
| `NativeLib` (`stringFromJNI`, `nativeFFmpegVersion`) | current Kotlin/JNI facade of `:videolib` (to be extended, not broken) | `videolib/src/main/java/com/cii/videolib/NativeLib.kt:3-23` |
| `Java_com_cii_videolib_NativeLib_*` | current exported JNI names | `videolib/src/main/cpp/videolib.cpp:8-45` |
| CMake `add_library(videolib SHARED …)` + `target_link_libraries(… android log)` | native build/link contract (no `EGL`/`GLESv3` yet) | `videolib/src/main/cpp/CMakeLists.txt:31-63` |
| Reference `nativeInit(Surface, vertex, fragment)` → `ANativeWindow_fromSurface` → `startInitVideoGL` | donor pattern for surface→EGL wiring | reference `native_video_player.cpp:20-34`, `egl_renderer.cpp:104-111` |

### Current behavior

| Behavior | Status | Evidence/Source |
|---|---|---|
| `:videolib` is a scaffold: loads `libvideolib.so`, returns a greeting and the linked FFmpeg version; no rendering, no EGL/GLES, no `Surface` handling. | fact | `NativeLib.kt`, `videolib.cpp:8-45` |
| CMake statically links FFmpeg 7.1 (arm64-v8a, armeabi-v7a) with 16 KB page alignment; links only `android` + `log` system libs — **no `EGL`/`GLESv3`**. | fact | `CMakeLists.txt:31-63`, `build.gradle.kts` (abiFilters, page-size flags) |
| Library `AndroidManifest.xml` is empty; host owns permissions/UI/surface. | fact | `videolib/src/main/AndroidManifest.xml` |
| `:videolib` targets `minSdk 21`; EGL + GLES3 are available at that level. | fact | `build.gradle.kts` (`minSdk = 21`); [assumption] GLES3 availability |

### Affected boundaries (confirmed)

| Boundary | Why it matters | Status | Evidence/Source |
|---|---|---|---|
| JNI exported-name contract | new `Java_com_cii_videolib_*` symbols + Kotlin `external` decls must match exactly; existing names must stay | fact | `videolib.cpp`, `NativeLib.kt` |
| CMake / native linkage | must add `EGL` + `GLESv3`; preserve FFmpeg static link order + `-Bsymbolic` (arm64) + 16 KB alignment | fact | `CMakeLists.txt:47-63` |
| ABI packaging | only `arm64-v8a` + `armeabi-v7a` are buildable (per-ABI FFmpeg archives) | fact | `build.gradle.kts` abiFilters, `CMakeLists.txt:19-30` |
| Public library API (`:videolib`) | `publicContract: true`, external consumers unknown → additive-only, no in-repo caller proves safety | fact | registry; project ground rules §11 |
| Native lifecycle / threading / EGL | surface create/destroy ordering, single render-thread affinity, `ANativeWindow` ref-count | fact (in donor); applies to new code | reference `egl_renderer.cpp`, `single_thread_executor.h` |

### Reuse candidates

| Candidate | Location | Apparent fit | Confidence |
|---|---|---|---|
| EGL bring-up + `renderFrame` + `cleanup` | reference `egl_renderer.cpp` (external repo) | high — direct preview-onto-surface pattern; **adapt**, do not import (FFmpeg-7.1/static, c++17 differ from donor's c++11 + shared FFmpeg 3.x `.so`) | medium |
| Shader compile/link helpers | reference `gl_unit.cpp` | high — standalone, no FFmpeg dependency | high |
| Texture upload helpers | reference `texture_loader.cpp` | medium — YUV path assumes `AVFrame`; RGBA path is source-agnostic | medium |
| `SingleThreadExecutor` | reference `single_thread_executor.h` | high — render-thread serialization primitive | high |
| In-repo `camera` EGL/GLES pipeline | `camera/src/main/cpp` | reference-only; **do not** cross module boundary (ground rules: scope = `:videolib`) | n/a |

> Non-normative. No placement/extension decision is made here; solution-design owns file layout and API shape.

## 7. Non-functional / Technical Constraints

- **ABI:** deliver `arm64-v8a` + `armeabi-v7a` only; keep 16 KB max-page-size on both and `-Wl,-Bsymbolic` on arm64 (existing FFmpeg link requirement). [fact: `CMakeLists.txt`, `build.gradle.kts`]
- **Toolchain:** C++17, NDK `29.0.14206865`, CMake `3.22.1`, `compileSdk 36`, `minSdk 21`. [fact: `build.gradle.kts`]
- **Additive public contract:** preserve existing `NativeLib` JNI names and `System.loadLibrary("videolib")` load. [fact]
- **Rendering thread-safety:** all GL/EGL calls on one thread with the context current (donor-established invariant). [assumption from reference]
- **API contract constraints:** N/A — no remote/backend/API surface (focus area 5 is N/A).

## 8. Open Questions, Assumptions & Conflicts

| ID | Item | Classification | Owner | Consequence |
|---|---|---|---|---|
| OQ-1 | What frame content does the preview render when video/camera are excluded — synthetic/test pattern generated natively, or a caller-supplied pixel/YUV buffer through JNI? | assumption / **deferrable** | solution-design | Changes JNI + Kotlin signature shape, not the product goal (visible GLES preview on the Surface). Assumed: self-contained/synthetic source. |
| OQ-2 | Preview frame pixel format & type — reuse FFmpeg `AVFrame` YUV420P (as donor) or a plain RGBA buffer independent of FFmpeg. | unknown / deferrable | solution-design | Determines whether the preview path depends on the linked FFmpeg archives at all. |
| OQ-3 | Surface delivery — host passes a `Surface` via JNI (donor `nativeInit(Surface,…)`) vs `:videolib` ships a `SurfaceView`/`SurfaceHolder.Callback`. | assumption / deferrable | solution-design | Affects public API and whether the library owns a view. Assumed: host-supplied `Surface`. |
| OQ-4 | Shader provenance — vertex/fragment passed as Kotlin strings (donor) vs bundled `assets/glsl`. | unknown / deferrable | solution-design | Affects packaging + API; not the product goal. |
| A-1 | GLES version = 3 (donor uses `EGL_CONTEXT_CLIENT_VERSION 3` / `GLESv3`); available at `minSdk 21`. | assumption / non-material | — | Standard baseline; low risk. |

No source conflicts detected between ticket statements. (Donor-vs-current toolchain differences are compatibility risks, §9-R4, not requirement conflicts.)

## 9. Risk Analysis

| Risk | Likelihood/impact | Affected FR/area | Status | Introduced by change | Source |
|---|---|---|---|---|---|
| R1 — Native lifecycle/threading defects: EGL context thread affinity, surface create/destroy ordering, `ANativeWindow` leak or use-after-release on rotation/backgrounding | med / high | FR-1, native-lifecycle | open | yes | reference `egl_renderer.cpp:130-148` shows the required teardown; new code must replicate it |
| R2 — ABI/link regression: adding `EGL`/`GLESv3` must not disturb FFmpeg static link order, `-Bsymbolic` (arm64), or 16 KB alignment; failure breaks the build on one ABI | med / high | FR-2, build/ABI | open | yes | `CMakeLists.txt:47-63`; project ground rules §11 (16 KB, ABI) |
| R3 — Public-contract breakage: renaming/moving existing `NativeLib` JNI names, or shipping a non-additive API, can break unknown external consumers of a public library | low / high | FR-2, public API | open | uncertain | registry (`externalConsumers: unknown`); ground rules §11 |
| R4 — Donor/spec mismatch: reference code is c++11 + shared FFmpeg 3.x (`avcodec-57`) and `AVFrame`-fed by a video player; copying its YUV/`AVFrame` API into c++17 + static FFmpeg 7.1 (with the video source excluded) may not compile/link and can pull in unnecessary FFmpeg coupling | med / med | FR-1, FR-3, reuse | open | yes (if copied verbatim) | reference `CMakeLists.txt` vs `videolib/…/CMakeLists.txt`; `video_gl.cpp` `AVFrame` usage |
| R5 — Unverifiable by unit tests: GL rendering correctness needs on-device visual verification on both ABIs; CI/JVM tests give false confidence | med / med | SC-1, verification | open | no | GL work runs on GPU/EGL surface; no JVM harness for pixels |

---

### Handoff

Validation **PASS**; no blocking gap; guarded run → `AUTOMATION: CONTINUE`. Ready for solution-design.

```yaml
validation:
  status: PASS
  failures: []
  warnings:
    - "Ticket is terse; SC-1/SC-2 are the only explicit outcomes. Pipeline steps recorded as labelled assumptions (§5a/§6), not invented requirements."
    - "Corrected stale registry edge app->network to source-verified app->videolib to unblock the loader."
    - "Reference scaffold is in a separate repository; treated as adapt-only pattern evidence."
  coverage:
    requirements_with_evidence: "3/3"
    stories_mapped: "1/1"
    success_conditions_mapped: "2/2"
    material_unknowns_resolved: "n/a — 1 material ambiguity (OQ-1) classified deferrable, covered by assumption"
```
