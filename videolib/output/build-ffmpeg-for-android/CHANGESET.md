AUTOMATION: CONTINUE

# CHANGESET — build-ffmpeg-for-android

Android-dev execution of the approved plan. Implements the three `android-dev`-owned tasks
**T1→T2→T3** (out-of-band FFmpeg cross-compile tooling for `:videolib`). Integration-testing tasks
**T4/T5** are handed off, not executed. Predecessors verified `AUTOMATION: CONTINUE`
(SOLUTION-DESIGN, IMPLEMENT-PLAN).

**Deferred inputs locked this stage** (`[fact:clarification]`): UPI-1 FFmpeg **7.1** (tag `n7.1`,
AC-3); UPI-3 **LGPL-safe** (AC-5); UPI-2 ABIs **arm64-v8a, armeabi-v7a** (D3); UPI-4 **static `.a`**
(D6).

## 1. Implementation outcome

- **Status:** completed for the android-dev task range **T1–T3**.
- **Deviations:** none material. One planning refinement: T2's C2 output plus supporting files
  realized as a small script set under the planned package `videolib/ffmpeg-build/`
  (`config.sh`, `build-ffmpeg.sh`, `check-alignment.sh`, `.gitignore`, `README.md`) — within the
  bounded planned package the plan left implementation-local (design D1). No scope added/dropped.
- **Not touched (collision key `videolib-runtime`):** `videolib/build.gradle.kts`,
  `videolib/src/main/cpp/CMakeLists.txt`, `videolib/src/main/cpp/videolib.cpp`,
  `videolib/src/main/java/com/cii/videolib/NativeLib.kt`. **Read-only:** `camera/**` (unchanged).
- **Not executed:** the FFmpeg build itself (fetches source, cross-compiles) and the alignment run
  are T4/T5, integration-testing-owned, and require a build host — see §4/§6.

## 2. Actual change manifest

| FR | SC | AC | Work | Task | Change-ID | module | affected consumers/contracts | Design-Ref | planned action | actual path | symbol/config key | diff status | purpose | Test/Check | verification |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| FR-1 | SC-1 | AC-3 | W1 | T1 | C1 | `:videolib` | external consumers unknown (soname/version becomes contract at integration) | none | create (source pin) | `videolib/ffmpeg-build/config.sh` | `FFMPEG_VERSION=7.1`, `FFMPEG_TAG=n7.1`, `FFMPEG_REPO` | added | Pin one FFmpeg tag for all ABIs | — | `bash -n` OK; source inspection |
| FR-1, FR-2 | SC-1, SC-2 | AC-1, AC-4, AC-5 | W1 | T2 | C2 (→C4) | `:videolib` | 16 KB devices; license posture downstream | none | create (orchestrator+profiles) | `videolib/ffmpeg-build/build-ffmpeg.sh` | per-ABI `configure→make→install`; triples aarch64/armv7a; `API=21`; `--enable-static --disable-shared`; `--extra-ldflags=-Wl,-z,max-page-size=16384`; no `--enable-gpl`/`--enable-nonfree`; `--prefix=dist/<abi>` | added | Drive from-source build to C4 layout | Check-1 | `bash -n` OK; source inspection |
| FR-2 | SC-2 | AC-2 | W1 | T3 | C3 | `:videolib` | 16 KB-page Android devices | none | create (alignment gate) | `videolib/ffmpeg-build/check-alignment.sh` | `readelf -l` LOAD `align>=0x4000` over `dist/`; non-zero on misalignment | added | 16 KB alignment gate | Check-2 | `bash -n` OK; source inspection |
| FR-1, FR-2 | SC-1, SC-2 | AC-1, AC-2 | W1 | T2/T3 | C4 | `:videolib` | integration (deferred) | none | produce (artifact tree) | `videolib/ffmpeg-build/dist/<abi>/{include,lib}` (git-ignored) | per-ABI headers + `*.a` | pending build (T4) | The deliverable artifacts | Check-1/2 | not produced — build is T4 |
| — | — | — | W1 | T2 | — (support) | `:videolib` | — | none | create | `videolib/ffmpeg-build/.gitignore`, `README.md` | ignore `src/ build/ dist/`; usage doc | added | Keep produced outputs out of VCS; document run | — | source inspection |

Every planned Change-ID present: C1 completed (T1), C2 completed (T2), C3 completed (T3), C4 pending
its producing build run (T4, integration-testing). No unplanned production path written.

## 3. Task completion

| Task | preconditions | invariants checked | done condition | result | evidence |
|---|---|---|---|---|---|
| T1 | UPI-1 confirmed (7.1) | single tag all ABIs; no `camera/ffmpegv2` change | pinned source params available to build | done | `config.sh` sets `FFMPEG_TAG=n7.1`; `camera/` untouched (git status) |
| T2 | T1; UPI-2/3/4 confirmed; host toolchain expectation | per-ABI isolated toolchain (own triple/cross-prefix); API≥minSdk21 (AC-4); LGPL flags — no gpl/nonfree (AC-5); ABI set = {arm64-v8a, armeabi-v7a} (D3); static linkage (D6); 16 KB ldflag on every ELF (AC-2); layout `<abi>/{include,lib}` (D7); no `videolib-runtime` edit | build driver + profiles exist; a per-ABI run installs to C4 | done (authoring) | `build-ffmpeg.sh` `bash -n` OK; per-ABI `configure` builds `--prefix=dist/<abi>`; `rm -rf` before build enforces no-partial-on-failure |
| T3 | T2 | fails build if any produced ELF LOAD align < 0x4000; covers every ABI/lib; static-archive case handled | alignment check wired as a gate | done (authoring) | `check-alignment.sh` `bash -n` OK; iterates `$ABIS`, exits 1 on FAIL |

## 4. Authorized command results

| Command | scope | outcome | environment |
|---|---|---|---|
| `bash -n config.sh build-ffmpeg.sh check-alignment.sh` | static syntax validation | **OK** (all three) | macOS (darwin/arm64), local |
| `chmod +x *.sh` | make scripts executable | done | local |
| `git status --short videolib/ camera/` | confirm collision-key discipline | only `?? videolib/ffmpeg-build/`; no runtime/camera change | local |
| `shellcheck` | lint | **not run** — not installed | — |
| `./build-ffmpeg.sh` (fetch + cross-compile) | produce C4 artifacts (AC-1) | **not run — integration-testing (T4); requires build host with NDK 29.0.14206865 + network to fetch FFmpeg** | — |
| `./check-alignment.sh` (AC-2) | verify 16 KB alignment | **not run — integration-testing (T5); requires produced dist/** | — |

No compilation/build success is claimed; only static syntax checks were executed.

## 5. Testing Handoff

No `testing`-stage (JVM/Android unit) tasks: no Kotlin/JVM production symbol was added or changed —
this is build tooling. `testing` = **n/a** (justified). All verification is integration-testing
(§6). Runtime FFmpeg behavior on a device remains out of scope (deferred integration ticket).

## 6. Integration Handoff

| integration Task | Check-ID | changed module | affected consumer/external contract | boundary | exact command / check | required environment | blocking policy |
|---|---|---|---|---|---|---|---|
| T4 | Check-1 | `:videolib` | external consumers unknown; produced soname/version (AC-3) + license (AC-5) | native/ABI packaging, build-tooling | `ANDROID_NDK_HOME=<ndk 29.0.14206865> ./build-ffmpeg.sh` → confirm `dist/<abi>/include` + `dist/<abi>/lib/*.a` for arm64-v8a and armeabi-v7a | build host: bash, git, make, NDK 29.0.14206865, network | blocking — AC-1 |
| T5 | Check-2 | `:videolib` | 16 KB-page Android devices | native/ABI (ELF alignment) | `./check-alignment.sh` (uses `llvm-readelf -l`; LOAD align ≥ 0x4000). For static `.a`, alignment is carried by the `max-page-size=16384` ldflag and finally proven when a consumer `.so` links them; set `STRICT_REQUIRE_SO=1` only if a shared `.so` is expected | produced `dist/`; llvm-readelf (NDK) | blocking — AC-2 |

Handing off to integration-testing on `AUTOMATION: CONTINUE`. Note for T5: because linkage is static
(D6), `dist/` contains `.a` archives with no LOAD segments of their own; the gate documents that the
16 KB guarantee is enforced by the build ldflag and conclusively verified when the deferred
integration links these archives into `libvideolib.so`. If the integration stage wants a shared-ELF
alignment proof now, it can build one FFmpeg `.so` and re-run the gate.
