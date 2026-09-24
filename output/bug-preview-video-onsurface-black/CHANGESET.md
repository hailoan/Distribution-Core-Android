AUTOMATION: CONTINUE

# Changeset — black video preview surface

## Implementation Outcome

- outcome: completed
- approved task range: `FIX-01`
- ticket: `bug-preview-video-onsurface-black`
- changed module: `:app`
- deviations: none
- Design-Ref: none — requirement/code-driven

The opaque Android View background was removed from the sample app's preview `SurfaceView`. The
view ID, type, dimensions, constraints, accessibility description, holder lifecycle, public
`VideoPreview` API, JNI/native ownership, FFmpeg pipeline, EGL/GLES implementation, ABI packaging,
and build configuration are unchanged.

## Actual Change Manifest

| ticket | fix Task-ID | Change-ID | owning module | affected consumers/contracts | Design-Ref(s) | planned action | actual path | actual symbol/resource/config key | diff status | purpose | Test-ID/Check-ID | verification |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `bug-preview-video-onsurface-black` | FIX-01 | CHG-01 | `:app` | `MainActivity2`; runtime SurfaceView composition; `:benchmark` package/startup contract unchanged | none — requirement/code-driven | remove the opaque black background only | `app/src/main/res/layout/activity_main.xml` | `SurfaceView#video_surface` / `android:background` | completed: one attribute removed | allow the below-parent native surface buffer to remain visible through the host View hierarchy | T-SURFACE-BG; I-COMPILE-APP; I-VISIBLE-VIDEO | XML parsed by `xmllint`; scoped `git diff --check` passed; Java 21 `:app:assembleDebug` passed |
| `bug-preview-video-onsurface-black` | testing-owned handoff for FIX-01 | CHG-02 | `:app` test source | guards the `MainActivity2` layout contract | none — requirement/code-driven | add the focused instrumentation regression assertion | `app/src/androidTest/java/com/chiistudio/library/MainActivity2SurfaceTest.kt` | `videoSurface_hasNoCoveringViewBackground` | unchanged: correctly deferred to owner stage `testing` | preserve test ownership from the confirmed investigation | T-SURFACE-BG | not run/not implemented in android-dev; handed off as TEST-01 |

## Task Completion

| Task-ID | preconditions | invariants checked | done condition | result | evidence |
| --- | --- | --- | --- | --- | --- |
| FIX-01 | predecessor begins `AUTOMATION: CONTINUE`; confirmed cause and exact XML attribute/path; no task dependency | retained `SurfaceView`, `video_surface`, dimensions, constraints, accessibility, lifecycle, and all `:videolib`/native contracts; no Z-order or pixel-format change | background attribute absent and all other layout behavior unchanged | completed | scoped diff contains exactly one production-line deletion; `xmllint --noout` and `git diff --check` passed; `:app:assembleDebug` passed with Java 21 |

## Module Impact Revalidation

- primary owner: `:app`, because the only changed path is under `app/src/main/res/layout` and is
  inflated by `MainActivity2`.
- changed modules: `:app` only.
- validated dependency closure: `app/build.gradle.kts:55` declares
  `implementation(project(":videolib"))`; `benchmark/build.gradle.kts:31` targets `:app`.
- compatibility: internal/runtime presentation correction only. No source, binary, public API,
  JNI, native/ABI, manifest, package, benchmark-target, or build/publication contract changed.
- verification closure: `:app:assembleDebug` executed successfully; T-SURFACE-BG,
  T-PLAYBACK-PROGRESSIVE, and the real-device I-VISIBLE-VIDEO check remain downstream.
- external `:videolib` consumers: unaffected because neither library source nor packaged API changed.

## Authorized Command Results

| command | scope | outcome | environment |
| --- | --- | --- | --- |
| `git diff --check -- app/src/main/res/layout/activity_main.xml` | changed production resource | passed | local workspace |
| `xmllint --noout app/src/main/res/layout/activity_main.xml` | XML well-formedness | passed | local macOS toolchain |
| `./gradlew :app:assembleDebug` | smallest owning-module compile/package check | blocked before task execution: root configuration resolved `com.chiistudio:plugin:1.0.0`, which requires JVM 21, while Gradle used JVM 17 | OpenJDK 17.0.16 |
| `JAVA_HOME='/Applications/Android Studio.app/Contents/jbr/Contents/Home' ./gradlew :app:assembleDebug` | same owning-module compile/package check, including `:videolib` ARM native dependency packaging | passed: `BUILD SUCCESSFUL`; 57 actionable tasks, 20 executed and 37 up-to-date | Android Studio JBR 21.0.7; existing publication/Kotlin-plugin warnings were non-blocking |

### UI Design Conformance

| Task-ID | Design-Ref | cached digest/screenshot inspected | states compared | source-inspection result | rendered comparison command/result | deviations |
| --- | --- | --- | --- | --- | --- | --- |
| FIX-01 | none — requirement/code-driven | none applicable | reported black preview vs expected visible native frames | exact confirmed covering attribute removed; all sibling layout attributes unchanged | not run in android-dev; real SurfaceView/native-frame visibility requires I-VISIBLE-VIDEO on a supported device | none |

## Testing Handoff

| testing Task-ID | Work-ID/Story-ID/fix Task-ID | AC-ID/risk | Test-ID | level | target component/contract | behavior/transition/error scope | fake/fixture boundary | relevant changed paths/symbols | depends on | execution expectation |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| TEST-01 | FIX-01 | regression: a host View background can cover native SurfaceView buffers | T-SURFACE-BG | Android instrumentation | launched/inflated `MainActivity2`; `R.id.video_surface` layout contract | assert the resolved `SurfaceView.background == null` so the overlay cannot return | no native fake; layout-contract assertion only | `app/src/main/res/layout/activity_main.xml` → `SurfaceView#video_surface` | FIX-01 | create `app/src/androidTest/java/com/chiistudio/library/MainActivity2SurfaceTest.kt`, then run `./gradlew :app:connectedDebugAndroidTest -Pandroid.testInstrumentationRunnerArguments.class=com.chiistudio.library.MainActivity2SurfaceTest` on an Android emulator/device |
| TEST-02 | FIX-01 | regression: visible progressive frames still originate from the unchanged native playback path | T-PLAYBACK-PROGRESSIVE | Android native/instrumentation | existing `ProgressivePlaybackInstrumentedTest` over real JNI, FFmpeg, EGL/GLES, and consumable surface | multiple distinguishable frames precede successful completion | existing generated video fixture and `PlaybackSurfaceProbe`; no mock decoder/renderer | production change is app-only; validates unchanged `VideoPreview.play` native producer contract | FIX-01 | run on a supported ARM EGL/GLES 3.0 device with `./gradlew :videolib:connectedDebugAndroidTest -Pandroid.testInstrumentationRunnerArguments.class=com.cii.videolib.ProgressivePlaybackInstrumentedTest` |

## Integration Handoff

| integration Task-ID | Check-ID | changed module | affected consumer/external contract | boundary | exact command or device/manual check | required environment | blocking policy |
| --- | --- | --- | --- | --- | --- | --- | --- |
| INT-01 | I-COMPILE-APP | `:app` | app resource packaging plus `:videolib` project dependency | Gradle/resource/native packaging | `JAVA_HOME='/Applications/Android Studio.app/Contents/jbr/Contents/Home' ./gradlew :app:assembleDebug` (already passed in android-dev; integration stage should record or re-run per its contract) | Java 21, configured repository credentials/URL, Android SDK/NDK | blocking |
| INT-02 | I-VISIBLE-VIDEO | `:app` | `MainActivity2` consuming public `VideoPreview` and native Surface/EGL playback | app View hierarchy → SurfaceView → JNI/FFmpeg/EGL buffer visibility | On a supported ARM device, launch `MainActivity2`, pick a known-good video with visible motion/colors, confirm non-black moving frames while status is playing and natural completion; repeat after background/foreground and after selecting a second video | supported ARM ABI, EGL/GLES 3.0, installed debug app, known-good local video | blocking; compilation and ImageReader-only tests cannot prove SurfaceView composition |
