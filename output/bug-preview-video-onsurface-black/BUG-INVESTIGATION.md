AUTOMATION: CONTINUE

# Bug Investigation — black video preview surface

## Classification

- ticket: `bug-preview-video-onsurface-black`
- ticket_priority: unspecified
- technical_severity: Medium
- confidence: high
- reproduction_status: reported; source-confirmed; not independently reproduced on a device during this investigation
- affected_environment: Android sample `:app`, `MainActivity2`, device/API/ABI and selected video format not supplied
- trigger: launch `MainActivity2`, pick a video, and reach the visible `playing video...` state
- observed: the `SurfaceView` remains completely black
- expected: decoded video frames are visible on the preview surface
- status: confirmed
- source_report: `output/bug-preview-video-onsurface-black.md:1-5`

## Summary

The sample app paints an opaque black Android View background over the `SurfaceView` that owns the
native preview surface. The playback request is accepted and the status changes to “playing,” but
the View-hierarchy layer remains black above the separately composited surface, hiding frames that
EGL presents underneath. The narrow fix belongs to `:app`: remove the `android:background` from
`video_surface`. No `:videolib`, JNI, FFmpeg, EGL/GLES, CMake, ABI, or public API change is needed.

## Confirmed Root Cause

1. `app/src/main/res/layout/activity_main.xml:11-20` declares the preview as a `SurfaceView`, and
   line 15 assigns `android:background="@android:color/black"`.
2. Inflating a non-null background clears `PFLAG_SKIP_DRAW`; the installed Android 36 platform
   implementation shows this in
   `/Users/loannth20/Library/Android/sdk/sources/android-36/android/view/View.java:26541-26556`.
3. With drawing enabled, `SurfaceView.draw()` first punches the surface hole and then calls
   `super.draw()` (`.../android/view/SurfaceView.java:687-697`). `View.draw()` paints the background
   as its first drawing step (`.../android/view/View.java:25066-25087`). The opaque black drawable
   therefore repaints the just-cleared region in the View hierarchy and visually covers the
   below-parent SurfaceView buffer.
4. The report's “playing video...” state is set immediately after `VideoPreview.play()` accepts the
   request (`app/src/main/java/com/chiistudio/library/MainActivity2.kt:202-235`); it is not evidence
   that pixels are visible. Native decoded frames are converted to RGBA and submitted to
   `PreviewRenderer::pushFrame` (`videolib/src/main/cpp/video_playback.cpp:402-455`), which draws and
   swaps the EGL window surface (`videolib/src/main/cpp/preview_renderer.cpp:102-134`). Those buffers
   can be valid while the host View background hides them.

The declared opaque background is sufficient to produce the exact observed/expected mismatch for
every rendered frame; no competing code cause is needed to explain the persistent black region.

## Recommended Fix

Remove `android:background="@android:color/black"` from the `video_surface` declaration in
`app/src/main/res/layout/activity_main.xml`. Preserve the existing SurfaceView, holder lifecycle,
JNI/native ownership, decode pipeline, EGL render-thread affinity, and playback callback behavior.

Do not work around the overlay by changing SurfaceView Z order, surface pixel format, EGL alpha,
the shader, or decoded pixel conversion. Those broaden the contract and do not address the View
background that is covering the composed surface.

## Module and Contract Impact

- primary owner: `:app`; evidence is the host-owned layout resource at
  `app/src/main/res/layout/activity_main.xml:11-20`.
- changed modules: `:app` only.
- dependency closure: `:app` consumes `:videolib` through
  `implementation(project(":videolib"))` at `app/build.gradle.kts:45-55`; the fix changes only the
  consumer's presentation resource, so `:videolib` and its external consumers are behaviorally and
  binary unaffected. `:benchmark` targets the app, but no startup/package contract changes.
- public/native/build contracts: no Kotlin/Java public API, JNI name/signature, native ownership,
  CMake linkage, FFmpeg archive, ABI packaging, manifest, or publication change.
- verification closure: compile/package the changed `:app` resource, verify native frame output
  independently with the existing `:videolib` progressive playback device test, then verify the
  actual `MainActivity2` + `SurfaceView` consumer path visually on a supported ARM device.

| Boundary | Owner | Consumers | Compatibility obligation | Risk |
| --- | --- | --- | --- | --- |
| `video_surface` layout presentation | `:app` | `MainActivity2` | SurfaceView's host View layer must remain transparent so the below-parent native surface is visible | Medium runtime/behavior |
| `VideoPreview` public API | `:videolib` | `:app`, unknown external consumers | No source or binary change | None introduced |
| Kotlin → JNI → FFmpeg → EGL/GLES | `:videolib` | `:app`, unknown external consumers | Preserve names, ownership, worker ordering, and render-thread/context affinity unchanged | High-risk boundary traversed, but not edited |
| App package/benchmark target | `:app` | `:benchmark` | Preserve `com.chiistudio.library` and launch behavior | Low; unchanged |

## Change Surface

| Change-ID | existing/new | action | exact path | symbol/resource/config key | responsibility | evidence | collision key |
| --- | --- | --- | --- | --- | --- | --- | --- |
| CHG-01 | existing | remove the opaque black background attribute only | `app/src/main/res/layout/activity_main.xml` | `SurfaceView#video_surface` / `android:background` | leave the host View layer transparent so SurfaceView buffers are composited visibly | layout line 15 plus Android `SurfaceView.draw`/`View.draw` ordering cited above | `app:activity_main:video_surface-background` |
| CHG-02 | new | add a focused instrumentation regression assertion | `app/src/androidTest/java/com/chiistudio/library/MainActivity2SurfaceTest.kt` | `videoSurface_hasNoCoveringViewBackground` | guard the host layout from reintroducing a non-null background | current app test suite has only the generated package-name smoke test at `ExampleInstrumentedTest.kt:16-23` | `app:test:video-surface-background` |

## Fix Tasks

| Task-ID | owner stage (android-dev) | objective | Change-IDs/exact path-symbol scope | preconditions | invariants | done condition | verification | depends on | collision key |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| FIX-01 | android-dev | expose native preview buffers by removing the covering View background | CHG-01; `app/src/main/res/layout/activity_main.xml` → `video_surface/android:background` | retain `SurfaceView` and its ID/constraints | no `:videolib`, JNI, native, Z-order, pixel-format, or lifecycle edits | the attribute is absent and the rest of the layout contract is unchanged | `./gradlew :app:assembleDebug` | — | `app:activity_main:video_surface-background` |

## Testing Tasks

| Task-ID | owner stage (testing) | target behavior/regression | Test-ID | level | target component/contract | fake/fixture boundary | relevant fix Task-ID | depends on | execution expectation |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| TEST-01 | testing | the sample preview SurfaceView has no host View background capable of covering native buffers | T-SURFACE-BG | Android instrumentation | inflate/launch `MainActivity2`, resolve `R.id.video_surface`, assert `background == null` | no native fake; layout-contract assertion only | FIX-01 | FIX-01 | run on an Android emulator/device via `./gradlew :app:connectedDebugAndroidTest -Pandroid.testInstrumentationRunnerArguments.class=com.chiistudio.library.MainActivity2SurfaceTest` |
| TEST-02 | testing | native playback still presents multiple distinguishable frames before completion | T-PLAYBACK-PROGRESSIVE | Android native/instrumentation | existing `ProgressivePlaybackInstrumentedTest` over real JNI, FFmpeg, EGL/GLES, and consumable surface | existing generated video fixture and `PlaybackSurfaceProbe`; no mock decoder/renderer | FIX-01 | FIX-01 | run on a supported ARM EGL/GLES 3.0 device via `./gradlew :videolib:connectedDebugAndroidTest -Pandroid.testInstrumentationRunnerArguments.class=com.cii.videolib.ProgressivePlaybackInstrumentedTest` |

## Integration Tasks

| Task-ID | owner stage (integration-testing) | Check-ID | changed module | affected consumer/external contract | boundary | exact command or device/manual check | blocking policy | depends on |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| INT-01 | integration-testing | I-COMPILE-APP | `:app` | app resource packaging and `:videolib` project dependency | Gradle/resource/native packaging | `./gradlew :app:assembleDebug` | blocking | FIX-01 |
| INT-02 | integration-testing | I-VISIBLE-VIDEO | `:app` | `MainActivity2` consuming public `VideoPreview` and the native Surface/EGL pipeline | app View hierarchy → SurfaceView → JNI/FFmpeg/EGL buffer visibility | On a supported ARM device, launch `MainActivity2`, pick a known-good video with visible motion/colors, confirm non-black moving frames appear while status is playing, then confirm natural completion; repeat after background/foreground and after selecting a second video | blocking; compile or ImageReader-only tests cannot prove SurfaceView composition | FIX-01, TEST-01, TEST-02 |

## Regression Checklist

| Could regress | Causal link to change | Verification | Source/owner |
| --- | --- | --- | --- |
| black placeholder before the first frame is no longer supplied by a View drawable | removal of the black background intentionally leaves the SurfaceView hole transparent; platform SurfaceView has its own black resize/background layer for an opaque below-parent surface | inspect pre-first-frame behavior during INT-02; add a dedicated sibling placeholder only if product UX explicitly requires one, without setting it as the SurfaceView background | `:app` layout; Android `SurfaceView` composition |
| layout inflation/resource packaging | the only production edit is an XML attribute removal | `./gradlew :app:assembleDebug` and T-SURFACE-BG | `:app` |
| visible video orientation/color/progression | host overlay removal exposes the existing buffer; it must not be accompanied by GL/FFmpeg changes | T-PLAYBACK-PROGRESSIVE plus INT-02 | `:videolib` producer, `:app` consumer |
| stop, surface destroy/recreate, retry, and callback terminal semantics | failing path crosses native lifecycle, although the fix does not edit it | retain and run `PlaybackLifecycleInstrumentedTest` if any implementation expands beyond CHG-01/CHG-02; otherwise manually background/foreground in INT-02 | `:videolib` / `MainActivity2` |

## Risks and Unknowns

- The reported device/API/ABI, selected codec/container, logs, and screenshots were not supplied.
  They are not needed to establish the opaque overlay cause, but remain unknown for device-specific
  decoder or driver behavior after the overlay is removed.
- Existing progressive playback tests use an `ImageReader` surface rather than the app's
  `SurfaceView` (`PlaybackSurfaceProbe.kt:115-187`), so they can prove native buffers are produced
  but cannot catch a covering View background. T-SURFACE-BG and I-VISIBLE-VIDEO close that gap.
- If I-VISIBLE-VIDEO still fails after the scoped attribute removal, capture `videolib.playback`,
  `videolib.egl`, and `videolib.gl` logcat plus the terminal status and selected media metadata.
  That would be new evidence for a separate decode/render/driver investigation; do not broaden
  FIX-01 speculatively.
