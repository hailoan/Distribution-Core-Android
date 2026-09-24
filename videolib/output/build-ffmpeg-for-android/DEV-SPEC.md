AUTOMATION: CONTINUE

# DEV-SPEC — build-ffmpeg-for-android

Feature Analysis understanding spec. Product truth plus the minimum current-system evidence needed
for solution-design. Engineering evidence is non-normative. Proposed edge cases, impacts, and risks
are labelled `[assumption]`/`[unknown]` and are not requirements.

## 0. Analysis Control

- **Outcome:** `CONTINUE` — final validation PASS; the single blocking gap (deliverable ambiguity)
  was resolved by clarification; no blocking gap remains.
- **Kind:** `feature` (ticket-reading; not a bug).
- **Triage path:** Full path — native / build / ABI surface. `:videolib` registry risk tags:
  `new-contract, sdk-exception, jni, abi` (`.aidlc/modules.json`).
- **Scope classification:** single-module build-tooling task, owner `:videolib`; native/ABI-sensitive
  (16 KB alignment); public module with unknown external consumers.
- **Depth / lookup ledger:** standard budget (~14); ~10 lookups used. Evidence: ticket +
  clarification; `videolib` build/CMake/JNI scaffold; `camera` vendored-FFmpeg + 16 KB precedent
  (incl. commit `b8b5804`); `settings.gradle.kts`; version catalog. No bounded escalation needed.
- **Skills loaded:** ticket-reading, module-impact-analysis (always); codebase-search, reuse-detection,
  risk-analysis, feature-clarification (triggered); dev-spec-validation (gate). figma-fetch /
  design-intent / api-analysis / dependency-analysis left OFF (no trigger).

Focus-area applicability:

| # | Focus area | Status |
|---|---|---|
| 1 | Requirements | Applicable (§2–§5) |
| 2 | Edge cases | Applicable — 16 KB explicit; others proposed (§5, §5a) |
| 3 | Feature impact | Applicable — bounded; `:videolib` build surface (§6) |
| 4 | Risk | Applicable (§9) |
| 5 | API docs | N/A — no remote/backend/network/auth surface; FFmpeg is a local native C API, no API doc supplied |
| 6 | Figma/design | N/A — no design source supplied |

## 1. Sources

| Source | Type | Location | Read status | Revision/time |
|---|---|---|---|---|
| Feature ticket | markdown | `videolib/output/build-ffmpeg-for-android.md` | read | working tree, 2026-09-08 |
| Clarification (deliverable) | user answer | this `/study` run | recorded | 2026-09-08 |
| `:videolib` module scaffold | code | `videolib/build.gradle.kts`, `videolib/src/main/cpp/CMakeLists.txt`, `NativeLib.kt`, `videolib.cpp` | read | working tree |
| `:camera` FFmpeg + 16 KB precedent | code/history | `camera/src/main/cpp/CMakeLists.txt`, `camera/build.gradle.kts`, commit `b8b5804` | read | history |
| Module registry | generated | `.aidlc/modules.json` (via stage packet) | read | — |
| Version catalog | build | `gradle/libs.versions.toml`, `settings.gradle.kts` | read | working tree |

- Design source: **none**.
- API docs: **none** (no remote/backend surface).
- Converted files: **none** (ticket already Markdown).

## 2. Overview & Business Goal

Provide the `:videolib` module with the ability to **build FFmpeg from source for Android using the
NDK**, producing cross-compiled native artifacts (shared/static libraries + headers) for the target
Android ABIs, with the resulting binaries **16 KB page-size aligned** so they load on Android
devices/emulators that use 16 KB memory pages. `[fact: ticket + clarification]`

Deliverable clarified: **compile FFmpeg from source with the NDK** (a build-tooling task producing
binaries + headers). `[fact:clarification]` Wiring those binaries into `:videolib`'s CMake/JNI so the
module links FFmpeg at runtime is **out of scope for this ticket** and deferred to a later
integration ticket. `[fact:clarification]`

## 3. Functional Requirements

| FR-ID | Requirement | Status | Evidence/source |
|---|---|---|---|
| FR-1 | Cross-compile FFmpeg from source for Android using the NDK, owned by module `:videolib`. | fact | ticket ("Build FFMPEG for Android use NDK", "scope: `:videolib`") + clarification (from source) |
| FR-2 | The produced FFmpeg native binaries must support Android 16 KB page alignment. | fact | ticket ("Edge case: support Android 16KB") |

## 4. Actors & User Stories

| Story-ID | FR-ID | Story |
|---|---|---|
| ST-1 | FR-1 | As a `:videolib` maintainer/developer, I can run an NDK-based build that compiles FFmpeg from source into Android-native artifacts, so the module has FFmpeg binaries available to integrate later. `[fact: ticket + clarification]` |
| ST-2 | FR-2 | As a maintainer, the FFmpeg binaries I build are 16 KB-aligned, so a consuming app does not crash loading them on 16 KB-page Android devices. `[fact: ticket]` |

Primary actor: `:videolib` maintainer / build engineer (no end-user runtime actor in this ticket —
integration is deferred). `[assumption: derived from build-tooling scope]`

## 5. Observable Success Conditions

| SC-ID | FR-ID | Explicit/clarified outcome | Evidence/source |
|---|---|---|---|
| SC-1 | FR-1 | Running the FFmpeg build with the NDK produces FFmpeg native artifacts (libraries + headers) for the targeted Android ABI(s). | ticket + clarification |
| SC-2 | FR-2 | The produced `.so` artifacts are 16 KB page-aligned (loadable on 16 KB-page Android). | ticket |

Explicit-outcome edge cases from the ticket: **16 KB page support** (SC-2) is the only edge case the
ticket states. All rows in §5a are best-effort proposals and are **not** success conditions.

### 5a. Proposed edge cases & boundary behavior (best-effort — not success conditions)

| FR-ID | Edge / boundary case | Expected handling (proposed) | Status | Source |
|---|---|---|---|---|
| FR-2 | 16 KB alignment mechanism | Build/link with 16 KB max-page-size (e.g. linker `-z max-page-size=16384` / NDK page-size flags), mirroring `camera` which sets `CMAKE_ANDROID_PAGE_SIZE 16384` and passes `-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON`, `-DANDROID_LD_FLAGS=--page-size=16384`. | assumption | code: `camera/src/main/cpp/CMakeLists.txt:10`, `camera/build.gradle.kts` externalNativeBuild args |
| FR-1 | Target ABIs | Cover `arm64-v8a` + `armeabi-v7a`; `camera` deliberately dropped `x86`/`x86_64` in commit `b8b5804`. Confirm whether x86/x86_64 emulator support is wanted here. | unknown | code: `camera/build.gradle.kts` `abiFilters`; commit `b8b5804` |
| FR-1 | FFmpeg version/tag to build | A specific FFmpeg release/tag must be chosen; `camera` vendors an older era (`libavcodec-57`, `libavutil-55`, `libavformat-57`). Version for a from-source build is unspecified. | unknown | code: `camera/src/main/cpp/ffmpegv2/*/lib*-*.so` |
| FR-1 | Enabled components + license | `configure` flags (which codecs/muxers/filters; `--enable-gpl` vs LGPL-only) affect size and licensing. Unspecified. | unknown | assumption: FFmpeg build conventions |
| FR-1 | NDK / min API level | `camera` builds with `ndkVersion 29.0.14206865` and `ANDROID_PLATFORM android-24`; `:videolib` declares `minSdk 21`. Build API level for the from-source artifacts is unspecified and interacts with the module's min SDK. | unknown | code: `camera/build.gradle.kts`; `videolib/build.gradle.kts` (`minSdk 21`) |
| FR-1 | Artifact output location/layout | Proposed to mirror `camera`'s `ffmpegv2/<ABI>/{include, lib*.so}` layout for later reuse. | assumption | code: `camera/src/main/cpp/CMakeLists.txt:13` |
| FR-1 | Where the build runs | Cross-compile is a host build script (NDK + configure/make), not necessarily a Gradle `externalNativeBuild` task; `:videolib` currently only has a hello-world CMake target. | assumption | code: `videolib/src/main/cpp/CMakeLists.txt` |

## 6. Engineering Evidence — Non-normative

**Module impact hypothesis**

| Module | Owner/consumer | Dependency evidence | Likely contract | Status/confidence |
|---|---|---|---|---|
| `:videolib` | primary owner | ticket names `:videolib`; `settings.gradle.kts:79` includes it; no in-repo project consumers | native/ABI + build-tooling; gains a from-source FFmpeg build capability + artifacts | fact / high |
| `:camera` | reference only (not modified) | existing vendored FFmpeg + 16 KB config | reuse pattern source; **do not modify** | fact / high |

- Public module, **external consumers unknown** — keep compatibility visible even though no in-repo
  consumer exists (`.aidlc/modules.json`: `:videolib` `public; external consumers unknown`). `[fact]`
- Composite-build substitution list in `settings.gradle.kts` is empty; `:videolib` has no project
  dependents in-repo. `[fact]`

**Verification implications** (candidates only — non-normative)

| Module/consumer | Candidate check | Reason | Status |
|---|---|---|---|
| `:videolib` build | Run the FFmpeg NDK build script; confirm artifacts produced per targeted ABI | SC-1 | candidate |
| artifacts | Inspect `.so` alignment (e.g. `readelf -l`/`llvm-readelf` LOAD `align 0x4000`, or NDK `check_elf_alignment`) | SC-2 (16 KB) | candidate |
| `:videolib` | `:videolib:assembleDebug` (module default) once/if artifacts are wired — deferred | module default verification | candidate (deferred) |

**Entry points** (current scaffold — smallest confirmed set)

| Symbol | Role | File:line |
|---|---|---|
| `NativeLib.stringFromJNI()` | Kotlin `external` JNI entry | `videolib/src/main/java/com/cii/videolib/NativeLib.kt:9` |
| `System.loadLibrary("videolib")` | native load | `videolib/src/main/java/com/cii/videolib/NativeLib.kt:14` |
| `Java_com_cii_videolib_NativeLib_stringFromJNI` | JNI impl ("Hello from C++") | `videolib/src/main/cpp/videolib.cpp:4-9` |
| CMake target `videolib` | links `android`, `log` only | `videolib/src/main/cpp/CMakeLists.txt` |

**Current behavior**

| Behavior | Status | Evidence/source |
|---|---|---|
| `:videolib` is a bare JNI/CMake scaffold; native lib returns a hello string; **no FFmpeg** present or linked. | fact | `NativeLib.kt`, `videolib.cpp`, `CMakeLists.txt` |
| `:videolib` build has **no** `abiFilters`, **no** NDK page-size flags, `minSdk 21`, JVM 11, `cmake cppFlags -std=c++17`, and does **not** apply the publish plugin. | fact | `videolib/build.gradle.kts` |
| `:camera` already builds native code with 16 KB alignment and imports prebuilt FFmpeg per-ABI; it is the in-repo precedent (not the target). | fact | `camera/src/main/cpp/CMakeLists.txt:10,13,43-142`, `camera/build.gradle.kts` |

**Affected boundaries** (confirmed)

| Boundary | Why it matters | Status | Evidence/source |
|---|---|---|---|
| Native/ABI packaging | 16 KB alignment + ABI selection are ELF/packaging contracts; wrong alignment crashes on 16 KB devices | fact | ticket; `camera` precedent + commit `b8b5804` |
| Build/toolchain (NDK/CMake) | From-source cross-compile depends on NDK version, platform/API level, STL | fact | `camera/build.gradle.kts` (`ndkVersion 29.0.14206865`, `ANDROID_PLATFORM android-24`) |
| Public library / publication | `:videolib` is public with unknown external consumers; bundled native + license flags affect downstream | fact | `.aidlc/modules.json` |

**Reuse candidates** (no reuse decision)

| Candidate | Location | Apparent fit | Confidence |
|---|---|---|---|
| FFmpeg per-ABI CMake `IMPORTED` + `ffmpegv2/<ABI>/{include,lib}` layout | `camera/src/main/cpp/CMakeLists.txt:13,43-103` | canonical pattern for later wiring (integration, deferred) | high |
| 16 KB build config (`CMAKE_ANDROID_PAGE_SIZE 16384`, flexible-page-size + `--page-size=16384`, ndk 29) | `camera/build.gradle.kts`, `camera/src/main/cpp/CMakeLists.txt:10` | directly reusable for FR-2 alignment | high |

## 7. Non-functional / Technical Constraints

- **16 KB alignment (FR-2):** produced `.so` must be 16 KB page-aligned; precedent uses page-size
  16384 / flexible-page-size linker flags. `[fact: ticket; assumption on exact mechanism]`
- **Toolchain:** NDK-based cross-compile; catalog is Kotlin 2.2.10 / AGP 8.9.1 / compileSdk 36; `camera`
  uses `ndkVersion 29.0.14206865`. `:videolib` module: `minSdk 21`, JVM 11. `[fact]` The build API
  level vs `minSdk 21` should be reconciled. `[unknown]`
- **Scope discipline:** change confined to `:videolib`; do **not** modify `:camera` or its vendored
  `ffmpegv2` binaries; no opportunistic renames/upgrades/publication changes. `[fact: ground rules]`
- **Publication:** publishing/signing/upload/distribution and any FFmpeg licensing decision
  (GPL/LGPL) are separately authorized and not part of this analysis. `[fact: ground rules]`
- API contract constraints: **N/A** — no remote/backend API surface.

## 8. Open Questions, Assumptions & Conflicts

| Item | Classification | Owner | Consequence |
|---|---|---|---|
| Deliverable = compile FFmpeg **from source** with NDK; runtime wiring into `:videolib` deferred. | fact:clarification (resolved) | user | Sets scope; resolved the only blocking gap. |
| Target ABIs (arm64-v8a + armeabi-v7a only, or include x86/x86_64?) | unknown — deferrable | user/maintainer | Affects build matrix & size; `camera` precedent = 2 ARM ABIs. |
| FFmpeg version/tag to build. | unknown — deferrable | user/maintainer | Determines features/API; not blocking for build-tooling setup. |
| Enabled components + license (`--enable-gpl` etc.). | unknown — deferrable | user/maintainer | Size + downstream licensing for a public module. |
| Build API level vs `minSdk 21`. | unknown — deferrable | maintainer | Affects supported devices; reconcile before shipping artifacts. |
| Artifact output layout (mirror `camera` `ffmpegv2/<ABI>`?). | assumption — deferrable | maintainer | Eases later integration; not blocking. |
| Static vs shared libs / STL (`c++_static` per camera). | assumption — deferrable | maintainer | Packaging & symbol footprint. |

No unresolved **blocking** gaps. No source conflicts (ticket + clarification + code are consistent;
the `minSdk 21` vs `android-24` item is a reconciliation note, not a source conflict).

## 9. Risk Analysis

| Risk | Evidence | Affected FR/area | Likelihood/Impact | Introduced by change | Status | Owner | Needs design decision |
|---|---|---|---|---|---|---|---|
| Built `.so` not 16 KB-aligned → crash on 16 KB-page Android devices. | ticket edge case; `camera` needed explicit 16 KB config (commit `b8b5804`) | FR-2 / native-ABI | med / high | yes | open | maintainer | yes (alignment flags) |
| Wrong/incomplete ABI coverage (missing arm64, or shipping unwanted x86). | `camera` `abiFilters` = 2 ARM ABIs after `b8b5804` | FR-1 / packaging | med / med | yes | open | maintainer | yes (ABI list) |
| FFmpeg licensing (GPL vs LGPL) on a public module with unknown external consumers. | `.aidlc/modules.json` (`:videolib` public; publication); FFmpeg `configure` options | FR-1 / publication | low-med / high | uncertain | open | maintainer | yes (license/config) |
| NDK/toolchain drift (build NDK/API level differs from module `minSdk 21` / catalog). | `camera` ndk 29 + android-24 vs `videolib` minSdk 21 | FR-1 / build | med / med | uncertain | open | maintainer | yes |
| From-source build not reproducible / not integrated with Gradle (`:videolib` has only a hello CMake target). | `videolib/src/main/cpp/CMakeLists.txt` | FR-1 / build tooling | med / med | yes | open | maintainer | no (tooling choice) |
| Version divergence from `camera`'s older vendored FFmpeg if later cross-used. | `camera` `libavcodec-57`/`libavutil-55` (older era) | FR-1 / future integration | low / med | uncertain | deferred | maintainer | no (integration ticket) |
