AUTOMATION: CONTINUE

# INTEGRATION-TEST-REPORT — preview-video-opengles-by-ndk

Dependency-closure verification for the `:videolib` OpenGL ES preview. Both predecessors
(`CHANGESET.md`, `UNIT-TEST-REPORT.md`) begin `AUTOMATION: CONTINUE`. No production/test source changed
in this stage.

## 1. Scope reconciliation

- **Changed modules (actual diff vs plan):** `:videolib` only — matches IMPLEMENT-PLAN / CHANGESET. `git status` confirms the 8 planned touchpoints and nothing else in a runtime module:
  - modified: `videolib/src/main/cpp/videolib.cpp`, `videolib/src/main/cpp/CMakeLists.txt`
  - new: `preview_renderer.{h,cpp}`, `gl_program.{h,cpp}`, `render_thread_executor.h`, `java/.../VideoPreview.kt`, `androidTest/.../PreviewBindingTest.kt`
  - non-runtime: `.aidlc/modules.json` (registry edge fix, DEV-SPEC-recorded), `output/**`, `videolib/output/*.md` (ticket docs)
- **Primary owner:** `:videolib` (native-library, `com.cii.videolib`).
- **Registry vs source edges:** `app → videolib` verified in `app/build.gradle.kts:55`. No other in-repo module depends on `:videolib` (`camera`, `core`, `network` independent).
- **Affected consumers:** `:app` (direct, project dependency) — build-verified. `:benchmark` targets `:app` package only; not affected by an additive library API → not run.
- **External consumers:** `:videolib` is `publicContract: true`, `externalConsumers: unknown`. Change is **additive** (new `VideoPreview` class + new JNI exports; existing `NativeLib` names untouched) → no source/binary break for out-of-repo consumers. Cannot be executed here; kept visible.
- **Unexpected scope:** none.

## 2. Contract matrix

| Contract-ID | boundary/type | producer | consumer(s) | compatibility obligation | evidence required | status |
|---|---|---|---|---|---|---|
| CT-1 | JNI export ↔ Kotlin `external fun` | `videolib.cpp` `Java_com_cii_videolib_VideoPreview_*` | `VideoPreview.kt` | names/signatures match exactly | native compile + symbol inspection | **verified** |
| CT-2 | Native link (EGL/GLESv3 added; FFmpeg static group / `-Bsymbolic` / 16 KB preserved) | `CMakeLists.txt` | `libvideolib.so` (both ABIs) | link succeeds on arm64-v8a + armeabi-v7a; no FFmpeg link regression | `:videolib:assembleDebug` both ABIs + ELF inspection | **verified** |
| CT-3 | Public Kotlin library API (additive) | `:videolib` | `:app` + external | existing `NativeLib` preserved; new API additive | `:app:assembleDebug` + symbol diff | **verified** |
| CT-4 | ABI packaging (arm64-v8a, armeabi-v7a only) | `build.gradle.kts` abiFilters | consumers | both ARM ABIs packaged; no x86 | built `.so` per ABI present in merged_native_libs | **verified** |
| CT-5 | Surface rendering + native lifecycle/threading (AC-1/2/3/4 visible behavior) | `PreviewRenderer` (EGL) | end user | frame/pattern visible; ordered teardown; no leak/crash | supported-device run (T-LOAD execute + IT3) | **not executed (device)** |

## 3. Verification matrix

| Check-ID | module/consumer | Contract/Test/risk | exact command or device check | environment | executed | result | evidence |
|---|---|---|---|---|---|---|---|
| IT1 | videolib | CT-1, CT-2, CT-4 / R2 | `./gradlew :videolib:assembleDebug` | JBR 21.0.7, NDK 29.0.14206865, CMake 3.22.1, SDK present | **yes** | **PASS** | `buildCMakeDebug[arm64-v8a]` + `buildCMakeDebug[armeabi-v7a]` + `compileDebugKotlin` executed; `BUILD SUCCESSFUL in 2s` |
| IT2 | app | CT-3 / R3 | `./gradlew :app:assembleDebug` | as above | **yes** | **PASS** | consumer built + packaged `:videolib` native libs; `compileDebugKotlin`, `packageDebug`; `BUILD SUCCESSFUL in 1s` |
| IT1-sym | videolib | CT-1, CT-3 / R3 | `nm -D libvideolib.so` (arm64-v8a) | llvm/xcrun nm | **yes** | **PASS** | 6 `VideoPreview_native*` exports present **and** existing `NativeLib_stringFromJNI` / `NativeLib_nativeFFmpegVersion` still exported (R3 regression clean) |
| IT1-egl | videolib | CT-2 | `nm -Du libvideolib.so` | as above | **yes** | **PASS** | undefined refs `eglGetDisplay`, `eglCreateContext`, `glClear`, `glClearColor`, `glDrawElements` → EGL/GLESv3 actually linked/used |
| IT1-align | videolib | CT-2 / R2 (16 KB) | `llvm-objdump -p libvideolib.so` | Xcode llvm-objdump | **yes** | **PASS** | every LOAD segment `align 2**14` (16 KB) preserved |
| T-LOAD | videolib | CT-1, CT-5 / AC-4,AC-5,AC-6,R3 | `./gradlew :videolib:connectedDebugAndroidTest` | **no device/emulator** (`adb devices` empty) | **no** | not executed | authored in UNIT-TEST-REPORT; blocker = device availability |
| IT3 | videolib | CT-5 / AC-1,AC-2,AC-3,R1,R5 | supported-device run: RGBA frame visible, test pattern visible, surface destroy→recreate no crash/leak | **no device** | **no** | not executed | GL runtime/visible output unprovable without a GLES 3.0 device |

## 4. Native/build/package checks

- **Toolchain note (pre-existing, not from this change):** `:videolib:assembleDebug` fails at **configuration** under JDK 17 because `:camera` resolves `com.chiistudio:plugin:1.0.0`, which requires JVM 21 ("Dependency requires at least JVM runtime version 21"). Re-run under the Android Studio JBR 21.0.7 (`JAVA_HOME=".../Android Studio.app/Contents/jbr/Contents/Home"`) configures and builds cleanly. This is a repo-wide root-configuration coupling, independent of the `:videolib` diff; flagged for the review stage, not introduced here.
- **ABI:** both `arm64-v8a` and `armeabi-v7a` `libvideolib.so` produced and merged; no x86/x86_64 (abiFilters honored).
- **16 KB alignment:** preserved (§3 IT1-align).
- **FFmpeg static link:** unchanged group/`-Bsymbolic` blocks; `assembleDebug` linked without the aarch64 TX-table relocation error the `-Bsymbolic` guard prevents → still effective.
- **Private-repo env:** `GITHUB_USERNAME/ACCESS_TOKEN/PUBLISH` present (required for configuration); no secret value printed. No publication/upload/sign task run.

## 5. Gaps

| Gap | impact | owner | smallest follow-up | blocks review/release? |
|---|---|---|---|---|
| T-LOAD not executed (no device) | JNI symbol resolution proven statically (IT1-sym) but not at runtime load | integration/QA on device | `./gradlew :videolib:connectedDebugAndroidTest` on a GLES 3.0 device (both ABIs) | **not a code blocker** — symbol presence + link verified statically; runtime load is the remaining confirmation |
| IT3 visual/lifecycle (AC-1/2/3, R1) | Visible rendering + EGL teardown/thread-affinity correctness unverified | QA on device | supported-device preview run | **release-readiness item** — SC-1 (visible frame) is only fully confirmed on device; the review stage decides if it gates release |
| Pre-existing JDK-21 config coupling | any CI/build on JDK 17 fails at config due to `:camera`→`:plugin` | build/review | run builds on JDK 21 (or align plugin target) | environmental; not introduced by this change |

## 6. Integration verdict

**PASS WITH LIMITATIONS** — every executable closure check passed (`:videolib` native build both ABIs, `:app` consumer build, JNI-symbol/EGL-linkage/16 KB ELF inspection, R3 regression symbols intact); the only unexecuted checks (T-LOAD runtime load, IT3 visible rendering/lifecycle) are device-dependent and explicitly non-blocking for code integration — they are the on-device confirmation of SC-1 that no compile can provide, deferred to a GLES 3.0 device run.

---

Guarded closure complete → hand off to review. Device checks (T-LOAD, IT3) remain the outstanding release-readiness confirmation.
