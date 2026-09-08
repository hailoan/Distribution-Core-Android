AUTOMATION: CONTINUE

# DEV-SPEC — implement-ffmpeg-to-android-ndk

Integrate the FFmpeg-from-source artifacts produced by `videolib/ffmpeg-build/` into the NDK/CMake
native build of module `:videolib`, via `videolib/src/main/cpp/CMakeLists.txt`. Scope is limited to
`:videolib`.

---

## 0. Analysis Control

- **Outcome:** CONTINUE — validation PASS, no blocking gap. Ticket is a bounded native/build
  integration with rich artifact provenance; ABI, linkage, alignment, and toolchain details are
  evidence-backed and recorded as constraints/assumptions rather than blocking questions.
- **Scope classification:** existing-code, single-module, native-boundary + build-logic. Owning
  module `:videolib`. Not greenfield (a JNI/CMake scaffold already exists).
- **Depth:** standard (~12 lookups; native/build boundary). No escalation used.
- **Lookup ledger:** ticket spec; `videolib` `CMakeLists.txt`, `build.gradle.kts`, `videolib.cpp`,
  `NativeLib.kt`; `ffmpeg-build/README.md`, `config.sh`, `dist/MANIFEST.txt`, actual `dist/<abi>/`
  tree; `camera` `ffmpegv2/<abi>` layout, `camera/src/main/cpp/CMakeLists.txt`,
  `camera/build.gradle.kts`. Two consecutive lookups added no new material evidence → stopped.

**Focus areas**

| # | Focus area | Status |
|---|---|---|
| 1 | Requirements | Applicable — §2/§3/§4/§5 |
| 2 | Edge cases | Applicable (best-effort) — §5a |
| 3 | Feature impact | Applicable — §6 (existing scaffold touched) |
| 4 | Risk | Applicable — §9 |
| 5 | API docs | N/A — no remote/backend/network surface; "API" here is native C linkage, covered in §6/§7 |
| 6 | Figma/design | N/A — no design source supplied |

---

## 1. Sources

- **Ticket:** `videolib/output/implement-ffmpeg-to-android-ndk.md` — `source_type: local-md`,
  `retrieved_at: 2026-09-08`, revision: working tree.
- **Referenced build tooling (evidence, not a new requirement source):**
  `videolib/ffmpeg-build/` — `README.md`, `config.sh`, `dist/MANIFEST.txt`, `dist/SHA256SUMS`,
  produced `dist/<abi>/{include,lib/*.a}`. Provenance for the artifacts to be integrated. This
  tooling was delivered by the sibling ticket `build-ffmpeg-for-android`.
- **Referenced target:** `videolib/src/main/cpp/CMakeLists.txt` (the file the ticket names as the
  integration point).
- **Mirror reference:** `camera/src/main/cpp/ffmpegv2/<abi>` + `camera/src/main/cpp/CMakeLists.txt`
  (existing FFmpeg-in-NDK integration pattern in this repo).
- Design link/source: none. API docs: none. Converted files: none.

---

## 2. Overview & Business Goal

`:videolib` is a scaffold native library whose CMake build currently links only Android system
libraries. A separate ticket already cross-compiled FFmpeg 7.1 from source into per-ABI static
archives + headers under `ffmpeg-build/dist/`. This ticket wires those artifacts into `:videolib`'s
NDK build so the module's shared object (`libvideolib.so`) is built against FFmpeg and FFmpeg symbols
are available to the module's native code. `[fact:ticket]` `[fact:code]`

The `ffmpeg-build/README.md` explicitly frames this ticket: *"wiring the artifacts into the module's
CMake/JNI is a separate, later ticket."* `[fact:code]`

---

## 3. Functional Requirements

| FR-ID | Requirement | Status | Evidence/Source |
|---|---|---|---|
| FR-1 | Integrate the FFmpeg artifacts from `ffmpeg-build/` into the `:videolib` NDK build through `src/main/cpp/CMakeLists.txt` so FFmpeg is linked into the module's native shared library. | fact | ticket ("Implement ffmpeg ffmpeg-build on ndk of module `:videolib` by CMakeLists.txt") |
| FR-2 | Confine all changes to module `:videolib`; do not modify other modules (incl. `camera/src/main/cpp/ffmpegv2`). | fact | ticket ("### Scope — just update into module `:videolib`"); packet §0 ground rules |

---

## 4. Actors & User Stories

| Story-ID | FR-ID | Story |
|---|---|---|
| ST-1 | FR-1 | As a `:videolib` maintainer, I want FFmpeg 7.1 linked into the module's native build so downstream native video code can call FFmpeg APIs without a separate build step. `[assumption]` — actor/intent inferred from module role; ticket states the mechanism, not the actor. |

Actor/capability note: `:videolib` is a public library scaffold with no in-repo consumers and unknown
external consumers (registry). No end-user-facing behavior is defined by this ticket. `[fact:code]`

---

## 5. Observable Success Conditions

| SC-ID | FR-ID | Explicit outcome | Evidence/Source |
|---|---|---|---|
| SC-1 | FR-1 | The `:videolib` native build (`src/main/cpp/CMakeLists.txt`) references/links the FFmpeg artifacts produced by `ffmpeg-build/`, and `libvideolib.so` builds successfully with FFmpeg linked in. | ticket |
| SC-2 | FR-2 | No files outside `:videolib` are changed. | ticket |

> Only the explicitly stated outcome (integrate FFmpeg into the CMake build) is recorded as a success
> condition. Whether the module must also *call* an FFmpeg function to prove linkage is not stated in
> the ticket; see SC-5a-1. Functional video decode/encode behavior is out of scope (the current
> native entry point is a `Hello from C++` stub; production video source "does not exist yet" per
> packet §2). `[fact:code]`

### 5a. Proposed edge cases & boundary behavior (best-effort — not normative)

| FR-ID | Edge / boundary case | Expected handling | Status | Source |
|---|---|---|---|---|
| FR-1 | **Dead-code stripping of an unused static library.** Static `.a` archives contribute only referenced symbols. The current stub calls no FFmpeg function, so the linker may strip all of FFmpeg and "successful build" would not prove FFmpeg actually links/loads. | Reference at least one FFmpeg symbol (e.g. `av_version_info()`/`avformat_version()`) from `videolib.cpp` to force linkage during verification. | assumption | code (`videolib.cpp` stub) + linker behavior |
| FR-1 | **ABIs without artifacts.** `ffmpeg-build` produced only `arm64-v8a` + `armeabi-v7a`; `:videolib` sets no `abiFilters`, so a default NDK build also attempts `x86`/`x86_64`, which have no FFmpeg archives → link failure. | Restrict `:videolib` ABIs to the two produced ABIs (mirrors `camera`'s `abiFilters`), or provide artifacts for all built ABIs. | assumption | code (`videolib/build.gradle.kts` has no `abiFilters`; `dist/` has 2 ABIs; `camera/build.gradle.kts` filters to 2) |
| FR-1 | **arm64-v8a static link requires `-Wl,-Bsymbolic`.** Without it the link fails: `R_AARCH64_ADR_PREL_PG_HI21 cannot be used against symbol 'ff_tx_tab_*_float'`. | Add `-Wl,-Bsymbolic` (or equivalent) to the arm64 link flags. | fact | `ffmpeg-build/README.md` "Consumer link note"; `dist/MANIFEST.txt` `consumer_note` |
| FR-2 | **16 KB page alignment regression.** `:videolib` CMake sets no page-size flags today, unlike `camera` (`CMAKE_ANDROID_PAGE_SIZE 16384`, `--page-size=16384`). FFmpeg archives were built `max-page-size=16384`. | Apply matching 16 KB page-size link flags so `libvideolib.so` stays 16 KB-aligned. | assumption | code (`videolib` vs `camera` CMake); `config.sh` `MAX_PAGE_SIZE=16384` |
| FR-1 | **Artifact location is git-ignored.** `ffmpeg-build/dist/` (and `src/`, `build/`) are git-ignored and "produced on demand", so the `.a`/headers may be absent on a clean checkout / CI. | Solution design must decide a committed, CMake-readable location (the README notes `dist/` mirrors `camera/ffmpegv2/<abi>` "for easy reuse at integration"). | unknown | `ffmpeg-build/.gitignore`; `README.md` |

*(No `SC-ID` is assigned to any 5a row; these require user confirmation before becoming normative.)*

---

## 6. Engineering Evidence — Non-normative

### Module impact hypothesis

| Module | Owner/consumer | Dependency evidence | Likely contract | Status/confidence |
|---|---|---|---|---|
| `videolib` | primary owner | Ticket names `:videolib` + its `src/main/cpp/CMakeLists.txt`; scope "just update into `:videolib`" | CMake/native linkage + ABI packaging + build config (`abiFilters`, page size, `ndkVersion`) | fact / high |
| `videolib` (public) | external consumers | Registry: `direct consumers: none`; `external consumers unknown` (public module) | Native/ABI + LGPL static-link licensing obligation on a shipped `.so` | unknown / medium |
| `camera` | NOT changed | Reference-only mirror; ground rule forbids touching `camera/.../ffmpegv2` | none — must stay untouched | fact / high |

### Verification implications

| Module/consumer | Candidate command or check | Reason | Status |
|---|---|---|---|
| `videolib` | `:videolib:assembleDebug` (registry default) | Confirm CMake configures + `libvideolib.so` links FFmpeg | fact |
| `videolib` | Native load / symbol check on a supported ABI (`arm64-v8a`, `armeabi-v7a`) | Confirm the linked `.so` loads and an FFmpeg symbol resolves at runtime (guards against dead-code stripping) | assumption |
| `videolib` | 16 KB alignment check on the produced `.so` | Preserve 16 KB page alignment (matches artifact build + `camera`) | assumption |
| toolchain | NDK `29.0.14206865` present; `ndkVersion` set for `:videolib` | Artifacts built against NDK 29; `:videolib` sets no `ndkVersion` today | assumption |

### Entry points

| Symbol | Role | file:line |
|---|---|---|
| `videolib` CMake target (`add_library(${CMAKE_PROJECT_NAME} SHARED videolib.cpp)`) | native build target to link FFmpeg into | `videolib/src/main/cpp/CMakeLists.txt:27` |
| `target_link_libraries(... android log)` | current link list (FFmpeg to be added) | `videolib/src/main/cpp/CMakeLists.txt:34` |
| `Java_com_cii_videolib_NativeLib_stringFromJNI` | sole native entry point today (stub) | `videolib/src/main/cpp/videolib.cpp:4` |
| `NativeLib` (`System.loadLibrary("videolib")`, `external fun stringFromJNI`) | Kotlin JNI facade | `videolib/src/main/java/com/cii/videolib/NativeLib.kt` |

### Current behavior

| Behavior | Status | Evidence/Source |
|---|---|---|
| `:videolib` builds `libvideolib.so` from `videolib.cpp`, linking only `android` + `log`; no FFmpeg. | fact | `videolib/src/main/cpp/CMakeLists.txt` |
| `videolib.cpp` returns a `"Hello from C++"` string; no video/FFmpeg code exists. | fact | `videolib/src/main/cpp/videolib.cpp` |
| `:videolib` build has `cppFlags -std=c++17`, `minSdk 21`, no `abiFilters`, no `ndkVersion`, no 16 KB page-size flags, JVM 11. | fact | `videolib/build.gradle.kts` |
| Produced artifacts exist locally: `dist/<abi>/lib/{libavcodec,libavdevice,libavfilter,libavformat,libavutil,libswresample,libswscale}.a` + `include/` for `arm64-v8a`, `armeabi-v7a` (FFmpeg 7.1, `components: full`, static, LGPL, `max_page_size 16384`, NDK 29). | fact | `dist/MANIFEST.txt`; on-disk `dist/<abi>/` tree |

### Affected boundaries

| Boundary | Why it matters | Status | Evidence/Source |
|---|---|---|---|
| CMake/native linkage | FR-1 adds imported FFmpeg targets + link entries; static-lib link differs from `camera`'s SHARED-imported `.so` pattern | fact | `videolib` vs `camera` `CMakeLists.txt` |
| ABI packaging | Only 2 ABIs have artifacts; module currently unfiltered | fact | `dist/`; `videolib/build.gradle.kts` |
| Build config (`abiFilters`, page size, `ndkVersion`, external-native args) | Likely `build.gradle.kts` edits alongside CMake to match artifact toolchain | assumption | `camera/build.gradle.kts` precedent |
| JNI (optional) | Proving linkage may require calling an FFmpeg symbol from native code | assumption | stub state + linker behavior |
| Publication / licensing (public module) | Shipping FFmpeg statically in a public lib carries LGPL obligations; `:videolib` does not currently apply the publish plugin (`camera`/`core` do) | unknown | registry (public; external unknown); `config.sh` `LICENSE=lgpl`; `videolib/build.gradle.kts` |

### Reuse candidates

| Candidate | Location | Apparent fit | Confidence |
|---|---|---|---|
| FFmpeg-in-NDK CMake pattern (imported FFmpeg libs, `FFMPEG_ROOT/${ANDROID_ABI}`, 16 KB flags, `abiFilters`) | `camera/src/main/cpp/CMakeLists.txt` + `camera/build.gradle.kts` | High for structure; **differs** — camera imports SHARED `.so` (FFmpeg 3.2.12), this ticket links STATIC `.a` (FFmpeg 7.1) | medium |
| Per-ABI artifact layout `include/` + libs | `dist/<abi>/` mirrors `camera/ffmpegv2/<abi>` | High — README states it mirrors camera "for easy reuse at integration" | high |

---

## 7. Non-functional / Technical Constraints

- **Linkage:** static `.a` + headers (not shared `.so`). Differs from `camera`. `[fact]` `config.sh` `LINKAGE=static`; `dist/MANIFEST.txt`.
- **ABIs:** `arm64-v8a`, `armeabi-v7a` only. `[fact]` `config.sh` `ABIS`; `dist/`.
- **arm64-v8a link flag:** `-Wl,-Bsymbolic` required. `[fact]` README / MANIFEST `consumer_note`.
- **16 KB page alignment:** artifacts built `max-page-size=16384`; module `.so` must remain 16 KB-aligned. `[fact]` `config.sh`; `[assumption]` current `:videolib` CMake lacks the flags.
- **API level / minSdk:** artifacts built at API 21 per ABI, honoring `:videolib` `minSdk 21`. `[fact]` `config.sh`; `videolib/build.gradle.kts`.
- **Toolchain:** artifacts built with NDK `29.0.14206865`; `:videolib` sets no `ndkVersion`. `[fact]` `config.sh`; `videolib/build.gradle.kts`.
- **License posture:** LGPL-safe build (no `--enable-gpl`, no `--enable-nonfree`); static linking into a public library imposes LGPL relink/attribution obligations. `[fact]` `config.sh`; `[unknown]` publication path for `:videolib`.
- **Components:** `full` profile in the produced artifacts (every LGPL decoder/encoder/muxer/demuxer/filter); `slim` allowlist available but not applied. `[fact]` `dist/MANIFEST.txt` `components: full`.

---

## 8. Open Questions, Assumptions & Conflicts

| Item | Classification | Owner | Consequence |
|---|---|---|---|
| Where should the `.a`/headers live for the module build, given `dist/` is git-ignored? (e.g. commit under `videolib/src/main/cpp/ffmpeg/<abi>` mirroring camera, vs point CMake at `ffmpeg-build/dist`) | deferrable | solution-design | Determines CMake paths and whether artifacts are reproducible on CI/clean checkout |
| Should `videolib.cpp`/`NativeLib` be extended to call an FFmpeg symbol to prove linkage, or is a clean link of an (initially unused) FFmpeg sufficient for this ticket? | deferrable | solution-design | Affects whether the JNI/Kotlin surface changes and how SC-1 is verified (dead-code stripping) |
| Should `:videolib` restrict `abiFilters` to the 2 produced ABIs, and set `ndkVersion = 29.0.14206865`? | deferrable | solution-design | Prevents x86/x86_64 link failures and toolchain drift; evidence strongly favors "yes" |
| Does the `full` component set stay, or switch to `slim`? | deferrable | solution-design | `config.sh` already decides `full`; only revisit if size is a stated concern (not stated in ticket) |
| Assumption: no in-repo consumers are affected; external consumers of the public `:videolib` are unknown and out of view. | non-material | — | Recorded for compatibility tracking |
| Conflicts | none | — | The `camera` shared-`.so` vs `ffmpeg-build` static-`.a` difference is a design constraint, not a source conflict |

No blocking gap: every open item has an evidence-backed default or belongs to solution-design; none
changes scope, data ownership, permissions, or a protected boundary in a way that best-effort
assumption cannot safely cover.

---

## 9. Risk Analysis

| Risk | Likelihood/Impact | Affected FR/area | Status | Source |
|---|---|---|---|---|
| x86/x86_64 link failure — no FFmpeg archives for unfiltered ABIs | High / High | FR-1, ABI packaging | assumption | `videolib/build.gradle.kts` (no `abiFilters`) vs `dist/` (2 ABIs) |
| arm64-v8a link failure without `-Wl,-Bsymbolic` | High / High | FR-1, native linkage | fact | README / MANIFEST `consumer_note` |
| "Builds green" but FFmpeg dead-code-stripped → linkage not actually proven | Medium / Medium | SC-1 verification | assumption | static-lib linker behavior + stub state |
| Git-ignored `dist/` absent on clean checkout / CI → build cannot find artifacts | Medium / High | FR-1, reproducibility | fact | `ffmpeg-build/.gitignore` |
| 16 KB alignment regression (module CMake lacks page-size flags) | Medium / High | 16 KB constraint | assumption | `videolib` vs `camera` CMake |
| NDK version drift (`:videolib` has no `ndkVersion`; artifacts built on NDK 29) | Medium / Medium | toolchain | assumption | `videolib/build.gradle.kts`; `config.sh` |
| LGPL static-link obligations on a public library if later published | Low-now / High-if-published | publication/licensing | unknown | `config.sh` `LICENSE=lgpl`; registry (public, external unknown) |
| Accidental edit to `camera/ffmpegv2` or another module violates scope | Low / High | FR-2 | assumption | packet §0 ground rules |

---

_Validation: PASS — every FR is ticket-evidenced (no requirement derived only from code); SC-1/SC-2
map to FRs; success conditions are explicit, not invented; every material statement carries one
epistemic status + source; §6 engineering evidence is non-normative with no solution decision; risks
cite evidence and an affected FR/area; standard depth honored; no placeholders. Coverage —
requirements_with_evidence 2/2, stories_mapped 1/1, success_conditions_mapped 2/2,
material_unknowns_resolved: 0 blocking remaining (all deferrable)._
