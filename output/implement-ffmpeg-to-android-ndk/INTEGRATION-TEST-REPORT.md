AUTOMATION: CONTINUE

# INTEGRATION-TEST-REPORT — implement-ffmpeg-to-android-ndk

Both predecessors verified `AUTOMATION: CONTINUE` (CHANGESET.md, UNIT-TEST-REPORT.md). Flow:
impl-flow, guarded. All checks below were **executed**; source was not modified (read-only stage).

Environment: device SM-F721B (Android 16, arm64-v8a); NDK `29.0.14206865`; Gradle JVM = Android
Studio JBR 21.0.7 (via `-Dorg.gradle.java.home`, no project file changed — see §5 toolchain note).

---

## 1. Scope reconciliation

| Aspect | Result |
|---|---|
| Changed modules (actual diff) | `:videolib` only — `build.gradle.kts`, `src/main/cpp/CMakeLists.txt`, `src/main/cpp/videolib.cpp`, `src/main/java/com/cii/videolib/NativeLib.kt`, new `src/main/cpp/ffmpeg/<abi>/` (artifacts), new `src/androidTest/.../FfmpegLinkageTest.kt` |
| Primary owner | `:videolib` (android-native-library / scaffold) — matches plan & registry |
| Registry vs source edges | Registry: `:videolib` depends on — none; direct consumers — none. Source-verified: no first-party module references `com.cii.videolib`/`:videolib` (grep over app/camera/core/network/benchmark = 0 hits). `settings.gradle.kts` still `include(":videolib")`. |
| Affected in-repo consumers | none |
| External consumers | unknown (public module) — cannot execute here; kept visible for compatibility. Change is additive (new `external fun`; existing `stringFromJNI` + `System.loadLibrary` preserved), so no source/binary break to a hypothetical consumer. |
| Unexpected scope | none — `camera/` (incl. vendored `ffmpegv2`) has 0 diff; no other module touched |

---

## 2. Contract matrix

| Contract-ID | boundary/type | producer | consumer(s) | compatibility obligation | evidence required | status |
|---|---|---|---|---|---|---|
| CT-1 | CMake/native linkage (FFmpeg 7.1 static `.a` → `libvideolib.so`) | `:videolib` CMake | native lib itself | archives link; arm64 `-Bsymbolic` resolves TX-table relocs | per-ABI NDK build + symbol presence | ✅ verified |
| CT-2 | ABI packaging | `:videolib` gradle | AAR/APK consumers | only `arm64-v8a`, `armeabi-v7a` packaged | AAR `jni/` inspection | ✅ verified |
| CT-3 | 16 KB page alignment | `:videolib` CMake | Android 15+ / 16 KB devices | every LOAD align ≥ 0x4000 | `readelf -l` both ABIs | ✅ verified |
| CT-4 | JNI ↔ Kotlin (name-mangled, no RegisterNatives) | `videolib.cpp` | `NativeLib` | export symbol matches `external fun`; existing `stringFromJNI` intact | symbol table + on-device call | ✅ verified |
| CT-5 | Runtime linkage (FFmpeg callable via `System.loadLibrary`) | `libvideolib.so` | `NativeLib.nativeFFmpegVersion()` | load + FFmpeg symbol resolves at runtime | instrumented device run | ✅ verified (arm64); armeabi-v7a proven by build+symbol, not runtime (§5) |
| CT-6 | Committed artifact integrity | `ffmpeg-build` | `:videolib` CMake input | archives byte-match provenance | `shasum -c SHA256SUMS` | ✅ verified |
| CT-7 | Module scope | this change | all other modules | no out-of-`:videolib` edits | git diff | ✅ verified |
| CT-8 | Public-lib source compatibility | `:videolib` public API | external (unknown) | additive only | source inspection | ✅ verified (additive) |

---

## 3. Verification matrix

| Check-ID | module/consumer | Contract/Test/risk | exact command or check | environment | executed | result | evidence |
|---|---|---|---|---|---|---|---|
| Check-BUILD | :videolib | CT-1, AC-1/AC-3 | `./gradlew :videolib:assembleDebug -Dorg.gradle.java.home="$JBR21"` | NDK 29, JBR 21 | yes | **BUILD SUCCESSFUL** | `buildCMakeDebug[arm64-v8a]` + `[armeabi-v7a]` both built; arm64 `-Bsymbolic` link succeeded (no `R_AARCH64_ADR_PREL_PG_HI21` error) |
| Check-ABI | :videolib | CT-2, AC-6 | `unzip -l videolib-debug.aar \| grep jni/` | local | yes | **only** `jni/arm64-v8a/`, `jni/armeabi-v7a/` | AAR listing |
| Check-ALIGN | :videolib | CT-3, AC-4 | `llvm-readelf -l libvideolib.so` (both ABIs from AAR) | local | yes | all LOAD segments `0x4000` on **both** ABIs | readelf output (arm64: 3 LOAD @0x4000; armeabi-v7a: 3 LOAD @0x4000) |
| Check-SYM | :videolib | CT-1/CT-4/CT-5 | `llvm-nm` pre-strip merged `.so` (both ABIs) | local | yes | `av_version_info`, `avformat_version`, both `Java_com_cii_videolib_*` = `T` on **both** ABIs | nm output |
| T-LINK (exec) | :videolib | CT-5, AC-2 | `./gradlew :videolib:connectedDebugAndroidTest -P…class=…FfmpegLinkageTest` | SM-F721B arm64 | yes (in UT stage; re-confirmed) | **2 tests passed** | UNIT-TEST-REPORT §3 |
| Check-C1 | :videolib | CT-6 | `shasum -a 256 -c SHA256SUMS` (committed libs) | local | yes | **14/14 OK** | shasum output |
| Check-SCOPE | all modules | CT-7, SC-2/AC-5 | `git status --porcelain` filtered | local | yes | all changes under `videolib/` (+ `output/`); `camera/` diff = 0 lines | git status |
| Check-CONSUMER | first-party modules | CT-8 | grep for `:videolib`/`com.cii.videolib` in app/camera/core/network/benchmark | local | yes | 0 references → no in-repo consumer to break | grep |

---

## 4. Native/build/package checks (triggered — native-boundary/ffmpeg/ndk-cpp/gradle)

- **Static-link correctness (both ABIs):** NDK build succeeded for `arm64-v8a` and `armeabi-v7a`.
  The arm64 `-Wl,-Bsymbolic` guard is proven load-bearing-and-satisfied (link produced a valid `.so`
  and it loaded/ran on-device). FFmpeg text symbols (`av_version_info`, `avformat_version`) are
  present in both `.so`, confirming the static archives were actually pulled in (not stripped).
- **Packaging:** FFmpeg is absorbed into `libvideolib.so` (static). The AAR ships exactly one `.so`
  per the 2 ARM ABIs — no separate FFmpeg `.so`, no `jniLibs` double-packaging (correctly unlike
  camera's SHARED model).
- **16 KB alignment:** delivered by `CMAKE_ANDROID_PAGE_SIZE 16384` + `-Wl,-z,max-page-size=16384`;
  verified on both ABIs.
- **Toolchain:** built with the artifact-matching NDK `29.0.14206865` (module now pins `ndkVersion`).

**Limitations:** runtime `T-LINK` executed on arm64-v8a only (no 32-bit device attached); armeabi-v7a
is proven by successful build + symbol presence, not by a runtime call.

---

## 5. Gaps

| Gap | impact | owner | smallest follow-up | blocks review/release? |
|---|---|---|---|---|
| armeabi-v7a **runtime** linkage not exercised | low — build + symbol table confirm the link; only on-device call unproven for 32-bit | integration-testing (future) | run `connectedDebugAndroidTest` on an armeabi-v7a device/emulator | **No** — not a release blocker; both ABIs build and link correctly |
| Redundant CMake arg `ANDROID_LD_FLAGS=--page-size=16384` (T3) reported *unused* by CMake | none functional — alignment holds via the CMake page-size path (verified) | android-dev (via review) | drop the unused `ANDROID_LD_FLAGS`/`ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES` args | **No** — cosmetic warning; carried from UT report §5 |
| Toolchain: workspace default JVM is 17 but `:camera`'s publish plugin (unrelated) requires JVM 21 | build must use a 21+ JVM | project (pre-existing) | use JBR 21 / set `org.gradle.java.home` | **No** for this ticket — condition pre-exists; `:videolib` itself builds under either once a 21 JVM is selected at the daemon level |
| External (out-of-repo) consumers of public `:videolib` | cannot execute here | review | source-compat inspection (done: additive) | **No** — change is additive |

---

## 6. Integration verdict

**PASS WITH LIMITATIONS** — every required contract (CT-1…CT-8) has an executed, passing check
(build both ABIs, ABI packaging, 16 KB alignment both ABIs, FFmpeg symbol linkage both ABIs, runtime
call on arm64, artifact integrity, module scope, no in-repo consumer break); the only unexecuted item
is armeabi-v7a *runtime* (no 32-bit device), which is explicitly non-blocking because the 32-bit build
links correctly and carries the FFmpeg symbols. Hand off to review on CONTINUE.

_One artifact written (this report). No production/test source changed; no publish/sign/commit/push
performed. Exact commands, environment, and limitations recorded without overstating coverage.
`AUTOMATION: CONTINUE`._
