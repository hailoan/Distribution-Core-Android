AUTOMATION: CONTINUE

# SOLUTION-DESIGN — preview-video-opengles-by-ndk

Turns DEV-SPEC (FR-1..FR-3, SC-1/SC-2, ST-1) into a behavioral + native-boundary contract for an
OpenGL ES preview path inside `:videolib`. Design only — no production code, no task sequencing.

## 1. Decision ledger

**Investigation depth:** standard; ~7 design lookups (reused DEV-SPEC evidence, no new material after
2 confirming reads). Graph unavailable → source inspection. Skills routed: module-impact-analysis,
architecture-analysis, reuse-detection, risk-analysis, native-boundary-guideline, opengles-guideline,
ndk-cpp-guideline, gradle-module-guideline.

| ID | Type | Statement | Status | Impact |
|---|---|---|---|---|
| EV-1 | observed | `:videolib` native surface = one static `libvideolib.so`, FFmpeg 7.1 **statically** linked, links only `android`+`log`; no `EGL`/`GLESv3`. `System.loadLibrary("videolib")` in `NativeLib` companion is the only load step (no separate FFmpeg `.so`, unlike `camera`'s `JNILibraryLoader`). | fact (`CMakeLists.txt:31-63`, `videolib.cpp`, `NativeLib.kt:18-23`, packet §8) | Load order is trivial; no loader wiring needed. |
| EV-2 | observed | Donor `videogl` renders host-supplied `Surface` → `ANativeWindow_fromSurface` → single render thread (`SingleThreadExecutor`) → EGL(GLES3) → draw → `eglSwapBuffers`; teardown releases context/surface/display + `ANativeWindow_release`. | fact (`native_video_player.cpp:20-34`, `egl_renderer.cpp`) | Adopted as the structural pattern (adapted, not imported). |
| EV-3 | observed | Donor upload path is YUV420P `AVFrame` → 3× `GL_R8` textures, YUV→RGB in shader. Donor is c++11 + **shared** FFmpeg 3.x (`avcodec-57`). `:videolib` is c++17 + static FFmpeg 7.1. | fact (`video_gl.cpp:65-139`, donor `CMakeLists.txt`) | Verbatim reuse risks R4; preview format decided below (D-2). |
| EV-4 | observed | In-repo precedent for a library-owned GL surface = `camera`'s `GLPreview` (`SurfaceView`+`SurfaceHolder.Callback2`); `camera` links `EGL`,`GLESv3` and keeps 16 KB/ABI settings. | fact (packet §6, §8) | Confirms both delivery models are valid in-repo; informs D-3. |
| EV-5 | observed | GL invariant (transferable from `opengles-guideline`): single EGL context/thread; **no** OES/`SurfaceTexture`/`samplerExternalOES` path; shaders compiled at runtime from strings; GLES 3.0 `#version 300 es`. | fact | Constrains the renderer contract (§2 state model, §5). |
| D-1 | decision | Deliver the preview as a **native EGL/GLES renderer in `:videolib`** adapted from the donor, not by importing donor files and not by reaching into `camera`. Resolves DEV-SPEC OQ-1 owner. | proposed | Keeps scope = `:videolib` (FR-2), avoids R4 verbatim-copy trap. |
| D-2 | decision | Preview frame source = **caller-supplied RGBA8888 pixel buffer** (single 2D texture, no YUV/`AVFrame`), **plus** a native-generated **test pattern** used when no host frame is supplied. **No FFmpeg dependency on the preview path.** Resolves OQ-1+OQ-2. | proposed | Satisfies FR-3/SC-2 (independent of video/camera), neutralizes R4. FFmpeg stays linked but unused by preview. |
| D-3 | decision | Surface delivery = **host-supplied `android.view.Surface` via JNI** (donor model, EV-2), matching DEV-SPEC OQ-3 assumption. A library-owned `SurfaceView` wrapper (the `GLPreview` precedent, EV-4) is a **compatible additive future extension**, not built here. Resolves OQ-3. | proposed | Smallest public contract; library stays view-agnostic; host owns lifecycle (packet §6). |
| D-4 | decision | Shaders authored as **native constant strings inside the module** (GLES 3.0), not host-passed and not `assets/glsl`. Resolves OQ-4. | proposed | Minimal API; no packaging/asset-reader surface; RGBA path needs only a trivial passthrough program. |
| D-5 | decision | New JNI exports follow the exact mangled pattern `Java_com_cii_videolib_<KotlinClass>_<method>`; existing `stringFromJNI`/`nativeFFmpegVersion` names are **preserved unchanged** (additive only). | proposed | Enforces R3 (public-contract) + ndk-cpp name-link discipline. |
| D-6 | decision | CMake adds `EGL` + `GLESv3` to the existing `videolib` target's link list; **preserve** FFmpeg static `--start-group…--end-group` order, arm64 `-Wl,-Bsymbolic`, and 16 KB page settings; add no new native target. | proposed | Contains R2; one shared `.so` (ndk-cpp "one target" rule). |
| A-1 | assumption | GLES **3.0** context (`EGL_CONTEXT_CLIENT_VERSION 3`), available at `minSdk 21`. | non-material | Consistent with donor + `camera`. |
| U-1 | unspecified (impl-local) | Whether preview lifecycle methods extend the existing `NativeLib` Kotlin class or a sibling facade class; exact method/param names; test-pattern appearance; vertex/frag string contents; even-dimension handling. | open | Any choice keeps the contract additive; deferred to implementation. |
| BL-0 | blocker | None. All DEV-SPEC deferrals (OQ-1..OQ-4) resolved from evidence above. | resolved | Guarded run → CONTINUE. |

**Traceability:** FR-1 → SC-1 → AC-1..AC-5 · FR-3 → SC-2 → AC-6 · FR-2 → Module Contract Matrix (§3) · ST-1 spans FR-1/FR-2.

## 2. Behavior and state transitions

### Behavior contract

| FR | SC | AC | Story | Rule / trigger | Observable outcome | Failure / recovery |
|---|---|---|---|---|---|---|
| FR-1 | SC-1 | AC-1 | ST-1 | Host supplies a valid `Surface` + an RGBA pixel buffer, then requests render | The RGBA image is visible on the surface (GPU-composited via GLES) | Invalid buffer/dims → frame ignored, logged; last good frame or clear color remains |
| FR-1 | SC-1 | AC-2 | ST-1 | Host supplies a `Surface` but no frame (or requests the built-in pattern) | A native-generated test pattern is visible — proves the pipeline with zero host data | Program/EGL not ready → nothing drawn, logged; no crash |
| FR-1 | SC-1 | AC-3 | ST-1 | Surface becomes available / is destroyed (host lifecycle) | On available: EGL+program initialized once; on destroyed: EGL torn down and `ANativeWindow` released **in order** | Double-init/double-release are ignored (idempotent) |
| FR-1 | SC-1 | AC-4 | ST-1 | Any GL/EGL operation is requested from any caller thread | Operation is marshalled onto the single render thread with the context current | Off-thread GL is never executed directly |
| FR-1 | SC-1 | AC-5 | ST-1 | Shader compile/link or EGL config/context creation fails | Renderer enters a safe non-rendering state; error is logged; init returns failure | Host may retry init on a fresh surface |
| FR-3 | SC-2 | AC-6 | ST-1 | Preview runs end-to-end | Output produced with **no** call into any video-decode/camera/FFmpeg code path | n/a (structural guarantee) |

### State model (renderer lifecycle)

| State | Meaning / invariants | Permitted events | Prohibited / ignored |
|---|---|---|---|
| `Idle` | No EGL, no `ANativeWindow`, no program | `surfaceAvailable` | render, releaseSurface (ignored) |
| `Ready` | EGL display/surface/context current on render thread; GL program linked; no frame yet | `pushFrame`, `requestPattern`, `surfaceDestroyed` | second `surfaceAvailable` (ignored/idempotent) |
| `Rendering` | ≥1 frame drawn; last frame/texture owned by renderer | `pushFrame`, `requestPattern`, `surfaceDestroyed` | — |
| `Released` | EGL torn down in order, `ANativeWindow` released, thread drained | `surfaceAvailable` (→ re-init) | render/push (ignored) |
| `Failed` | init/compile failed; non-rendering | `surfaceDestroyed`→`Released`, `surfaceAvailable`→retry | push/render (ignored) |

### Transition contract

| From | Event / precondition | To | Side effect | Failure / cancellation / recovery |
|---|---|---|---|---|
| Idle | `surfaceAvailable(Surface)`; window non-null | Ready | `ANativeWindow_fromSurface`; on render thread: initEGL(GLES3), build program, `eglMakeCurrent` | null window → log, stay Idle; EGL/compile fail → Failed (AC-5) |
| Ready/Rendering | `pushFrame(rgba,w,h)` valid | Rendering | On render thread: upload 1 RGBA texture, draw, `eglSwapBuffers` | invalid dims/buffer → ignore frame, keep state (AC-1) |
| Ready/Rendering | `requestPattern` | Rendering | On render thread: draw native test pattern, `eglSwapBuffers` | not Ready → ignore (AC-2) |
| Ready/Rendering/Failed | `surfaceDestroyed` | Released | On render thread: `eglMakeCurrent(NONE)`, destroy context/surface, `eglTerminate`, `ANativeWindow_release`, null it | idempotent if already Released |
| Released/Failed | `surfaceAvailable` | Ready | re-init as above | — |

## 3. Components and responsibilities

### Module Contract Matrix

| Module | Owner/consumer | Responsibility | Depends on | Crossed contract | Compatibility obligation | Verification obligation |
|---|---|---|---|---|---|---|
| `:videolib` | owner | Native EGL/GLES RGBA preview onto a host `Surface`; Kotlin JNI facade | Android NDK `EGL`,`GLESv3`,`android`,`log`; existing static FFmpeg (unused by preview) | JNI export names ↔ Kotlin `external`; CMake link list; ABI/16 KB packaging; public Kotlin API | Additive only; preserve existing JNI names, `loadLibrary("videolib")`, FFmpeg static link order, `-Bsymbolic`(arm64), 16 KB, ABI filter | `:videolib:assembleDebug` (both ABIs) + on-device visual run |
| `:app` | consumer | Sample host: owns `Surface`/View lifecycle, calls the facade | `:videolib` (`project(:videolib)`) | Consumes additive Kotlin API | No change forced by this ticket | `:app:assembleDebug` |
| external consumers | consumer (unknown) | — | `:videolib` public API | Public Kotlin/JNI contract | Must not break existing names (R3) | n/a (out-of-repo) |

### Components

| Component role | observed/proposed | Responsibility / owned state | Delegates to | Dependency direction | Must not own/know | Evidence/decision |
|---|---|---|---|---|---|---|
| Preview JNI facade (Kotlin) | proposed (extends existing `NativeLib` evidence) | Public entry: pass `Surface`, push RGBA frame / request pattern, release; declares `external fun`s; `System.loadLibrary("videolib")` | native renderer via JNI | Kotlin → JNI | EGL/GL details, threads | EV-1, D-5; U-1 (extend vs sibling) |
| Native preview renderer (C++) | proposed (adapts donor `EGLRenderer`) | Owns EGLDisplay/Surface/Context, GL program, current-frame texture, lifecycle state; runs the state model (§2) | render-thread executor; GL program unit | JNI → renderer | Kotlin types, `Surface` object (only `ANativeWindow`), video/camera/FFmpeg | EV-2, D-1 |
| Render-thread executor (C++) | proposed (adapts donor `SingleThreadExecutor`) | Serialize all GL/EGL work on one thread; drain+join on destroy | — | renderer → executor | GL semantics | EV-5, AC-4 |
| GL program + RGBA upload unit (C++) | proposed (adapts donor `gl_unit` + simplified texture path) | Compile/link GLES3 program, upload one RGBA texture, draw a full-surface quad | — | renderer → GL unit | EGL lifecycle, threading | EV-3, EV-5, D-4 |
| Frame source | proposed | Host RGBA buffer (direct `ByteBuffer`, zero-copy) or native test pattern | — | inbound to renderer | FFmpeg/YUV/`AVFrame` | D-2, FR-3 |
| Native build/link contract (CMake) | proposed change to observed | Add `EGL`+`GLESv3`; preserve FFmpeg group/`-Bsymbolic`/16 KB/ABI; one shared target | — | build | — | EV-1, D-6 |

## 4. End-to-end data flow

**Flow A — host-supplied RGBA frame → visible preview (AC-1):**

| # | Participant | Input/source | Decision/transformation | Output/side effect | Error propagation |
|---|---|---|---|---|---|
| 1 | Host | `Surface` (surface available) | — | Passed to facade | invalid → host ret/logs |
| 2 | JNI facade | `Surface` | `ANativeWindow_fromSurface` | window handed to renderer; post `surfaceAvailable` | null window → log, no state change |
| 3 | Renderer (render thread) | window | initEGL(GLES3), build program (D-4) | `Ready`; context current | EGL/compile fail → `Failed` (AC-5) |
| 4 | Host | RGBA buffer + w/h (**direct** `ByteBuffer`) | validate dims/capacity | `pushFrame` posted | invalid → frame ignored (AC-1) |
| 5 | GL unit (render thread) | RGBA bytes | upload single 2D texture (`GL_RGBA8`), draw quad | `eglSwapBuffers` → pixels on surface | GL error → log, keep last state |

**Flow B — no host frame → test pattern (AC-2):** steps 1-3 as above; then facade `requestPattern` →
renderer draws native pattern → `eglSwapBuffers`. Proves SC-1 with zero host data and zero FFmpeg use (AC-6).

**Flow C — surface destroyed (AC-3):** host `surfaceDestroyed` → renderer (render thread) tears down
EGL in order + `ANativeWindow_release` → `Released`. Ordering + idempotency are correctness-critical (R1).

Source of truth: the renderer owns EGL/GL state and the current texture; the host owns the `Surface`
and frame bytes (must outlive the native call — zero-copy direct buffer, per ndk-cpp-guideline).

## 5. Boundary contracts

| Contract / boundary | observed/proposed | Semantic input | Output/result | Invariants | Errors | Compatibility/versioning | Owner |
|---|---|---|---|---|---|---|---|
| Kotlin ↔ JNI (preview) | proposed | Surface; RGBA buffer + dims; pattern request; release | void / boolean init result | Export name = `Java_com_cii_videolib_<Class>_<method>`; Kotlin `external` signatures match exactly; existing names untouched | `UnsatisfiedLinkError` if names drift (build won't catch) | **Additive**; preserve `stringFromJNI`,`nativeFFmpegVersion`,`loadLibrary("videolib")` (R3, D-5) | `:videolib` |
| Frame buffer boundary | proposed | RGBA8888 bytes, width, height | Uploaded GL texture | Buffer **direct** + outlives call; capacity ≥ w·h·4; even dims (U-1) | non-direct/short buffer → reject frame | New; no external schema | `:videolib` |
| Surface ownership | proposed | `android.view.Surface` | `ANativeWindow` (renderer-held) | Renderer holds exactly one window ref; released once on destroy | null surface → init declines | Host owns Surface lifecycle (packet §6) | host / `:videolib` |
| Render-thread affinity | proposed | any GL/EGL request | executed on render thread | Context current only on that thread; no OES/SurfaceTexture (EV-5) | off-thread GL never runs directly | invariant | `:videolib` |
| CMake native link | proposed change | build config | `libvideolib.so` (arm64-v8a, armeabi-v7a) | FFmpeg static group order + arm64 `-Bsymbolic` + 16 KB preserved; `EGL`+`GLESv3` added; one target | link failure on either ABI = build break (R2) | ABI filter unchanged | `:videolib` |
| GLES version | proposed | — | GLES 3.0 context, `#version 300 es` | available at minSdk 21 (A-1) | context-create fail → `Failed` | matches donor/`camera` | `:videolib` |

## 6. Conditional cross-cutting design

- **Concurrency (correctness-critical):** exactly one render thread owns EGL/GL; the executor's
  destructor drains+joins before native objects are freed — order teardown so the executor's worker
  references nothing already released (R1; ndk-cpp-guideline). No new global locks.
- **Native lifecycle / references:** `GetStringUTFChars`↔`ReleaseStringUTFChars` if any string
  crosses; direct-`ByteBuffer` access is zero-copy (buffer must be direct + outlive the call); one
  `ANativeWindow` ref released exactly once (AC-3). No cross-thread `JNIEnv*` caching.
- **Build/packaging:** no vendored FFmpeg edits; no new ABI; 16 KB + `-Bsymbolic`(arm64) retained
  (D-6). Adding `EGL`/`GLESv3` must not reorder the FFmpeg link group (R2).
- **DI / UI toolkit:** none. `:videolib` has no DI framework and ships no View in this ticket (D-3);
  do not introduce either (packet §3, §7).
- **Security/privacy:** none — no I/O, network, permissions, or persistence. Library manifest stays empty.
- **Verification obligations (architectural):** `assembleDebug` on both ABIs proves JNI names,
  linkage, shader-string compile wiring; **only** a supported-device run proves visible rendering,
  EGL lifecycle, thread affinity, and RGBA correctness (R5; native/opengles guidelines). Never claim
  GL runtime behavior from compilation alone.
- **Risk mitigations (from DEV-SPEC §9):** R1→§2 ordered/idempotent transitions + single-thread rule;
  R2→D-6 preserve-link constraints; R3→D-5 additive names; R4→D-2 RGBA/no-FFmpeg preview; R5→two-level
  verification above.

No design diagram added — the state table (§2) and flows (§4) express the lifecycle more compactly.

## 7. Coverage audit

| Item | Resolved by | Status |
|---|---|---|
| FR-1 (GLES preview onto Surface via NDK) | D-1,D-2,D-3; AC-1..AC-5; Flows A/B/C; §5 | designed |
| FR-2 (changes confined to `:videolib`) | Module Contract Matrix §3; D-1,D-6 | designed |
| FR-3 (no video/camera source) | D-2; AC-6; Flow B | designed |
| SC-1 | AC-1 (host frame) + AC-2 (pattern) | designed |
| SC-2 | AC-6 (FFmpeg/video/camera-free path) | designed |
| ST-1 | facade + renderer contract (§3,§5) | designed |
| OQ-1 frame content | D-1 (owner) + D-2 (RGBA/pattern) | resolved |
| OQ-2 pixel format | D-2 (RGBA8888, no `AVFrame`) | resolved |
| OQ-3 surface delivery | D-3 (host `Surface` via JNI) | resolved |
| OQ-4 shader provenance | D-4 (native constant strings) | resolved |
| A-1 GLES version | GLES 3.0 (§5) | resolved |
| R1..R5 | §6 mitigations | addressed |

**Unresolved inputs needed to complete the design:** none (BL-0 resolved).

**Explicitly implementation-local (U-1), not blockers:** extend `NativeLib` vs sibling facade; exact
method/param names; test-pattern appearance; vertex/fragment string contents; even-dimension policy.

---

Design complete; no blocker remains. Guarded run → `AUTOMATION: CONTINUE`.
