AUTOMATION: CONTINUE

# INTEGRATION-TEST-REPORT — build-ffmpeg-for-android

Module integration verification for the `:videolib` FFmpeg-from-source build tooling. Executes the
two handed-off checks (T4/Check-1 build, T5/Check-2 16 KB alignment). No production or test source
was edited. Predecessor `CHANGESET.md` begins `AUTOMATION: CONTINUE`.

**Predecessor note:** the packet flags `missing_reads: ["UNIT-TEST-REPORT.md"]`. The Testing (`/ut`)
stage was intentionally skipped — CHANGESET §5 records `testing = n/a` because no JVM/Android
unit-testable symbol was added (build tooling only). Recorded as a **non-blocking process gap** (§5),
not a STOP: all release-relevant verification for this ticket is native/build, executed below.

## 1. Scope reconciliation

- **Changed module:** `:videolib` (only). **Primary owner:** `:videolib`.
- **Changed paths (actual diff):** new dir `videolib/ffmpeg-build/` (`config.sh`, `build-ffmpeg.sh`,
  `check-alignment.sh`, `.gitignore`, `README.md`). `git status` shows only `?? videolib/ffmpeg-build/`.
- **Registry vs source edges:** `:videolib` has no in-repo project consumers (`settings.gradle.kts`
  includes it; composite list empty) — confirmed. Runtime `:videolib` files and all of `camera/`
  unchanged (collision keys `videolib-runtime`, `camera` respected).
- **External consumers:** unknown (public module). The produced FFmpeg **version (7.1)**, **soname/
  archive set**, and **LGPL license posture** become a downstream contract only at the deferred
  integration; kept visible, not executable here.
- **Unexpected scope:** none. `dist/`, `src/`, `build/` are git-ignored (produced on demand).

## 2. Contract matrix

| Contract-ID | boundary/type | producer | consumer(s) | compatibility obligation | evidence required | status |
|---|---|---|---|---|---|---|
| K1 | native/ABI packaging — per-ABI artifact tree (AC-1) | `build-ffmpeg.sh` | deferred `:videolib` integration; external | Produce libs+headers for each declared ABI | Cross-compile runs; `dist/<abi>/{include,lib}` populated | **met** |
| K2 | native/ABI — 16 KB ELF alignment (AC-2) | build ldflag `max-page-size=16384` | 16 KB-page Android devices | Every runtime-loadable ELF LOAD align = 0x4000 | Shared-ELF `readelf -l` per ABI | **met** (both ABIs) |
| K3 | source pin (AC-3) | `config.sh` | integration; external | One FFmpeg tag for all ABIs | Tag recorded + built | **met** (n7.1) |
| K4 | API level vs `minSdk 21` (AC-4) | per-ABI profile | consuming apps | Build API ≥ module minSdk | API 21 both ABIs | **met** |
| K5 | license posture (AC-5) | `configure` flags | external consumers | LGPL-safe (no gpl/nonfree) | Flag inspection + build | **met** |
| K6 | static→shared link contract (consumer-side) | produced `.a` | deferred integration `libvideolib.so` | arm64 archives need `-Wl,-Bsymbolic` to link into a `.so` | Shared-link attempt both ABIs | **met with documented constraint** (see §4) |

## 3. Verification matrix

| Check-ID | module | Contract/Test/risk | exact command | environment | executed | result | evidence |
|---|---|---|---|---|---|---|---|
| Check-1 | `:videolib` | K1/AC-1 | `ANDROID_NDK_HOME=<ndk> ./build-ffmpeg.sh` | macOS 15 (darwin/arm64) + Rosetta; NDK 29.0.14206865; git/make/bash | **yes** | **PASS** (exit 0, ~3m31s) | 14 `.a` (7 per ABI: avcodec/avdevice/avfilter/avformat/avutil/swresample/swscale) + headers under `dist/<abi>/{include,lib}`; no `.so` (static, D6) |
| Check-2a | `:videolib` | K2/AC-2 | `./check-alignment.sh` | same | **yes** | **PASS** (exit 0) | Static-archive mode: 7 `.a`/ABI present; gate reports alignment carried by ldflag |
| Check-2b | `:videolib` | K2/AC-2 (conclusive shared-ELF proof) | link `.so` from `libavutil.a` with `-Wl,-z,max-page-size=16384`, then `llvm-readelf -l` | same | **yes** | **PASS** | armeabi-v7a: all LOAD Align = **0x4000**; arm64-v8a (with `-Bsymbolic`): all LOAD Align = **0x4000** |
| Check-3 | `:videolib` | K3/K4/K5 | source inspection of `config.sh` + `configure` invocation in build log | same | **yes** | **PASS** | tag `n7.1`; `API=21` both ABIs; `--enable-static --disable-shared`; no `--enable-gpl`/`--enable-nonfree` |

## 4. Native/build/package checks (triggered)

**Toolchain (ndk-cpp guideline):** NDK 29.0.14206865 `darwin-x86_64` clang under Rosetta; per-ABI
target triples `aarch64-linux-android21` / `armv7a-linux-androideabi21`; `llvm-ar/nm/ranlib/strip`.
Build emitted only deprecation warnings, no errors.

**FFmpeg packaging (ffmpeg guideline):** This is a **separate from-source FFmpeg 7.1** for `:videolib`
— unrelated to `camera`'s vendored **3.2.12** tree, which was not touched. Soname era differs by
design (static `.a`, not the camera `-57`/`-55` shared sonames).

**16 KB alignment — conclusive:** Static `.a` archives have no LOAD segments, so alignment was proven
by linking a real shared object per ABI with the build's `max-page-size=16384` ldflag and reading its
program headers. **Both ABIs: every LOAD segment Align = 0x4000.** AC-2 satisfied.

**Consumer-side link constraint (K6) — actionable finding:** Linking the **arm64-v8a** static
archives into a `.so` fails without `-Wl,-Bsymbolic`:
`R_AARCH64_ADR_PREL_PG_HI21 cannot be used against symbol 'ff_tx_tab_*_float'` from FFmpeg's aarch64
NEON assembly (`libavutil/aarch64/tx_float_neon.S`, PC-relative refs to local `.rodata` TX tables).
Adding `-Wl,-Bsymbolic` links cleanly (armeabi-v7a is unaffected). This is a **consumer/integration
build-flag requirement**, not a producer defect — the archives are correct. It must be applied by the
deferred JNI/CMake integration ticket when it links FFmpeg into `libvideolib.so`.

**Limitations:** Host cross-compile + static ELF inspection prove linkage, packaging, license flags,
and 16 KB alignment. They do **not** prove FFmpeg runtime behavior on a device — out of scope for this
ticket (deferred integration), never claimed.

## 5. Gaps

| Gap | impact | owner | smallest follow-up | blocks review/release? |
|---|---|---|---|---|
| `/ut` UNIT-TEST-REPORT.md absent | none — no unit-testable symbol (build tooling) | testing stage | n/a (justified in CHANGESET §5) | **no** |
| arm64 static→shared needs `-Wl,-Bsymbolic` | consumer must set one linker flag | deferred integration ticket | add `-Wl,-Bsymbolic` to the future `libvideolib.so` link | **no** (documented; producer artifacts correct) |
| On-device FFmpeg runtime behavior unverified | expected — integration scope | deferred integration ticket | supported-device load test after wiring | **no** for this ticket |

## 6. Integration verdict

**PASS WITH LIMITATIONS** — every required check for this build-tooling ticket executed and passed
(AC-1 build for both ABIs; AC-2 16 KB alignment conclusively proven on both ABIs; AC-3/4/5 confirmed);
the only unexecuted coverage (device runtime) is explicitly deferred-integration scope, and the arm64
`-Wl,-Bsymbolic` link requirement is a documented, non-blocking consumer constraint carried to the
integration ticket.
