AUTOMATION: CONTINUE

# SOLUTION-DESIGN — implement-ffmpeg-to-android-ndk

Integrate the FFmpeg 7.1 static artifacts produced by `videolib/ffmpeg-build/` into the `:videolib`
NDK/CMake build so `libvideolib.so` links FFmpeg. Design only; scope confined to `:videolib`.

Traceability carried from DEV-SPEC: **FR-1** (link FFmpeg via `src/main/cpp/CMakeLists.txt`),
**FR-2** (changes confined to `:videolib`), **SC-1** (FFmpeg linked, `.so` builds), **SC-2** (no
out-of-module changes), **ST-1** (maintainer can call FFmpeg without a separate build step).

---

## 1. Decision ledger

**Investigation depth:** standard — ~14 lookups (DEV-SPEC; `videolib` CMake/gradle/cpp/kt;
`ffmpeg-build` README/config/build-ffmpeg.sh/check-alignment.sh/MANIFEST/on-disk `dist/`; `camera`
CMake/gradle/`ffmpegv2` layout; `settings.gradle.kts`). Stopped after two consecutive lookups added
no material design evidence. Graph absent for these C targets → validated against source.

**Sources / evidence**

| Ref | Evidence | Status |
|---|---|---|
| E1 | Ticket: link `ffmpeg-build` artifacts into `:videolib` via `src/main/cpp/CMakeLists.txt`; scope = `:videolib` only | observed |
| E2 | Produced artifacts on disk: `ffmpeg-build/dist/{arm64-v8a,armeabi-v7a}/{include, lib/*.a}` — 7 static archives (avcodec/avdevice/avfilter/avformat/avutil/swresample/swscale), FFmpeg 7.1, `full` components, LGPL, `max_page_size 16384`, NDK 29 | observed (`dist/MANIFEST.txt`, tree) |
| E3 | Link recipe (authoritative): static archives + `-Wl,-z,max-page-size=16384` (all ABIs) + `-Wl,-Bsymbolic` (arm64-v8a only) + system `-lm -lz`; arm64 without `-Bsymbolic` fails `R_AARCH64_ADR_PREL_PG_HI21 … ff_tx_tab_*` | observed (`check-alignment.sh:63-92`, `build-ffmpeg.sh:94-97`, README) |
| E4 | `:videolib` today: links only `android`+`log`; `videolib.cpp` is a `"Hello from C++"` stub; `build.gradle.kts` has `cppFlags -std=c++17`, `minSdk 21`, **no** `abiFilters`, **no** `ndkVersion`, **no** page-size flags | observed |
| E5 | `dist/` (and `src/`, `build/`) are git-ignored — "produced on demand"; README: `dist/` mirrors `camera/ffmpegv2/<abi>` "for easy reuse at integration" | observed (`ffmpeg-build/.gitignore`, README) |
| E6 | Repo precedent: `camera` commits per-ABI FFmpeg into `src/main/cpp/ffmpegv2/<abi>`, imports them as CMake targets, filters ABIs to the 2 ARM ABIs, sets 16 KB page flags + `ndkVersion 29.0.14206865`. **Difference:** camera imports **SHARED** `.so` (FFmpeg 3.2.12); this ticket links **STATIC** `.a` (7.1) | observed (`camera/src/main/cpp/CMakeLists.txt`, `camera/build.gradle.kts`) |
| E7 | FFmpeg-from-source uses autotools; Gradle `externalNativeBuild`/CMake cannot drive it → the build is out-of-band, artifacts are prebuilt inputs to the module | observed (README) |

**Decisions**

| ID | Decision | Rationale / evidence | Status |
|---|---|---|---|
| D1 | Consume FFmpeg as **prebuilt static artifacts committed into the `:videolib` source tree** at a stable per-ABI path mirroring `camera/ffmpegv2/<abi>` (an `include/` tree + `lib/*.a` per ABI). CMake resolves the path by `${ANDROID_ABI}`. | E5/E6/E7: `dist/` is git-ignored/non-reproducible on CI; committing per-ABI artifacts is the established repo pattern; autotools can't run inside the Gradle build. Point-at-`dist/` is rejected (not reproducible). | proposed |
| D2 | Import each archive as a **STATIC IMPORTED** CMake target (or direct `.a` link) and link into the `libvideolib` SHARED target; honor static inter-archive link order (format/device/filter → codec → swscale/swresample → avutil) plus system `-lm -lz`. | E2/E3: static `.a` unlike camera's SHARED `.so`; static link is order-sensitive; probe links `-lm -lz`. | proposed |
| D3 | Apply **`-Wl,-Bsymbolic` to the arm64-v8a link only**. | E3: documented hard requirement; armeabi-v7a unaffected. | proposed |
| D4 | Keep `libvideolib.so` **16 KB-aligned** via the build's page-size link flags (`-Wl,-z,max-page-size=16384`, matching camera's `CMAKE_ANDROID_PAGE_SIZE 16384` / `--page-size=16384`). | E2/E3/E6: artifacts built at 16 KB; repo/platform requirement. | proposed |
| D5 | Restrict `:videolib` to **`arm64-v8a` + `armeabi-v7a`** (`abiFilters`). | E2/E4/E6: only these ABIs have artifacts; unfiltered build would attempt x86/x86_64 and fail to link. | proposed |
| D6 | Pin **`ndkVersion = 29.0.14206865`** for `:videolib`. | E2/E6: artifacts built with NDK 29; avoids toolchain drift; matches camera. | proposed |
| D7 | Make linkage **observable at runtime**: `:videolib` native code references one FFmpeg symbol (a version/build-info accessor, e.g. `av_version_info()`/`avformat_version()`), surfaced through the existing JNI/Kotlin facade as a version accessor. | E3/E4: static archives contribute only referenced symbols; a stub that calls nothing lets the linker strip all of FFmpeg, so a green build would not prove SC-1. A referenced symbol forces retention and lets a device/ABI run prove linkage. | proposed |
| D8 | Keep FFmpeg **component profile = `full`** as already locked in `config.sh`; do not switch to `slim` in this ticket. | E2: `MANIFEST` records `components: full`; the ticket states no size constraint; profile is a `config.sh`-owned decision. | proposed |

**Assumptions**

| ID | Assumption | Impact if wrong |
|---|---|---|
| A1 | `:videolib` has no in-repo consumers and is not published in this ticket (no publish plugin applied). | If later published, LGPL static-link obligations (relink capability, notices) apply — see §6 (licensing). |
| A2 | FFmpeg C headers are consumed from C++ via their built-in `extern "C"` guards; the existing `-std=c++17` / default STL (`c++_shared`) needs no change to link C archives. | If a symbol needs STL/other system lib, add it at link time; not a structural change. |
| A3 | The committed artifacts are the exact `dist/` output whose provenance is in `MANIFEST.txt` + `SHA256SUMS`. | Mismatched/rebuilt archives could reintroduce the arm64 relocation or alignment issues; SHA256SUMS is the integrity gate. |

**Blockers:** none. Every DEV-SPEC open item resolves to an evidence-backed decision above.

**Intentionally unspecified (implementation-local):** exact committed directory name and CMake
target names; whether archives are imported as named STATIC IMPORTED targets vs. linked by path;
exact JNI method name/signature for the version accessor (D7); whether to reuse `stringFromJNI` or
add a sibling `external fun`; the precise system-lib list beyond `-lm -lz` if the linker requires
more.

---

## 2. Behavior and state transitions

This is a build/link-integration change; observable behavior is the build outcome plus one runtime
linkage probe. No user-facing state machine.

**Behavior contract**

| FR-ID | SC-ID | AC-ID | Story-ID | rule/trigger | observable outcome | failure/recovery |
|---|---|---|---|---|---|---|
| FR-1 | SC-1 | AC-1 | ST-1 | `:videolib` native build (CMake) links the committed FFmpeg static archives into `libvideolib.so` for each enabled ABI | `:videolib:assembleDebug` produces `libvideolib.so` with FFmpeg linked, per ABI | Missing/renamed archive or wrong path → CMake configure/link error; fix path (D1) |
| FR-1 | SC-1 | AC-2 | ST-1 | `:videolib` native code references ≥1 FFmpeg symbol, surfaced via the JNI/Kotlin facade | Loading `videolib` + invoking the accessor returns an FFmpeg-provided version string on a supported ABI | Symbol unresolved → `UnsatisfiedLinkError`/link error, exposing a stripped/broken link instead of a silent pass (D7) |
| FR-1 | SC-1 | AC-3 | — | arm64-v8a link applies `-Wl,-Bsymbolic` | arm64-v8a `.so` links successfully | Without the flag: `R_AARCH64_ADR_PREL_PG_HI21 … ff_tx_tab_*` link failure (E3) |
| FR-2 | SC-1 | AC-4 | — | Build applies 16 KB page-size link flags | Every LOAD segment of `libvideolib.so` aligns ≥ `0x4000` | Alignment < 16 KB on newer devices → load/packaging risk (D4) |
| FR-1 | SC-1 | AC-6 | — | `:videolib` builds only `arm64-v8a`, `armeabi-v7a` | No x86/x86_64 link attempt against absent artifacts | Unfiltered ABI set → x86/x86_64 link failure (D5) |
| FR-2 | SC-2 | AC-5 | — | Only `:videolib` files change | `camera/**` (incl. `ffmpegv2`) and all other modules unchanged | Any out-of-module edit violates scope (E1) |

**State model** (build-time; minimal)

| state | meaning/invariants | permitted events | prohibited/ignored |
|---|---|---|---|
| artifacts-absent | committed per-ABI tree missing/incomplete | commit artifacts (D1) | building a release from this state |
| linked | `.so` links FFmpeg for enabled ABIs, 16 KB-aligned, arm64 `-Bsymbolic` applied | assemble; runtime version probe (AC-2) | enabling an ABI without artifacts (AC-6) |

No transition table beyond artifacts-absent → linked (event: commit artifacts + configure CMake per
D1–D6); no runtime lifecycle, teardown, or concurrency is introduced by this ticket.

---

## 3. Components and responsibilities

**Module Contract Matrix**

| module | owner/consumer | responsibility | depends on | crossed contract | compatibility obligation | verification obligation |
|---|---|---|---|---|---|---|
| `videolib` | primary owner | Own the CMake linkage, ABI packaging, and build config that bind FFmpeg static archives into `libvideolib.so`; own the committed artifact tree; expose a linkage-proof accessor | committed FFmpeg 7.1 static artifacts (build input, E2) | CMake/native linkage; ABI packaging; JNI/Kotlin facade; (deferred) LGPL static-link licensing | Native/ABI: preserve `System.loadLibrary("videolib")` + existing JNI symbol; 16 KB alignment; ARM-only ABIs. Public module, external consumers unknown → additive only | `:videolib:assembleDebug`; supported-ABI native-load + version-probe run |
| `camera` | not changed | — | — | must stay untouched (separate FFmpeg 3.2.12 SHARED tree) | none | n/a — confirm no diff |
| others | not changed | — | — | none | none | n/a |

**Components**

| component role | observed/proposed | responsibility / owned state | delegates to | dependency direction | must not own/know | evidence/decision |
|---|---|---|---|---|---|---|
| FFmpeg static artifact tree (per-ABI `include/` + `lib/*.a`) | proposed (committed input) | Provide FFmpeg 7.1 headers + static archives for the two ARM ABIs; carry provenance (MANIFEST/SHA256SUMS) | — | consumed by CMake | build logic, runtime behavior | D1, E2, E5 |
| `:videolib` CMake build (`src/main/cpp/CMakeLists.txt`) | observed→changed | Import the archives, link them into the `videolib` SHARED target in correct static order with `-Wl,-Bsymbolic` (arm64) + 16 KB page flags + `-lm -lz` | NDK linker | depends on artifact tree | encode/decode logic; ABI filter policy (Gradle owns) | D2–D4, E3 |
| `:videolib` Gradle build (`build.gradle.kts`) | observed→changed | Own `abiFilters` (2 ARM ABIs), `ndkVersion`, external-native page-size args; keep `minSdk 21`, `-std=c++17` | AGP externalNativeBuild | depends on CMake | CMake link details | D5, D6, E4, E6 |
| `videolib.cpp` native entry | observed→changed | Reference one FFmpeg symbol; return its version/build info through JNI | FFmpeg (`libavutil`/`libavformat`) | depends on linked archives | video pipeline behavior (out of scope) | D7, E4 |
| `NativeLib` (Kotlin JNI facade) | observed→maybe extended | Expose the linkage-proof accessor (reuse `stringFromJNI` or add a sibling `external fun`); keep `System.loadLibrary("videolib")` | `videolib.cpp` via JNI | consumer of native | native linkage details | D7, E4 |

Decomposition is intentionally minimal: no new module, no domain/data/UI layer, no parallel
abstraction — this extends the existing scaffold's build + JNI seam only.

---

## 4. End-to-end data flow

**Flow F1 — build & link (artifacts-absent → linked)**

| step | participant | input/source | decision/transformation | output/side effect | error propagation |
|---|---|---|---|---|---|
| 1 | out-of-band FFmpeg build | pinned FFmpeg 7.1 source | cross-compile per ABI (already done) | `dist/<abi>/{include,lib/*.a}` (E2) | build-host failure; out of this ticket |
| 2 | integrator (impl stage) | `dist/<abi>` + `SHA256SUMS` | verify checksums (A3), place under committed module path (D1) | per-ABI artifact tree in `:videolib` | checksum mismatch → do not commit |
| 3 | `:videolib` CMake | committed tree, `${ANDROID_ABI}` | import archives; set link order; add `-Bsymbolic` (arm64), 16 KB + `-lm -lz` (D2–D4) | link inputs for `libvideolib.so` | missing path/flag → configure/link error (AC-1/AC-3) |
| 4 | AGP externalNativeBuild | CMake config + `abiFilters` (D5) | build only the 2 ARM ABIs | `libvideolib.so` per ABI in AAR/APK | absent-ABI attempt → link failure (AC-6) |
| 5 | packaging | linked `.so` | verify 16 KB LOAD alignment (D4) | 16 KB-aligned `.so` | sub-16 KB → alignment failure (AC-4) |

**Flow F2 — runtime linkage proof**

| step | participant | input/source | decision/transformation | output/side effect | error propagation |
|---|---|---|---|---|---|
| 1 | consumer/test | `NativeLib` | `System.loadLibrary("videolib")` | native lib loaded (dynamic FFmpeg symbols now resident, statically) | load failure surfaces packaging/linkage break |
| 2 | `NativeLib` accessor | — | JNI call into `videolib.cpp` → FFmpeg version symbol (D7) | FFmpeg version/build string returned to Kotlin (AC-2) | `UnsatisfiedLinkError` / null → linkage/strip defect exposed |

---

## 5. Boundary contracts

| contract / boundary | observed/proposed/blocked | semantic input | output/result | invariants | errors | compatibility/versioning | owner |
|---|---|---|---|---|---|---|---|
| CMake ↔ FFmpeg static archives | proposed | per-ABI `.a` set + headers at `${ANDROID_ABI}` path | archives linked into `libvideolib.so` | static link order (format/device/filter → codec → sws\* → avutil); `-lm -lz`; arm64 `-Wl,-Bsymbolic`; 16 KB page flags | unresolved symbol / relocation / missing archive | FFmpeg 7.1 soname-less static ABI; header API is 7.1 (post-5.x: `AVChannelLayout`, `av_frame_get_buffer(f,0)` era) — do **not** copy camera's 3.2.12 idioms | `:videolib` |
| Gradle ↔ NDK/ABI packaging | proposed | `abiFilters`, `ndkVersion`, page-size args | 2 ARM ABIs built; 16 KB-aligned `.so` packaged in AAR | only `arm64-v8a`+`armeabi-v7a`; NDK `29.0.14206865` | ABI without artifact; NDK drift | additive to scaffold; narrows ABI set (scaffold had none built with source yet) | `:videolib` |
| JNI ↔ Kotlin facade | observed→proposed | native method matched by exact `Java_com_cii_videolib_NativeLib_*` symbol | version string (AC-2); existing `stringFromJNI` preserved | name-mangled link (no `RegisterNatives`); keep `System.loadLibrary("videolib")` | `UnsatisfiedLinkError` on name mismatch | additive: keep existing `stringFromJNI` signature; any new `external fun` is source-additive | `:videolib` |
| Module scope | observed | change set | diff limited to `:videolib` | `camera/ffmpegv2` and other modules untouched (SC-2) | out-of-module edit | no change to other modules' contracts | project |
| LGPL static-link (deferred) | proposed/blocked-if-published | — | relink capability + notices if distributed | LGPL-safe build (no gpl/nonfree) preserved | distributing without relink means | not triggered while unpublished (A1); becomes a release gate on first publish | `:videolib` maintainer |

---

## 6. Conditional cross-cutting design

**Native/toolchain (triggered — `ndk-cpp`/`ffmpeg`/`native-boundary`).**
- Static-archive linkage differs from camera's SHARED-IMPORTED `.so`; camera's soname pins, loader
  order, and vendored-`.so` packaging do **not** transfer. `:videolib` needs no multi-`.so` load
  order — FFmpeg is absorbed into `libvideolib.so`, so only `System.loadLibrary("videolib")` is
  required (no `JNILibraryLoader` equivalent).
- FFmpeg **7.1** API era ≠ camera's 3.2.12. Implementation must use 7.x header idioms; the FFmpeg
  guideline's 3.2.12 alignment/`AVChannelLayout` cautions describe the *other* tree and must not be
  applied here.
- Keep `-std=c++17` (already set); FFmpeg C headers are `extern "C"`-guarded (A2). STL choice
  unchanged (intentionally unspecified) — C archives impose no STL requirement.

**ABI / packaging (triggered — `gradle-module`).** `abiFilters` narrowed to the 2 produced ABIs
(D5); 16 KB alignment enforced at link (D4); `ndkVersion` pinned (D6). No `jniLibs.srcDir` is needed
(static link, not bundled `.so`) — do not copy camera's vestigial `jniLibs` pattern.

**Security / privacy / performance.** No network, permissions, or data handling introduced. App-size
impact: static link pulls in only referenced FFmpeg symbols, but with `full` archives + a
version-only reference the retained set is small; real size grows when actual decode/encode code is
added (future ticket). Committed archive size in-repo is significant (arm64 `full` ≈ 30 MB) — an
accepted tradeoff consistent with camera committing its FFmpeg `.so` (D1/D8).

**Licensing (triggered — public module + static LGPL).** Static linking of LGPL FFmpeg into a
distributed `.so` obliges providing relinkable objects/instructions + notices. Not triggered while
`:videolib` is unpublished (A1); recorded as a first-publish release gate, not a blocker here.

**Architectural verification obligations (proof levels).**
- Native/Gradle compile + AAR inspection prove: archives link, symbols resolve, 2-ABI packaging,
  16 KB alignment (AC-1, AC-3, AC-4, AC-6). Command: `:videolib:assembleDebug`.
- Only a supported-ABI device/emulator run proves: `System.loadLibrary("videolib")` loads and the
  FFmpeg version accessor returns a real string (AC-2). Never claim AC-2 from compilation alone.
- `SHA256SUMS` verification proves artifact integrity (A3).

---

## 7. Coverage audit

| Item | Resolved by |
|---|---|
| FR-1 (link FFmpeg via CMake) | D1–D4, AC-1/AC-3/AC-4, §4 F1, §5 CMake contract |
| FR-2 (scope `:videolib` only) | AC-5, §3 Module Contract Matrix, §5 Module-scope contract |
| SC-1 (FFmpeg linked, `.so` builds) | AC-1, AC-3, AC-4, AC-6 |
| SC-2 (no out-of-module change) | AC-5 |
| ST-1 (call FFmpeg without separate build step) | D7, AC-2, §4 F2 |
| DEV-SPEC open: artifact location (git-ignored `dist/`) | D1 (commit per-ABI tree, mirror camera) |
| DEV-SPEC open: call an FFmpeg symbol to prove linkage? | D7 (yes — version accessor; AC-2) |
| DEV-SPEC open: restrict ABIs + set ndkVersion? | D5, D6 |
| DEV-SPEC open: full vs slim components | D8 (keep `full`, config-owned) |
| DEV-SPEC risk: arm64 `-Bsymbolic` | D3, AC-3 |
| DEV-SPEC risk: 16 KB alignment | D4, AC-4 |
| DEV-SPEC risk: x86/x86_64 link failure | D5, AC-6 |
| DEV-SPEC risk: LGPL static-link | §6 licensing, §5 (deferred), A1 |

**Unresolved inputs needed to complete the design:** none. All items resolve to decisions or
evidence-backed, implementation-local unspecified choices (§1). No blocker → `AUTOMATION: CONTINUE`.

_Traceability preserved: FR-1/FR-2 → SC-1/SC-2 → AC-1…AC-6; Story ST-1 mapped (AC-2). No backlog,
sequencing, task ownership, estimates, file-by-file edits, or test cases defined — those belong to
implementation-plan._
