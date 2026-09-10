AUTOMATION: CONTINUE

# CHANGESET — Reuse video-filter component functions

## Scope

- Owning module: `videolib`.
- Changed modules: `videolib` and direct sample consumer `app`.
- Dependency/verification closure: `videolib` public Kotlin source contract → native GLES composer/runtime → `app` project consumer. Unknown external version-1 consumers remain represented by compatibility-mode device coverage.
- Preserved contracts: `VideoFilter` constructor and `VERSION_1`, JNI transport, CMake/Gradle configuration, packaged ABIs, candidate-first atomic appearance replacement, and the existing `vec4` adjustment built-ins.

## Implemented changes

- `videolib/src/main/cpp/gl_program.cpp`
  - Added the reusable version-1 component bundle: `FILTER_HUE_RANGE`, `current_hue`, RGB/HSL conversion, the distinct `vec3 vibranceAdjust` overload, `cubicPulse`, and all six selective-color helpers.
  - Added declaration detection for every newly shared name.
  - Updated `GlProgram::buildGeneration` to inject the complete library bundle only when a filter source declares none of those names. A declaring source remains consumer-owned and receives the pre-change environment, avoiding duplicate top-level declarations.
  - Left the existing generation compile/link and atomic replacement path unchanged.
- `videolib/src/main/java/com/cii/videolib/VideoFilter.kt`
  - Documented the callable shared declarations and the mutually exclusive compatibility mode without changing public API shape.
- `app/src/main/java/com/chiistudio/library/MainActivity2.kt`
  - Reduced `DEMO_VIDEO_FILTER.source` to only the requested `addFilter` declaration.
  - Preserved the exact operation order, numeric arguments, `vec3` vibrance call, HSL round trip, alpha behavior, opacity, and existing attach-time rejection handling.
- `videolib/src/androidTest/java/com/cii/videolib/VideoFilterComponentsInstrumentedTest.kt`
  - Added real Surface/EGL coverage for an addFilter-only source, a consumer-owned compatibility source, and rejection recovery that verifies the prior generation continues presenting frames and a later valid candidate is accepted.

## Verification results

- `git diff --check`: passed.
- `./gradlew :videolib:assembleDebug :videolib:compileDebugAndroidTestKotlin`: not completed. The sandboxed attempt could not access the external Gradle cache; the escalated attempt was interrupted before producing a result. No Gradle verification is currently running.
- `./gradlew :app:assembleDebug`: not run because the producer build gate did not complete.
- Connected `VideoFilterComponentsInstrumentedTest`: not run. Runtime GLSL compilation/rendering, compatibility selection, atomic recovery, and packaged ABI evidence remain pending on supported GLES 3.0 ARM hardware.
- Sample `MainActivity2` device smoke: not run; visual filtered-output confirmation remains pending.

## Remaining verification

1. Run `./gradlew :videolib:assembleDebug :videolib:compileDebugAndroidTestKotlin`.
2. Run `./gradlew :app:assembleDebug`.
3. On supported hardware, run `./gradlew :videolib:connectedDebugAndroidTest -Pandroid.testInstrumentationRunnerArguments.class=com.cii.videolib.VideoFilterComponentsInstrumentedTest` and record `adb shell getprop ro.product.cpu.abi`; repeat for `arm64-v8a` and `armeabi-v7a` when devices are available.
4. Launch `MainActivity2`, select a video, and confirm filter acceptance plus visible nonblank filtered output.

No commit, push, publication, signing, upload, or distribution action was performed.
