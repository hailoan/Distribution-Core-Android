AUTOMATION: CONTINUE

# IMPLEMENT-PLAN — implement-ffmpeg-to-android-ndk

Execution plan to link the FFmpeg 7.1 static artifacts (from `videolib/ffmpeg-build/dist/`) into the
`:videolib` NDK build and prove the linkage at runtime. Planning only; all work confined to
`:videolib`.

Source design: `SOLUTION-DESIGN.md` (first line `AUTOMATION: CONTINUE`). Traceability chain preserved:
`FR-1/FR-2 → SC-1/SC-2 → AC-1…AC-6 → W1 → T1…T6`.

---

## 1. Planning control

- **Source design revision:** SOLUTION-DESIGN.md as written this ticket (decisions D1–D8, AC-1…AC-6).
- **Outcome:** CONTINUE — DAG valid, no planning blocker. Every AC maps to an ordered task with an
  exact change surface; every design decision resolves to a task or task invariant.
- **Investigation ledger (bounded, ~6 lookups):** confirmed exact current symbols/paths —
  `videolib/src/main/cpp/CMakeLists.txt` (`add_library(${CMAKE_PROJECT_NAME} SHARED videolib.cpp)`,
  `target_link_libraries(... android log)`), `videolib.cpp`
  (`Java_com_cii_videolib_NativeLib_stringFromJNI`), `NativeLib.kt`
  (`external fun stringFromJNI()`, `System.loadLibrary("videolib")`), `build.gradle.kts` (no
  `abiFilters`, no `ndkVersion`, `-std=c++17`, `minSdk 21`), empty `AndroidManifest.xml`, generated
  `ExampleUnitTest`/`ExampleInstrumentedTest`, on-disk `dist/{arm64-v8a,armeabi-v7a}/{include,lib/*.a}`
  (7 archives each) + `SHA256SUMS`. `settings.gradle.kts` already includes `:videolib`.
- **Assumptions (carried from design):** A1 `:videolib` unpublished this ticket (no LGPL release
  gate now); A2 `-std=c++17` + default STL links C archives unchanged; A3 committed archives are the
  provenance-stamped `dist/` output (verify via `SHA256SUMS`).
- **Blockers:** none.
- **Unresolved planning inputs:** none. Implementation-local latitude (design-sanctioned, not a
  blocker): exact committed directory name under `videolib/src/main/cpp/` (plan proposes `ffmpeg/`);
  whether the linkage-proof accessor reuses `stringFromJNI` or adds a sibling `external fun`; exact
  CMake IMPORTED target names vs. path-linking. android-dev chooses within the fixed structure/invariants
  below without selecting an architectural boundary.

---

## 2. Change-surface inventory

| Change-ID | existing/new | action | exact path | symbol/resource/config key | Design-Ref | responsibility | evidence/design decision | shared/collision key |
|---|---|---|---|---|---|---|---|---|
| C1 | new | migrate | `videolib/src/main/cpp/ffmpeg/<abi>/` (`include/` + `lib/*.a`) for `arm64-v8a`, `armeabi-v7a` | per-ABI FFmpeg 7.1 static tree (7 archives: avcodec, avdevice, avfilter, avformat, avutil, swresample, swscale) + headers | none — build-input | Commit provenance-verified artifacts into module source (git-ignored `dist/` is not reproducible on CI) | D1, E2, E5; `dist/MANIFEST.txt`, `SHA256SUMS` | `videolib-native-artifacts` |
| C2 | existing | modify | `videolib/src/main/cpp/CMakeLists.txt` | import static archives; `target_link_libraries(videolib …)`; arm64 `-Wl,-Bsymbolic`; `-Wl,-z,max-page-size=16384`; `-lm -lz`; `${ANDROID_ABI}` path; static link order | none — code-driven | Bind FFmpeg archives into `libvideolib.so` | D2, D3, D4; E3 (`check-alignment.sh:63-92`, `build-ffmpeg.sh:94-97`) | `videolib-build` |
| C3 | existing | modify | `videolib/build.gradle.kts` | `defaultConfig.ndk.abiFilters` = {arm64-v8a, armeabi-v7a}; `android.ndkVersion = "29.0.14206865"`; `externalNativeBuild.cmake.arguments` page-size flags | none — code-driven | ABI packaging + toolchain pin; keep `minSdk 21`, `-std=c++17` | D5, D6; E2, E6 (`camera/build.gradle.kts`) | `videolib-build` |
| C4 | existing | modify | `videolib/src/main/cpp/videolib.cpp` | JNI export (`Java_com_cii_videolib_NativeLib_*`) referencing one FFmpeg symbol (e.g. `av_version_info()`/`avformat_version()`) and returning it | none — code-driven | Force FFmpeg symbol retention; expose version to prove linkage | D7; E3 (static strip), E4 | `videolib-jni` |
| C5 | existing | modify | `videolib/src/main/java/com/cii/videolib/NativeLib.kt` | `external fun` accessor (reuse `stringFromJNI` or add sibling); keep `System.loadLibrary("videolib")` | none — code-driven | Kotlin facade for the linkage-proof accessor | D7; E4 | `videolib-jni` |
| C6 | new | verify | `videolib/src/androidTest/java/com/cii/videolib/` (e.g. `FfmpegLinkageTest`) | instrumented test invoking the accessor on-device | none — requirement-driven | Prove `System.loadLibrary` + FFmpeg symbol resolve at runtime (AC-2) | AC-2; §6 proof levels | `videolib-androidtest` |

Generated files (do not edit): `videolib/src/test/.../ExampleUnitTest.kt`,
`videolib/src/androidTest/.../ExampleInstrumentedTest.kt` — scaffold; untouched.

---

## 3. Work-item backlog

| FR-ID | SC-ID | AC-ID | Work-ID | outcome | module | depends on |
|---|---|---|---|---|---|---|
| FR-1, FR-2 | SC-1, SC-2 | AC-1, AC-2, AC-3, AC-4, AC-5, AC-6 | W1 | `:videolib` builds `libvideolib.so` with FFmpeg 7.1 statically linked for both ARM ABIs, 16 KB-aligned, and a runtime accessor returns the FFmpeg version — proving linkage — with no change outside `:videolib` | `videolib` | — |

Single vertical slice (walking skeleton): the smallest end-to-end path that proves FFmpeg is linked
and callable. No foundation or integration work item split out — the artifact commit and integration
check are ordered tasks within W1.

---

## 4. Task backlog

| Task-ID | Work-ID | owning module | owner stage | objective | Change-IDs / exact scope | Design-Ref | preconditions/inputs | invariants | done condition | verification / Test-ID / Check-ID | depends on | collision key |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| T1 | W1 | videolib | android-dev | Commit the per-ABI FFmpeg static artifact tree into module source | C1 — create `videolib/src/main/cpp/ffmpeg/{arm64-v8a,armeabi-v7a}/{include,lib/*.a}` | none | `dist/<abi>/` present; `SHA256SUMS` | Only the 2 ARM ABIs; 7 `.a` + `include/` per ABI; contents byte-match `dist/` (verify `SHA256SUMS`); no edit to `camera/ffmpegv2` | Both ABI trees committed; `sha256 -c SHA256SUMS` passes over committed libs | Check-C1 (source inspection + checksum) | — | videolib-native-artifacts |
| T2 | W1 | videolib | android-dev | Import archives and link FFmpeg into `libvideolib` in CMake | C2 — `videolib/src/main/cpp/CMakeLists.txt` | none | T1 tree present | Resolve libs by `${ANDROID_ABI}`; static link order (format/device/filter → codec → sws*/swresample → avutil) + `-lm -lz`; `-Wl,-Bsymbolic` on arm64-v8a only; `-Wl,-z,max-page-size=16384` both ABIs; keep `videolib.cpp` in target; keep `android` + `log`; FFmpeg 7.x header idioms (not camera 3.2.12) | CMake configures; link inputs correct per invariants | Check-BUILD (AC-1/AC-3), Check-ALIGN (AC-4) | T1 | videolib-build |
| T3 | W1 | videolib | android-dev | Set ABI filter + toolchain + page-size build config | C3 — `videolib/build.gradle.kts` | none | — | `abiFilters` = {arm64-v8a, armeabi-v7a} only; `ndkVersion = "29.0.14206865"`; external-native page-size args consistent with C2; preserve `minSdk 21`, `-std=c++17`, namespace, JVM 11 | `build.gradle.kts` sets the 3 config keys; no other module edited | Check-BUILD (AC-6), Check-ALIGN | — | videolib-build |
| T4 | W1 | videolib | android-dev | Reference an FFmpeg symbol via JNI and surface its version (both sides in sync) | C4 + C5 — `videolib.cpp` JNI export + `NativeLib.kt` `external fun` | none | T2 (archives linkable), T1 (headers) | JNI export name mangles to the exact `Java_com_cii_videolib_NativeLib_<method>` matching the Kotlin `external fun` (no `RegisterNatives`); keep `System.loadLibrary("videolib")`; keep or evolve `stringFromJNI` without breaking its symbol link; return value is FFmpeg-provided (not a literal) | Native + Kotlin compile; accessor returns FFmpeg version string; symbol names in sync | T-LINK (AC-2) | T2 | videolib-jni |
| T5 | W1 | videolib | testing | Author instrumented test proving runtime linkage | C6 — `videolib/src/androidTest/.../FfmpegLinkageTest` | none | T4 accessor exists | Instantiate `NativeLib`, invoke accessor, assert non-empty/FFmpeg-shaped version; runs on a supported ABI; do not assert exact version text (implementation-local) | Test authored, compiles, targets the accessor | Test-ID T-LINK (AC-2) | T4 | videolib-androidtest |
| T6 | W1 | videolib | integration-testing | Build, package-inspect, align-check, checksum, device-run, and scope-verify | C1–C6 (verify only) | none | T2, T3, T4, T5 complete | Execute the module integration matrix (§7) without modifying production code | `:videolib:assembleDebug` green; AAR carries only the 2 ARM ABIs; 16 KB LOAD alignment; `SHA256SUMS` ok; device/emulator run returns FFmpeg version; diff limited to `:videolib` | Check-BUILD, Check-ALIGN, Check-ABI, Check-C1, Check-SCOPE, T-LINK execution | T2, T3, T4, T5 | — |

---

## 5. Dependency map (DAG)

Typed edges:

- T1 → T2 (`contract`: CMake imports the committed archives)
- T1 → T2 (`data/schema`: headers needed to compile references)
- T2 → T4 (`contract`: FFmpeg symbols must be linkable/declared for the JNI reference)
- T4 → T5 (`test`: test exercises the accessor)
- T2 → T6, T3 → T6, T4 → T6, T5 → T6 (`wiring`/`test`: integration executes the full build + device run)
- C2 ↔ C3 share concern `videolib-build` but are **disjoint files** → logical order only, no ownership serialization; C4 ↔ C5 share `videolib-jni` and are edited together in **one task (T4)** so no cross-task collision.

Cycle check: T1 → T2 → T4 → T5 → T6, with T3 → T6 and T2/T3 feeding T6. No back-edges → acyclic. ✔

---

## 6. Execution waves

| Wave | Task IDs | Concurrency / serialization | Ownership reason | Prerequisites satisfied |
|---|---|---|---|---|
| 1 | T1, T3 | Concurrent | Disjoint files: artifact tree (`videolib-native-artifacts`) vs `build.gradle.kts` (`videolib-build`); no shared symbol | none |
| 2 | T2 | Serial | Needs T1 artifacts/headers; owns `CMakeLists.txt` | T1 |
| 3 | T4 | Serial | Needs T2 (linkable symbols); owns `videolib-jni` pair (C4+C5) | T2 |
| 4 | T5 | Serial | Testing stage; needs T4 accessor | T4 |
| 5 | T6 | Serial | Integration-testing; needs all production tasks + authored test | T2, T3, T4, T5 |

---

## 7. Test scope and verification matrix

| Test-ID | AC-ID / risk | level | target component/contract | behavior/transition/error scope | fake/fixture boundary | production Task-ID | depends on | execution expectation |
|---|---|---|---|---|---|---|---|---|
| T-LINK | AC-2; risk: static dead-code strip | Android instrumented (androidTest) | `NativeLib` accessor → FFmpeg symbol | Load `videolib`, invoke accessor, assert FFmpeg-provided non-empty version; `UnsatisfiedLinkError`/empty ⇒ fail | none (real native lib on device) | T4 (prod), T5 (test) | T4 | device/emulator-dependent — runs in T6; not proven by compile |

**Module integration matrix**

| Check-ID | changed module | affected consumer/external contract | boundary | command or device/manual check | why required | owner integration-testing task |
|---|---|---|---|---|---|---|
| Check-BUILD | videolib | external consumers unknown (public) | CMake/native linkage | `:videolib:assembleDebug` | Prove archives link into `libvideolib.so` per ABI; arm64 `-Bsymbolic` resolves TX-table relocations (AC-1, AC-3) | T6 |
| Check-ALIGN | videolib | packaging | 16 KB page alignment | Inspect `libvideolib.so` LOAD segment align ≥ `0x4000` per ABI (AAR/`readelf -l`) | Preserve 16 KB compatibility (AC-4) | T6 |
| Check-ABI | videolib | packaging | ABI set | Inspect AAR `jni/` contains only `arm64-v8a`, `armeabi-v7a` | No absent-ABI link attempt; artifacts exist only for these (AC-6) | T6 |
| Check-C1 | videolib | build input integrity | artifact provenance | `sha256 -c SHA256SUMS` over committed libs | Committed archives match provenance (A3) | T6 |
| Check-SCOPE | videolib | all other modules | module scope | `git diff --name-only` limited to `videolib/**`; `camera/ffmpegv2` untouched | Enforce SC-2 / AC-5 | T6 |
| T-LINK (exec) | videolib | runtime | JNI/load | Run T5 on supported-ABI device/emulator; accessor returns FFmpeg version | Only a device run proves load + symbol resolution (AC-2) | T6 |

> Proof-level discipline (native-boundary/ffmpeg/ndk-cpp guidelines): compile + AAR inspection prove
> AC-1/AC-3/AC-4/AC-6 and integrity (Check-BUILD/ALIGN/ABI/C1); **only** the device run proves AC-2
> (T-LINK exec). Do not claim AC-2 from compilation alone.

---

## 8. Sprint plan

Omitted — no delivery-backlog mode requested and no capacity supplied. Execution waves (§6) are the
ordering.

---

## 9. Shared infrastructure and risk constraints

| Risk (from design) | Affected Change/Task/AC | Required serialization / verification | Owning task |
|---|---|---|---|
| arm64-v8a link fails without `-Wl,-Bsymbolic` | C2 / T2 / AC-3 | Task invariant on T2; proven by Check-BUILD | T2, T6 |
| x86/x86_64 link failure (unfiltered ABIs) | C3 / T3 / AC-6 | Task invariant on T3; proven by Check-ABI | T3, T6 |
| 16 KB alignment regression | C2, C3 / T2, T3 / AC-4 | Page-size flags in both files; proven by Check-ALIGN | T2, T3, T6 |
| Static dead-code strip hides broken link | C4, C6 / T4, T5 / AC-2 | Force symbol reference (T4 invariant); device run T-LINK | T4, T5, T6 |
| Git-ignored `dist/` absent on CI | C1 / T1 | Commit tree into source; checksum gate Check-C1 | T1, T6 |
| NDK toolchain drift | C3 / T3 / AC-4 | Pin `ndkVersion 29.0.14206865` (T3 invariant) | T3 |
| Out-of-module edit violates scope | all / AC-5 | Serialize final scope check; JNI pair (C4+C5) kept in one task to avoid symbol desync | T4, T6 |
| LGPL static-link obligation (deferred) | — | Not triggered while unpublished (A1); first-publish release gate only — no task this ticket | — (recorded) |

No new design decisions introduced. Native/JNI (`videolib-jni`) and build-config (`videolib-build`)
touchpoints are flagged; the JNI pair is serialized within T4, and all closure verification is
serialized into T6.

---

_Traceability: FR-1/FR-2 → SC-1/SC-2 → AC-1…AC-6 → W1 → T1…T6, each AC mapped to a task + a
Check/Test-ID. Every task names its exact change surface (§2) or a design-sanctioned
implementation-local latitude (§1). DAG acyclic, waves ownership-disjoint within each wave. No
backlog metadata, estimates, priority, or code fabricated. `AUTOMATION: CONTINUE`._
