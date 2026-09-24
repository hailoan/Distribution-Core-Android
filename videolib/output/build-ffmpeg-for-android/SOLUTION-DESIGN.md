AUTOMATION: CONTINUE

# SOLUTION-DESIGN — build-ffmpeg-for-android

Solution/architecture design for **compiling FFmpeg from source for Android with the NDK**, owned by
`:videolib`. Design only — no production code, tasks, sequencing, or estimates. Statements are tagged
`observed` (repo evidence), `derived` (forced by evidence), `proposed` (evidence-backed default
awaiting confirmation), `intentionally unspecified`, or `blocked`.

This is a **build-tooling deliverable**: the "system" is a reproducible cross-compile that emits a
per-ABI FFmpeg artifact tree. There is no runtime UI/state/data-flow feature here; the five design
concerns are applied to the build pipeline and its output contracts. Wiring the artifacts into
`:videolib`'s CMake/JNI at runtime is **out of scope** (deferred integration ticket, per DEV-SPEC).

Traceability preserved: `FR-1/FR-2` → `SC-1/SC-2` → `AC-1/AC-2`; `ST-1/ST-2` retained.

---

## 1. Decision ledger

**Investigation depth:** Standard (~12 lookups). Evidence reused from `/study`: `:videolib`
scaffold (`build.gradle.kts`, `CMakeLists.txt`, `NativeLib.kt`, `videolib.cpp`), `camera` FFmpeg +
16 KB precedent (`camera/build.gradle.kts`, `camera/src/main/cpp/CMakeLists.txt`, commit `b8b5804`),
`settings.gradle.kts`, version catalog. Skills routed: module-impact-analysis, reuse-detection,
risk-analysis (reused); ffmpeg-guideline, ndk-cpp-guideline, gradle-module-guideline (loaded this
stage). No bounded escalation; two consecutive no-gain lookups not reached.

**Sources / evidence**

| Ref | Evidence | Status |
|---|---|---|
| E1 | DEV-SPEC FR-1/FR-2, SC-1/SC-2, §5a, §8, §9 | observed |
| E2 | `:videolib` build has **no** `abiFilters`, no NDK page-size flags, `minSdk 21`, JVM 11, links only `android`+`log` | observed (`videolib/build.gradle.kts`, `videolib/src/main/cpp/CMakeLists.txt`) |
| E3 | `camera` 16 KB precedent: `CMAKE_ANDROID_PAGE_SIZE 16384`, `-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON`, `-DANDROID_LD_FLAGS=--page-size=16384`, `abiFilters = {arm64-v8a, armeabi-v7a}`, ndk `29.0.14206865`, `ANDROID_PLATFORM android-24` | observed (`camera/build.gradle.kts`, commit `b8b5804`) |
| E4 | `camera` vendored FFmpeg is **3.2.12**, shared `.so` only (no static `.a`), layout `ffmpegv2/<abi>/{include, lib*-NN.so}`, read-only vendored input | observed (ffmpeg-guideline; `camera/src/main/cpp/ffmpegv2/`) |
| E5 | `:videolib` has no in-repo project consumers; composite-build list empty; public module, external consumers unknown | observed (`settings.gradle.kts`, `.aidlc/modules.json`) |

**Decisions** (evidence-backed defaults; each records its alternative + the confirmation it needs)

| ID | Decision | Status | Basis | Alternative / confirmation |
|---|---|---|---|---|
| D1 | Build is driven by a **standalone host NDK cross-compile script** invoking FFmpeg's autotools `./configure && make`, **not** Gradle `externalNativeBuild`/CMake. | derived | FFmpeg ships an autotools build; Gradle CMake cannot drive `configure`. E2 shows `:videolib`'s CMake target only builds first-party `.cpp`. | None viable; CMake cannot replace FFmpeg configure. |
| D2 | Pin **one FFmpeg release tag** for all ABIs; default to a **current stable/maintained FFmpeg release line** compatible with NDK r29 clang. | proposed | NDK `29.0.14206865` (E3) is recent; FFmpeg 3.2.12 (2018, E4) predates modern clang and is high-risk to configure under r29. | Alt: match `camera`'s 3.2.12 for soname/API parity. Confirm at implementation whether artifact-level consistency with `camera` is required. Version is a public-contract parameter (AC-3). |
| D3 | Target ABIs **arm64-v8a + armeabi-v7a**. | proposed | Mirrors `camera` (E3), which deliberately dropped x86/x86_64 in `b8b5804`. | Alt: add `x86`/`x86_64` for emulator use. Confirm if emulator support is wanted. |
| D4 | Build each ABI against an **NDK API level that honors `:videolib` `minSdk 21`** (API 21 floor; per-ABI minimum where a 64-bit ABI requires higher). | derived | E2 `minSdk 21`; artifacts must load on the module's declared floor. `camera` uses android-24 but has `minSdk 24`. | None; forced by module `minSdk`. Confirm only if `:videolib` `minSdk` is raised. |
| D5 | Default **license posture = LGPL-safe**: no `--enable-gpl`, no `--enable-nonfree`; component enable/disable set chosen accordingly. | proposed | Public, distributable module with unknown external consumers (E5); LGPL avoids copyleft obligations on consumers. | Alt: enable GPL components if required by intended features. GPL/nonfree is a deliberate legal decision (AC-5 note) and, with publication, separately authorized. |
| D6 | **Library linkage mode** default = **static `.a`** per ABI (plus headers), to be linked into the consumer's own `.so` at integration. | proposed | A single JNI consumer avoids the 8-soname load-order dance ndk/ffmpeg-guideline flag as load-bearing; simpler packaging. | Alt: shared `.so` to mirror `camera`'s vendored shape (E4). Confirm at integration; affects packaging & the deferred wiring ticket. |
| D7 | **Output artifact layout** = per-ABI `<abi>/include` + `<abi>/lib` under a `:videolib`-owned directory, mirroring `camera`'s `ffmpegv2/<abi>/…` shape. | proposed | Reuse of the proven layout (E4) eases the deferred integration. | Exact directory name is implementation-local. |

**Explicitly unspecified (implementation-local):** exact script filename/path and language; `configure`
flag list beyond the license/alignment/ABI/API contracts below; precise output directory name; whether
artifacts are committed to VCS vs produced on demand; build-host OS. These do not change architecture
or a public contract.

**Blockers:** none. The one blocking ambiguity (deliverable = from source) was resolved in DEV-SPEC by
clarification.

---

## 2. Behavior and state transitions

Behavior here is the **observable outcome of running the build**, per FR/SC.

**Behavior contract**

| FR-ID | SC-ID | AC-ID | Story-ID | Rule / trigger | Observable outcome | Failure / recovery |
|---|---|---|---|---|---|---|
| FR-1 | SC-1 | AC-1 | ST-1 | Maintainer runs the build for the selected ABI set | FFmpeg native artifacts (libraries per D6 + `include/` headers) exist under the per-ABI output layout (D7) for every selected ABI | A failed `configure`/`make` for an ABI leaves **no partial artifact** for that ABI and surfaces a non-zero build failure; other ABIs' results are independent |
| FR-2 | SC-2 | AC-2 | ST-2 | Same build, with 16 KB alignment enforced | Every produced ELF (`.so`, or the final consumer `.so` when static per D6) is **16 KB page-aligned** (LOAD segment align = 0x4000) | Alignment verification (§6) fails the build if any produced ELF is not 16 KB-aligned |

**State model** — per-ABI build outcome (only correctness-relevant states)

| State | Meaning / invariants | Permitted events | Prohibited / ignored |
|---|---|---|---|
| `unbuilt` | No artifact for this ABI | `configure` | consuming its artifacts |
| `configured` | ABI toolchain + flags resolved (target triple, sysroot, API level D4, license flags D5, alignment ldflags §5) | `make`/`install` | mixing another ABI's toolchain |
| `built` | Artifacts present in per-ABI layout (D7); alignment holds (AC-2) | verify, consume (deferred) | treating as verified before alignment check |
| `failed` | `configure`/`make`/verify failed | re-run after fix | leaving a partial artifact tree that reads as `built` |

**Transition contract**

| From | Event / precondition | To | Side effect | Failure / recovery |
|---|---|---|---|---|
| `unbuilt` | `configure` with the ABI profile (D2–D5, §5) | `configured` | toolchain/env resolved for exactly one ABI | non-zero exit → `failed`, no artifact |
| `configured` | `make` + install to per-ABI dir (D7) | `built` | libraries + headers written under `<abi>/…` | partial output cleaned; ABI → `failed` |
| `built` | alignment verify (§6) passes (AC-2) | `built` (verified) | — | verify fail → `failed`; artifact must not be published/consumed |

---

## 3. Components and responsibilities

**Module Contract Matrix**

| Module | Owner/consumer | Responsibility | Depends on | Crossed contract | Compatibility obligation | Verification obligation |
|---|---|---|---|---|---|---|
| `:videolib` | primary owner | Owns the FFmpeg-from-source build capability and the produced per-ABI artifact tree | NDK `29.0.14206865`, CMake toolchain (host), FFmpeg source tag (D2) | native/ABI packaging, build-tooling; (future) JNI linkage — deferred | Public module, external consumers **unknown**; produced soname/version + license (D2/D5) become a downstream contract when integration lands | Cross-compile emits artifacts per ABI (AC-1); ELF 16 KB alignment check (AC-2). `:videolib:assembleDebug` unaffected until wiring lands |
| `:camera` | reference only | Source of the 16 KB + FFmpeg-layout precedent | — | none (not modified) | Its vendored `ffmpegv2` (3.2.12) stays read-only | n/a |

**Components** (semantic build-tooling roles; no invented class/file names)

| Component role | Observed/proposed | Responsibility / owned state | Delegates to | Dependency direction | Must not own/know | Evidence/decision |
|---|---|---|---|---|---|---|
| FFmpeg source (pinned) | proposed | Versioned input; single tag for all ABIs | — | input to build orchestrator | Android specifics | D2 |
| NDK cross-compile toolchain | observed | Per-ABI clang/sysroot/target-triple + API level | — | input to build orchestrator | FFmpeg internals | E3, D4 |
| Build orchestrator (script) | proposed | Drives `configure`→`make`→install per ABI; injects alignment/license/ABI/API flags; enforces per-ABI isolation | toolchain, source | depends on toolchain + source; produces artifact tree | Runtime/JNI usage of the output | D1, D3–D5 |
| Per-ABI build profile | proposed | The exact cross-compile flag set for one ABI (triple, cross-prefix, `--enable/--disable`, extra-cflags/ldflags incl. 16 KB) | — | consumed by orchestrator | Other ABIs' state | D2–D5, §5 |
| Artifact tree (output) | proposed | Per-ABI `include/` + libraries (D6) in the layout D7; the deliverable | — | produced by orchestrator; consumed later by integration (deferred) | How it is linked | D6, D7 |
| Alignment verifier | proposed | Asserts every produced ELF is 16 KB-aligned (AC-2) before `built`→verified | — | reads artifact tree | Build internals | FR-2/SC-2, §6 |

Decomposition is intentionally minimal and single-module; no new runtime abstraction, DI, or shared
library is introduced (project ground rules — narrowest owning module).

---

## 4. End-to-end data flow

One materially different trigger: **maintainer runs the build**. Per selected ABI (D3), independently:

| Step | Participant | Input/source | Decision/transformation | Output/side effect | Error propagation |
|---|---|---|---|---|---|
| 1 | Build orchestrator | FFmpeg source tag (D2), selected ABI | Select per-ABI build profile (triple, cross-prefix, API level D4) | Configured cross-compile env for one ABI | Unknown ABI / missing toolchain → abort that ABI (`failed`) |
| 2 | Per-ABI build profile → FFmpeg `configure` | Env from step 1 | Apply component enable/disable (license D5), `--target-os=android`, arch/cpu, and **16 KB alignment ldflags** (§5), static/shared per D6 | `configured` state | `configure` non-zero → `failed`, no artifact |
| 3 | `make` + install | `configured` tree | Compile + install to per-ABI output dir (D7) | Libraries + headers under `<abi>/…`; `built` | build error → clean partial output, `failed` |
| 4 | Alignment verifier | Produced ELF(s) | Check LOAD segment align = 0x4000 (AC-2) | Pass → verified; artifact consumable (deferred) | Misaligned → `failed`; block consume/publish |
| 5 | (deferred) Integration | Verified artifact tree | Out of scope this ticket | — | — |

Source of truth: the produced per-ABI artifact tree. Ordering across ABIs is independent (no
cross-ABI reconciliation). No persistence/offline concerns.

---

## 5. Boundary contracts

| Contract / boundary | Status | Semantic input | Output/result | Invariants | Errors | Compatibility / versioning | Owner |
|---|---|---|---|---|---|---|---|
| **16 KB alignment** (AC-2) | proposed | Build ldflags | Every produced ELF has 16 KB max-page-size (LOAD align 0x4000) | Applied uniformly to all ABIs and all produced libs; from-source equivalent of `camera`'s `--page-size=16384` (E3) — expressed as FFmpeg `--extra-ldflags="-Wl,-z,max-page-size=16384"` | Any misaligned ELF fails verify | Required for 16 KB-page Android devices | `:videolib` |
| **FFmpeg version / soname** (AC-3) | proposed (D2) | Single pinned tag | Deterministic soname/API set across ABIs | Same version for every ABI | Version mismatch across ABIs is invalid | Becomes a public contract at integration; parity with `camera` 3.2.12 is the recorded alternative | `:videolib` |
| **ABI set** | proposed (D3) | Selected ABI list | Artifacts only for selected ABIs | Each ABI built with its own isolated toolchain | Cross-ABI toolchain bleed is invalid | Matches `camera` ARM-only default | `:videolib` |
| **Build API level** (AC-4→ D4) | derived | Per-ABI API level | Artifacts loadable at `:videolib` `minSdk 21` | ≥ module `minSdk`; ≥ per-ABI 64-bit floor | Building below `minSdk` is invalid | Tied to `:videolib` `minSdk 21` (E2) | `:videolib` |
| **License posture** (AC-5→ D5) | proposed | `configure` enable/disable set | LGPL-safe artifact by default | No GPL/nonfree unless explicitly chosen | Enabling GPL/nonfree without approval is a policy break | Affects distribution of a public module; publication separately authorized | `:videolib` |
| **Linkage mode** | proposed (D6) | static/shared choice | `.a` (default) or `.so` per ABI + headers | Consistent across ABIs | Mixed linkage modes invalid | Drives the deferred integration/packaging shape | `:videolib` |
| **Output layout** | proposed (D7) | Per-ABI install path | `<abi>/include` + `<abi>/lib` tree | Stable, per-ABI isolated | — | Mirrors `camera` `ffmpegv2/<abi>` for easy reuse | `:videolib` |
| **(deferred) JNI/CMake link contract** | blocked-by-scope | — | — | — | — | Out of scope; belongs to the integration ticket | `:videolib` |

---

## 6. Conditional cross-cutting design

**Native/toolchain (ndk-cpp + gradle-module guidelines).** The build is out-of-band from Gradle
(D1); it does not alter `:videolib`'s CMake target, `abiFilters`, or `assembleDebug` in this ticket.
Keep NDK `29.0.14206865` / CMake `3.22.1` consistent with the repo toolchain (E3). Do **not** modify
`camera`'s vendored `ffmpegv2` (read-only precedent, ffmpeg-guideline).

**Licensing/security.** No credentials or secrets in the build script; publication credentials remain
environment-only (gradle-module guideline). License posture is D5.

**Verification obligations** (proof levels — never claim runtime from compile):

| Obligation | Proves | Level |
|---|---|---|
| Cross-compile completes per ABI, artifacts present (AC-1) | linkage, soname/arch resolution, configure correctness | build |
| ELF 16 KB alignment check — `readelf -l` LOAD `align 0x4000` (or NDK `check_elf_alignment`) on every produced ELF (AC-2) | 16 KB page compliance | build/static-inspection |
| (deferred) consumer loads FFmpeg on a supported device/ABI | actual runtime load/behavior | device — **out of scope**, integration ticket |

No DI, UI, navigation, persistence, migration, or concurrency concerns are implicated by this
build-tooling deliverable.

---

## 7. Coverage audit

| Item | Design decision / blocker |
|---|---|
| FR-1 (build FFmpeg from source, NDK, `:videolib`) | §3 components, §4 flow, D1–D7; AC-1 |
| FR-2 (16 KB support) | §5 alignment contract, §6 verify; AC-2 |
| SC-1 → AC-1 | §2 behavior row 1; §4 |
| SC-2 → AC-2 | §2 behavior row 2; §5/§6 |
| ST-1 / ST-2 | §2 behavior contract (Story-ID column) |
| DEV-SPEC §8: target ABIs | D3 (proposed, arm64-v8a + armeabi-v7a) |
| DEV-SPEC §8: FFmpeg version | D2 (proposed default + `camera` 3.2.12 alternative) — AC-3 contract |
| DEV-SPEC §8: components + license | D5 (LGPL-safe default) — AC-5 contract |
| DEV-SPEC §8: build API vs minSdk 21 | D4 (derived, honor minSdk 21) — AC-4 contract |
| DEV-SPEC §8: output layout | D7 (mirror `camera` layout) |
| DEV-SPEC §8: static vs shared / STL | D6 (static default + shared alternative) |
| DEV-SPEC §8: where build runs | D1 (host script, not Gradle CMake) |
| DEV-SPEC §9 risks (alignment, ABI, license, toolchain, reproducibility, version divergence) | §5 contracts + §6 verify + D2/D3/D5/D6 |

**Unresolved inputs needed to finalize (non-blocking; confirm at implementation):** FFmpeg version tag
(D2), whether x86/x86_64 are wanted (D3), GPL vs LGPL feature needs (D5), static vs shared (D6). Each
has an evidence-backed default recorded above; none blocks a complete, implementable design.

No blocker remains → `AUTOMATION: CONTINUE`.
