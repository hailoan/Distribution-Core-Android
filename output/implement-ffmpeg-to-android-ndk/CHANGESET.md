AUTOMATION: CONTINUE

# CHANGESET — implement-ffmpeg-to-android-ndk

Predecessors verified `AUTOMATION: CONTINUE` (SOLUTION-DESIGN.md, IMPLEMENT-PLAN.md). Flow: impl-flow,
guarded. Owner stage: android-dev (tasks **T1–T4**). T5 (testing) and T6 (integration-testing) are
handed off, not executed here.

---

## 1. Implementation outcome

- **Status:** completed — all android-dev tasks (T1, T3, T2, T4) implemented in DAG order.
- **Approved task range:** T1–T4. T5/T6 not owned by this stage.
- **Deviations:** none material. One design-sanctioned, implementation-local choice made explicit:
  the linkage-proof accessor is a **new** `nativeFFmpegVersion()` (SOLUTION-DESIGN D7 permitted either
  reusing `stringFromJNI` or adding a sibling); a sibling was chosen to preserve the existing
  `stringFromJNI` symbol contract unchanged. Committed artifact directory named
  `videolib/src/main/cpp/ffmpeg/` (plan §1 latitude). CMake links FFmpeg via a `--start-group`
  link group (robust static cross-reference resolution) — a mechanism detail within T2's approved
  invariants, not a scope change.

---

## 2. Actual change manifest

| FR | SC | AC | Work | Task | Change-ID | module | affected consumers/contracts | Design-Ref | planned action | actual path | actual symbol/config key | diff status | purpose | Test/Check-ID | verification |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| FR-1 | SC-1 | AC-1 | W1 | T1 | C1 | videolib | external (public, unknown) | none | migrate artifacts | `videolib/src/main/cpp/ffmpeg/{arm64-v8a,armeabi-v7a}/{include,lib/*.a}` | 14 static archives (7/ABI) + headers | added | commit provenance-verified FFmpeg 7.1 static tree | Check-C1 | `shasum -a256 -c` all 14 OK (run) |
| FR-1 | SC-1 | AC-1,AC-3,AC-4 | W1 | T2 | C2 | videolib | CMake/native linkage | none | modify | `videolib/src/main/cpp/CMakeLists.txt` | STATIC IMPORTED `avdevice/avfilter/avformat/avcodec/swscale/swresample/avutil`; `-Wl,--start-group`; arm64 `-Wl,-Bsymbolic`; `-Wl,-z,max-page-size=16384`; `z m`; `${ANDROID_ABI}` path | modified | link FFmpeg archives into `libvideolib.so` | Check-BUILD, Check-ALIGN | source inspection; build not run — T6 |
| FR-1,FR-2 | SC-1 | AC-4,AC-6 | W1 | T3 | C3 | videolib | ABI packaging/toolchain | none | modify | `videolib/build.gradle.kts` | `ndk.abiFilters += [arm64-v8a, armeabi-v7a]`; `android.ndkVersion = "29.0.14206865"`; cmake args `ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON`, `ANDROID_LD_FLAGS=--page-size=16384`; kept `minSdk 21`, `-std=c++17` | modified | 2-ABI packaging + 16 KB + NDK pin | Check-ABI, Check-ALIGN | source inspection; build not run — T6 |
| FR-1 | SC-1 | AC-2 | W1 | T4 | C4 | videolib | JNI export | none | modify | `videolib/src/main/cpp/videolib.cpp` | `Java_com_cii_videolib_NativeLib_nativeFFmpegVersion` referencing `av_version_info()` + `avformat_version()`; `stringFromJNI` preserved | modified | force FFmpeg symbol retention; expose version | T-LINK | source inspection; device run — T6 |
| FR-1 | SC-1 | AC-2 | W1 | T4 | C5 | videolib | Kotlin JNI facade | none | modify | `videolib/src/main/java/com/cii/videolib/NativeLib.kt` | `external fun nativeFFmpegVersion(): String`; `System.loadLibrary("videolib")` + `stringFromJNI` unchanged | modified | Kotlin accessor for linkage proof | T-LINK | symbol-sync verified (run); device run — T6 |
| FR-1 | SC-1 | AC-2 | W1 | — | C6 | videolib | — | none | verify (testing-owned) | `videolib/src/androidTest/.../` | instrumented linkage test | **not implemented here** | owned by testing stage (T5) | T-LINK | see Testing Handoff |

All 5 android-dev Change-IDs (C1–C5) completed. C6 belongs to the testing stage (T5).

---

## 3. Task completion

| Task-ID | preconditions | invariants checked | done condition | result | evidence |
|---|---|---|---|---|---|
| T1 | `dist/<abi>` + `SHA256SUMS` present | only 2 ARM ABIs; 7 `.a` + `include/` per ABI; byte-match provenance; `camera/ffmpegv2` untouched | both ABI trees committed; checksums pass | ✅ done | `find`: 2 ABIs × (7 archives + 7 header dirs); `shasum -a256 -c` → 14/14 OK |
| T3 | — | `abiFilters` = 2 ARM only; `ndkVersion 29.0.14206865`; page-size args; `minSdk 21`, `-std=c++17`, namespace, JVM 11 preserved | 3 config keys set; no other module edited | ✅ done | diff of `build.gradle.kts`; `git status` scope clean |
| T2 | T1 tree present | `${ANDROID_ABI}` path; static import; arm64-only `-Bsymbolic`; 16 KB on both; `z m`; keep `videolib.cpp`, `android`, `log`; FFmpeg 7.x idioms | CMake links FFmpeg per invariants | ✅ done (source) | rewritten `CMakeLists.txt`; header/macro availability confirmed |
| T4 | T2 linkable; T1 headers | JNI export name mangles to exact `Java_com_cii_videolib_NativeLib_nativeFFmpegVersion`; matches Kotlin `external fun`; keep `System.loadLibrary` + `stringFromJNI`; return is FFmpeg-provided | native + Kotlin in sync; accessor returns FFmpeg version | ✅ done (source) | symbol-sync grep: C++ exports == Kotlin funs; `av_version_info`/`avformat_version`/`AV_VERSION_MAJOR` confirmed in committed 7.1 headers |

---

## 4. Authorized command results

| Command | scope | outcome | environment |
|---|---|---|---|
| `cp -R` artifacts + `find` count | T1 file placement | 2 ABIs × 7 archives + 7 header dirs | local shell |
| `shasum -a 256 -c` (14 archives) | T1 integrity (Check-C1) | **14/14 OK** | local shell |
| `grep` JNI symbol-sync + header macros | T2/T4 static verification | C++ exports == Kotlin `external fun`; FFmpeg 7.1 symbols/macros present | local shell |
| `git status --porcelain` | scope (Check-SCOPE preview) | changes limited to `videolib/**` (+ `output/`) | local shell |
| `:videolib:assembleDebug` | build (Check-BUILD/ALIGN/ABI) | **not run — authorization required**; owned by integration-testing (T6) | — |
| device/emulator run of T-LINK | runtime linkage (AC-2) | **not run — requires device**; owned by T6 | — |

Smallest relevant next command for the integration stage:
`./gradlew :videolib:assembleDebug` (then AAR `jni/` ABI + `readelf -l` 16 KB align inspection; then run the T5 test on a supported-ABI device).

---

## 5. Testing Handoff

| testing Task-ID | Work-ID | AC-ID/risk | Test-ID | level | target component/contract | behavior/error scope | fake/fixture boundary | relevant changed paths/symbols | depends on | execution expectation |
|---|---|---|---|---|---|---|---|---|---|---|
| T5 | W1 | AC-2; risk: static dead-code strip | T-LINK | Android instrumented (androidTest) | `NativeLib.nativeFFmpegVersion()` → linked FFmpeg | load `videolib`; invoke accessor; assert non-empty FFmpeg-shaped version (e.g. starts with a digit / contains "avformat"); do not assert exact version text; `UnsatisfiedLinkError`/empty ⇒ fail | none — real native lib on device | `videolib/src/main/java/com/cii/videolib/NativeLib.kt` (`nativeFFmpegVersion`), `videolib/src/main/cpp/videolib.cpp` | T4 (done) | device/emulator-dependent; not proven by compile — create `videolib/src/androidTest/.../FfmpegLinkageTest` (C6) |

---

## 6. Integration Handoff

| integration Task-ID | Check-ID | changed module | affected consumer/external contract | boundary | exact command or device/manual check | required environment | blocking policy |
|---|---|---|---|---|---|---|---|
| T6 | Check-BUILD | videolib | external (public, unknown) | CMake/native linkage | `./gradlew :videolib:assembleDebug` | NDK 29.0.14206865 | blocking — AC-1/AC-3 (arm64 `-Bsymbolic` must resolve TX-table relocations) |
| T6 | Check-ALIGN | videolib | packaging | 16 KB alignment | `readelf -l` on each ABI's `libvideolib.so`: every LOAD align ≥ 0x4000 | NDK toolchain | blocking — AC-4 |
| T6 | Check-ABI | videolib | packaging | ABI set | inspect AAR `jni/`: only `arm64-v8a`, `armeabi-v7a` | — | blocking — AC-6 |
| T6 | Check-C1 | videolib | build-input integrity | provenance | `shasum -a 256 -c` over committed libs (already OK at T1) | — | non-blocking — re-verify only |
| T6 | Check-SCOPE | videolib | all other modules | module scope | `git diff --name-only` limited to `videolib/**`; `camera/ffmpegv2` untouched | — | blocking — SC-2/AC-5 |
| T6 | T-LINK (exec) | videolib | runtime | JNI/load | run T5 on supported-ABI device; accessor returns FFmpeg version | device/emulator | blocking — AC-2 (only a device run proves it) |

---

_Handoff on CONTINUE. Source-inspection evidence is kept distinct from executed commands: build and
device runs are explicitly `not run — authorization required` and assigned to T6. JNI symbol names
verified in sync; change scope confined to `:videolib`. `AUTOMATION: CONTINUE`._
