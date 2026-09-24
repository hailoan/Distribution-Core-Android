AUTOMATION: CONTINUE

# IMPLEMENT-PLAN — preview-video-opengles-by-ndk

Execution-ready plan for the `:videolib` OpenGL ES preview slice. Planning only — no production code.
Source design: `SOLUTION-DESIGN.md` (first line `AUTOMATION: CONTINUE`; AC-1..AC-6, D-1..D-6, U-1).

## 1. Planning control

- **Outcome:** CONTINUE — DAG valid, no planning blocker. One vertical slice, single module `:videolib`.
- **Design revision consumed:** SOLUTION-DESIGN §§1-7 (D-1 native renderer, D-2 RGBA+pattern/no-FFmpeg,
  D-3 host `Surface` via JNI, D-4 native shader strings, D-5 JNI naming, D-6 CMake EGL/GLESv3).
- **Skills routed:** module-impact-analysis, planning, risk-analysis, native-boundary-guideline,
  opengles-guideline, ndk-cpp-guideline, gradle-module-guideline. Their required checks are converted
  into Test-IDs / Check-IDs (§7), not left as prose.
- **Module reconciliation:** change set = `:videolib` only (FR-2). Verified edge `app → videolib`
  (`app/build.gradle.kts:55`); `:app` is an affected consumer for build-closure only (no forced app
  edit). No other in-repo consumer.
- **Investigation ledger (bounded):** `videolib.cpp` (JNI surface, 2 existing exports), `CMakeLists.txt`
  (target + link group, lines 30-60), `NativeLib.kt` (facade + `loadLibrary`), `FfmpegLinkageTest.kt`
  (existing instrumented convention, `T-LINK`). No new material after these 4 reads.
- **Unresolved planning inputs:** none blocking. **U-1** (design-intentional impl-local): the Kotlin
  facade class name (extend `NativeLib` vs new sibling), exact method/param names, new native
  filenames, shader/test-pattern contents. Allowed impl-local per planning-detail boundary; the
  class-name ↔ JNI-symbol coupling is contained inside a single task (T4) so names cannot drift.

## 2. Change-surface inventory

| Change-ID | existing/new | action | exact path | symbol / config key | Design-Ref | responsibility | evidence / decision | shared/collision key |
|---|---|---|---|---|---|---|---|---|
| C1 | existing | modify | `videolib/src/main/cpp/videolib.cpp` | add `Java_com_cii_videolib_<Facade>_<method>` exports; **preserve** `…_stringFromJNI`, `…_nativeFFmpegVersion` | none — code-driven (D-5) | JNI entry points: `Surface`→`ANativeWindow`, push RGBA (direct `ByteBuffer`), request pattern, release | `videolib.cpp:8-45`; D-5, ndk-cpp name-link | `videolib.cpp` |
| C2 | existing | modify | `videolib/src/main/cpp/CMakeLists.txt` | `add_library` source list (L30-32); `target_link_libraries` add `EGL GLESv3` (L46-60) | none | build/link wiring; **preserve** FFmpeg `--start-group…--end-group`, arm64 `-Wl,-Bsymbolic`, 16 KB, ABI filter | `CMakeLists.txt:30-60`; D-6, R2 | `CMakeLists.txt` |
| C3 | existing-or-new (U-1) | extend **or** create | `videolib/src/main/java/com/cii/videolib/NativeLib.kt` **or** new `…/com/cii/videolib/<Facade>.kt` | preview `external fun`s (attach surface, push RGBA frame, request pattern, release); **preserve** existing decls + `loadLibrary("videolib")` | none | Public Kotlin facade of the preview path | `NativeLib.kt:3-23`; D-3, D-5, R3 | Kotlin-facade |
| C4 | new | create | `videolib/src/main/cpp/preview_renderer.{cpp,h}` (filename impl-local, U-1) | EGL preview renderer + renderer state model + frame source | none | Owns EGL display/surface/context, GL program handle, current texture, lifecycle state (§2 design) | D-1, D-2, EV-2 | — |
| C5 | new | create | `videolib/src/main/cpp/gl_program.{cpp,h}` (filename impl-local, U-1) | GLES3 compile/link, RGBA texture upload, full-surface quad draw, native shader-string constants, test pattern | none | Draw path (RGBA + pattern), no FFmpeg/YUV | D-2, D-4, EV-3, EV-5 | — |
| C6 | new | create | `videolib/src/main/cpp/render_thread_executor.h` (filename impl-local, U-1) | single-thread task queue (mutex+condvar; drain+join on destroy) | none | GL/EGL thread-affinity primitive | D-1, EV-5, AC-4; ndk-cpp | — |
| C7 | new | create | `videolib/src/androidTest/java/com/cii/videolib/PreviewBindingTest.kt` (name impl-local) | instrumented test (owner: testing) | none | Symbol-resolution + facade load + R3 regression | extends `FfmpegLinkageTest.kt` convention | `videolib-androidTest` |

## 3. Work-item backlog

| FR-ID | SC-ID | AC-ID | Work-ID | outcome | module | depends on |
|---|---|---|---|---|---|---|
| FR-1, FR-2, FR-3 | SC-1, SC-2 | AC-1..AC-6 | W1 | An OpenGL ES preview renders a host-supplied RGBA frame (and a built-in test pattern) onto an Android `Surface`, entirely within `:videolib`, with no video/camera/FFmpeg dependency on the preview path | `videolib` (consumer `app` build-closure only) | — |

Single vertical slice (walking skeleton = pattern render, AC-2, proves the end-to-end EGL path with zero host data). No foundation/spike work item needed; a dedicated integration work item is unnecessary because the closure checks (§7) attach directly to W1.

## 4. Task backlog

Owner stage in brackets. All production tasks are android-dev; C7 is testing; Checks are integration-testing.

| Task-ID | Work-ID | module | owner stage | objective | Change-IDs / scope | Design-Ref | preconditions/inputs | invariants | done condition | verification | depends on | collision key |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| T1 | W1 | videolib | android-dev | Add single-thread GL executor primitive | C6 | none | — | Drain+join on destroy; no new global locks; task runs on the one render thread (EV-5, AC-4) | Header complete, self-consistent by source inspection | compile via IT1 | — |
| T2 | W1 | videolib | android-dev | Implement GLES3 program: compile/link, RGBA texture upload, quad draw, shader-string constants, native test pattern | C5 | none | GLES 3.0 `#version 300 es` | No OES/`SurfaceTexture`/YUV/`AVFrame`; **no FFmpeg include** on this path (AC-6); compile/link failure returns safe (AC-5) | Source complete; program-fail + upload paths present by inspection | IT1 | — |
| T3 | W1 | videolib | android-dev | Implement EGL preview renderer + state model (`Idle→Ready→Rendering→Released/Failed`) + frame source (host RGBA / pattern) | C4 | none | T1, T2 | All GL/EGL marshalled to executor (AC-4); ordered teardown; exactly one `ANativeWindow` ref released once; init failure→`Failed` (R1, AC-3, AC-5) | Source complete; transitions + idempotent teardown present by inspection | IT1; behavior via IT3 | T1, T2 | — |
| T4 | W1 | videolib | android-dev | Bind renderer across JNI: add C++ exports **and** Kotlin `external fun`s together (atomically coupled name contract) | C1, C3 | none | T3 | Export name `Java_com_cii_videolib_<Facade>_<method>` matches Kotlin class exactly; **preserve** existing 2 exports + `loadLibrary` (R3, D-5); direct `ByteBuffer` zero-copy, must outlive call; `GetStringUTFChars`↔release if any string crosses (ndk-cpp) | Both sides declared with matching names by inspection | T-LOAD; IT1 | T3 | `videolib.cpp`, `Kotlin-facade` |
| T5 | W1 | videolib | android-dev | CMake wiring: register C4/C5/C6 sources in `add_library`; add `EGL GLESv3` to `target_link_libraries` | C2 | none | T1-T4 (files exist) | **Preserve** FFmpeg `--start-group…--end-group` order, arm64 `-Wl,-Bsymbolic`, `CMAKE_ANDROID_PAGE_SIZE 16384` / `max-page-size=16384`, ABI filter (`arm64-v8a`,`armeabi-v7a`); add no new native target (R2, D-6) | `:videolib` configures & links | IT1 | T1, T2, T3, T4 | `CMakeLists.txt` |
| T6 | W1 | videolib | testing | Author instrumented test: preview facade native methods resolve (no `UnsatisfiedLinkError`); existing `stringFromJNI`/`nativeFFmpegVersion` still resolve (R3 regression) | C7 | none | T4 (facade+JNI names fixed) | Extend `FfmpegLinkageTest` convention; assert symbol resolution + facade construction, not pixels | Test source authored; `Test-ID T-LOAD` | executed in testing stage on device/emulator | T4 (author); T5 (execute) | `videolib-androidTest` |

No `:app` production task: FR-2 confines the change to `:videolib`; D-3 gives the host the `Surface`, and the design records no forced app edit. A sample-host demo in `:app` is optional and unrequested — omitted.

## 5. Dependency map (DAG)

- `T1 → T3` (contract: renderer uses executor)
- `T2 → T3` (contract: renderer uses GL program)
- `T3 → T4` (contract: JNI binds renderer API)
- `T4 → T5` (wiring: sources must exist to register; build)
- `T1,T2,T3,T4 → T5` (ownership serialization: single `CMakeLists.txt`)
- `T4 → T6` (test: needs fixed facade+JNI names) · `T5 → T6` (test executes against built `.so`)
- `T5 → IT1` (build), `T5 → IT2` (consumer build), `IT1 → IT3` (device run needs a linked `.so`)
- Ownership-serialization on `videolib.cpp` (T4) and `CMakeLists.txt` (T5): each edited by one task only.

**Cycle check:** T1/T2 → T3 → T4 → T5 → {IT1, IT2, T6} → IT3. No back-edges → acyclic. ✔

## 6. Execution waves

| Wave | Task IDs | Concurrency / serialization | Ownership reason |
|---|---|---|---|
| 1 | T1, T2 | concurrent | disjoint new files (C6, C5); no shared symbol |
| 2 | T3 | single | depends on T1+T2; new file C4 |
| 3 | T4 | single | modifies shared `videolib.cpp` + Kotlin facade; atomically coupled JNI name contract |
| 4 | T5 | single | modifies shared `CMakeLists.txt`; needs all sources present |
| 5 | IT1, IT2, T6 (author) | IT1‖IT2 concurrent (disjoint modules); T6 authoring concurrent | build closure; test authoring touches only androidTest |
| 6 | T6 (execute), IT3 | after a successful build | device/emulator required |

## 7. Test scope and verification matrix

| Test-ID | AC-ID / risk | level | target component/contract | behavior/error scope | fake/fixture boundary | production Task-ID | depends on | execution expectation |
|---|---|---|---|---|---|---|---|---|
| T-LOAD | AC-4, AC-5 (load), AC-6, R3 | Android instrumented (`androidTest`) | Kotlin facade ↔ JNI binding (C1+C3) | native methods resolve; `loadLibrary` succeeds; existing 2 exports still resolve; facade constructs without any FFmpeg/video/camera call | none (real `.so`, no Surface) | T4 | T5 build | device/emulator-dependent — authored-only until testing stage runs it |

Visual ACs (AC-1 host frame, AC-2 pattern, AC-3 surface destroy/recreate) cannot be asserted by JUnit without a real `Surface` + pixel readback → covered as device/manual checks below, not as authored assertions.

### Module integration matrix

| Check-ID | changed module | affected consumer/external contract | boundary | command or device/manual check | why required | owner integration-testing task |
|---|---|---|---|---|---|---|
| IT1 | videolib | self (native/ABI) | CMake link + ABI packaging | `:videolib:assembleDebug` on `arm64-v8a` + `armeabi-v7a` | proves EGL/GLESv3 added without breaking FFmpeg static group / `-Bsymbolic` / 16 KB (R2, D-6) | IT-task-1 |
| IT2 | videolib | `app` (project dependency) | additive public Kotlin/JNI API | `:app:assembleDebug` | consumer still builds; existing `NativeLib` names intact (R3) | IT-task-2 |
| IT3 | videolib | end user (SC-1) | Surface rendering + native lifecycle | supported-device run: RGBA frame visible (AC-1), test pattern visible (AC-2), surface destroy→recreate with no crash/leak (AC-3), thread affinity holds (AC-4) | GL runtime, EGL lifecycle, and visible output are unprovable by compile or JUnit (R1, R5) | IT-task-3 |

`execution expectation`: IT1/IT2 = runnable Gradle (authorized `assembleDebug` is normal verification); IT3 = device-dependent, not claimable before a supported-device run.

## 8. Sprint plan

Omitted — sprint packing / capacity not requested.

## 9. Shared infrastructure and risk constraints

| Risk (design §9) | affected Change/Task/AC | required serialization / verification | owning task |
|---|---|---|---|
| R1 native lifecycle/threading | C4 / T3 / AC-3, AC-4 | ordered + idempotent teardown, one `ANativeWindow` release, single-thread executor; device-verified | T3 (guard), IT3 (verify) |
| R2 ABI/link regression | C2 / T5 / FR-2 | serialize `CMakeLists.txt`; preserve FFmpeg group + `-Bsymbolic` + 16 KB + ABI; both-ABI build | T5 (guard), IT1 (verify) |
| R3 public-contract breakage | C1, C3 / T4 / FR-2 | additive only; preserve existing JNI names + `loadLibrary`; regression assert | T4 (guard), T6 + IT2 (verify) |
| R4 donor/spec mismatch | C4, C5 / T2, T3 / FR-3, AC-6 | RGBA-only preview, **no FFmpeg include** on preview path (source inspection) | T2, T3 (guard), IT1 (link-clean) |
| R5 unverifiable by unit test | all / IT3 / SC-1 | do not claim runtime rendering from compile/JUnit; require device run | IT3 |

**Shared-boundary serialization:** `videolib.cpp` (T4) and `CMakeLists.txt` (T5) are each single-owner and single-wave. No DI/DB/socket/nav shared infrastructure is touched (`:videolib` has no DI; no manifest/permission change). No vendored FFmpeg edit, no new ABI, no `SurfaceView`/View added (D-3).

---

Coverage: AC-1..AC-6 → W1 → T1-T5 with T-LOAD + IT1/IT2/IT3; every Task path/symbol resolves to §2;
T-LOAD resolves to AC-4/AC-5/AC-6/R3 + T4. DAG valid, no blocker. Guarded run → `AUTOMATION: CONTINUE`.
