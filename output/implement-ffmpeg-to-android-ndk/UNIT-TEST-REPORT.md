AUTOMATION: CONTINUE

# UNIT-TEST-REPORT — implement-ffmpeg-to-android-ndk

Predecessor `CHANGESET.md` verified `AUTOMATION: CONTINUE`. Flow: impl-flow, guarded. Owned test
contract from the CHANGESET Testing Handoff: **T5 / Test-ID T-LINK** (instrumented, AC-2). One
diff-evidenced regression guard added (`added-by-testing`). No unplanned scope.

Instrumented tests were **executed on a connected device** (SM-F721B, Android 16, `arm64-v8a`) — the
authorized smallest relevant test command for this level. The run also built the `:videolib` NDK
target for **both ABIs**, so it doubles as real evidence for several integration checks (recorded as
observed, not claimed as owned).

---

## 1. Test implementation

| FR-ID | SC-ID | AC-ID | Work-ID | production Task-ID | testing Task-ID | changed production path/symbol | Test-ID | test path/symbol | level | authored status |
|---|---|---|---|---|---|---|---|---|---|---|
| FR-1 | SC-1 | AC-2 | W1 | T4 | T5 | `NativeLib.nativeFFmpegVersion()`; `videolib.cpp` `Java_com_cii_videolib_NativeLib_nativeFFmpegVersion` | T-LINK | `videolib/src/androidTest/java/com/cii/videolib/FfmpegLinkageTest.kt::nativeFFmpegVersion_returnsLinkedFfmpegVersion` | Android instrumented (androidTest) | authored |
| FR-1 | SC-1 | AC-2 | W1 | T4 | T5 | `NativeLib.stringFromJNI()` (unchanged symbol; now co-linked with FFmpeg) | T-LINK-REG | `videolib/src/androidTest/java/com/cii/videolib/FfmpegLinkageTest.kt::stringFromJNI_stillReturnsHello_afterFfmpegLinked` | Android instrumented | authored — `added-by-testing` |

Framework/source-set mirror the existing `ExampleInstrumentedTest` (`AndroidJUnit4`,
`androidx.test.ext.junit.runners`, `org.junit.Assert`), which the module already provides via
`androidTestImplementation(libs.androidx.junit)`.

---

## 2. Coverage matrix

| Test-ID | behavior/transition/error/risk | fixture boundary | assertion scope | coverage status | evidence |
|---|---|---|---|---|---|
| T-LINK | FFmpeg 7.1 static archives are linked into `libvideolib.so` and callable at runtime (AC-2); guards static dead-code-strip risk | none — real native lib on device | accessor is non-null, non-blank, contains the `avformat` marker; exact version text intentionally not asserted (build-dependent) | covered — executed & passed | 2 tests passed on SM-F721B; `av_version_info`/`avformat_version` present as text symbols in the built arm64 `.so` |
| T-LINK-REG | pre-existing `stringFromJNI` contract still holds after FFmpeg co-linkage + library-load path exercised | none — real native lib on device | equals `"Hello from C++"` | covered — executed & passed | same run |

Not owned here (integration-testing, T6): AAR ABI-set inspection (Check-ABI), scope diff
(Check-SCOPE), armeabi-v7a device execution. See §5.

---

## 3. Execution results

| Test-ID/command scope | exact command | environment | executed status | result | failure evidence |
|---|---|---|---|---|---|
| T-LINK + T-LINK-REG (class `FfmpegLinkageTest`) | `./gradlew :videolib:connectedDebugAndroidTest -Dorg.gradle.java.home="/Applications/Android Studio.app/Contents/jbr/Contents/Home" -Pandroid.testInstrumentationRunnerArguments.class=com.cii.videolib.FfmpegLinkageTest` (env: `ANDROID_HOME=~/Library/Android/sdk`, `ANDROID_NDK_HOME=.../ndk/29.0.14206865`) | Device SM-F721B, Android 16, arm64-v8a; NDK 29.0.14206865; JBR 21.0.7 | executed | **BUILD SUCCESSFUL — Finished 2 tests, 0 failures** | — |
| Alignment spot-check (read-only, supporting AC-4) | `llvm-readelf -l …/arm64-v8a/libvideolib.so` | local | executed | all LOAD segments align `0x4000` (16384) | — |
| Symbol presence (read-only, supporting AC-2) | `llvm-nm -D …/arm64-v8a/libvideolib.so \| grep av_version` | local | executed | `T av_version_info`, `T avformat_version` present | — |

Environment note: the initial invocation failed at **project configuration** (unrelated `:camera`
publication plugin requires JVM 21; the default shell JVM is 17). Resolved for this run only by
pointing Gradle at Android Studio's bundled JBR 21 via `-Dorg.gradle.java.home` — **no project file
changed**. This is a pre-existing workspace toolchain condition, not caused by this ticket.

---

## 4. Failed cases and root cause

None. No product failure, test defect, or environment failure in the final run. (The first-attempt
configuration error was an environment/toolchain condition — Java 17 vs. required 21 — resolved by
selecting JBR 21; it is not a defect in the `:videolib` change.)

---

## 5. Gaps and recommendations

| Gap / recommendation | mapped to | owner | note |
|---|---|---|---|
| **Redundant CMake arg** — `ANDROID_LD_FLAGS=--page-size=16384` (T3, `build.gradle.kts`) is reported *unused* by CMake ("Manually-specified variables were not used"). 16 KB alignment is actually delivered by T2's `CMAKE_ANDROID_PAGE_SIZE 16384` + `-Wl,-z,max-page-size=16384` (verified: LOAD align `0x4000`). Recommend review drop the redundant/`ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES` args or migrate to the working flag. | AC-4 / C3 / T3 | android-dev (via review) — not a testing-owned edit | Cosmetic warning only; alignment is correct. Not fixed here to respect owner-stage boundaries. |
| **armeabi-v7a runtime not exercised** — the connected device is arm64-v8a only; the 32-bit archive built successfully but T-LINK ran only on arm64. | AC-2 (armeabi-v7a) | integration-testing (T6) | Build for armeabi-v7a succeeded in the same run; runtime proof on a 32-bit target remains a device gap. |
| **AAR ABI-set + scope inspection** not performed here (owned by T6). | AC-6 / SC-2 (Check-ABI, Check-SCOPE) | integration-testing (T6) | Carried in the CHANGESET Integration Handoff. |

---

## 6. Integration handoff status

The owned testing contract (T5 / T-LINK) is implemented **and executed green** on a supported ABI,
which also provides observed evidence for Check-BUILD (arm64), Check-ALIGN (arm64), and the T-LINK
device run. Integration-testing (T6) still owns: Check-BUILD/ALIGN on armeabi-v7a, Check-ABI (AAR
`jni/`), Check-C1 re-verify, and Check-SCOPE. Handoff proceeds on CONTINUE.

_Authored test source stayed within `videolib/src/androidTest/…`; the only ticket artifact written is
this report. Executed results are distinguished from authored status throughout. `AUTOMATION: CONTINUE`._
