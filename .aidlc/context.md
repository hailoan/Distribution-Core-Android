# Project Context — Distribution Core Android

This file contains source-validated facts and working conventions for the
`Distribution-Core-Android` repository. AI-DLC stages load selected sections through
`.aidlc/lib/stage-context.js`; framework-independent workflow rules remain in
`.aidlc/context-collection.md`.

## 0. Project ground rules

- This is a library-oriented, multi-module Android workspace with a sample app. It is not a
  clean-architecture product application and it has no single feature module. Put changes in the
  narrowest owning module and do not introduce domain/data/UI layers that the local code does not
  use.
- Treat Kotlin public APIs, Hilt bindings, Gradle plugin/publication behavior, JNI declarations,
  exported JNI names, C++ ownership, CMake linkage, ABI packaging, and bundled native libraries as
  cross-boundary contracts. Inspect every side of the relevant contract before editing it.
- The code graph is useful for first-party Kotlin/C++ relationships, but its aggregate view is
  dominated by vendored FFmpeg headers and also includes AI-DLC JavaScript. Validate graph results
  against source and do not treat a missing edge as proof that a library API has no external
  consumers.
- Treat `camera/src/main/cpp/ffmpegv2` as vendored headers and prebuilt binaries. Ignore `build/`,
  `.gradle/`, `.cxx/`, and `.externalNativeBuild/` during source analysis. Modify vendored or
  generated content only for an explicitly scoped upgrade or packaging task.
- Preserve the surrounding implementation style and existing module ownership. Do not combine a
  requested change with opportunistic renames, architectural rewrites, dependency upgrades, or
  publication changes.
- The smallest relevant compile or test is normal verification. Committing, pushing, signing,
  publishing, uploading, or distributing an artifact always requires separate explicit approval.

## 1. What the app is

`Distribution-Core-Android` is a Kotlin-first collection of reusable Android experiments and
libraries, plus a small XML/AppCompat host application. The substantive reusable areas are:

- coroutine-based MVVM/state primitives in `core`;
- Ktor/OkHttp networking and token hooks in `network`; and
- Camera2 NDK capture, JNI, EGL/OpenGL ES filtering, frame capture, and MP4 recording in `camera`.

The repository also contains a Gradle publication plugin, an app startup benchmark, and a newly
scaffolded `videolib` library. Current source is mixed Kotlin, Java, and C++; GLSL assets and vendored
FFmpeg C headers/shared objects support the camera pipeline. There is no Compose UI in source, no
Room/database layer, and no product-level clean-architecture feature stack.

The shared catalog declares Kotlin 2.2.10, AGP 8.9.1, compile SDK 36, target SDK 35, and min SDK 24.
Current module exceptions are: `core` compiles with SDK 35, `videolib` has min SDK 21, and `camera`
uses Java/Kotlin 17 while the other Android modules use JVM 11.

## 2. Modules / structure

- `app` — sample/host APK (`com.chiistudio.library`). It currently depends only on `:network`.
  `MainActivity2` is the launcher and exercises `RetryTokenManager`; `MainActivity` is a separate
  SharedFlow experiment. This module is demonstration code, not the owner of every library.
- `core` — Android library (`com.chiistudio.core`) containing `BaseViewModel`, `ReentrantMutex`, and
  the currently empty `BaseAdapter`. It applies the publication plugin.
- `network` — Android library (`com.chiistudio.network`) containing the Ktor client abstraction,
  token interfaces/refresh helper, a weather-service example, Kotlin serialization dependencies,
  and Hilt wiring.
- `camera` — Android/native library (`com.chiistudio.camerandk`). Kotlin APIs and utilities live
  under `src/main/java`; GLSL fragments under `src/main/assets/glsl`; JNI, Camera2 NDK, EGL/GLES,
  encoder, and threading code under `src/main/cpp`; FFmpeg headers and `.so` files under
  `src/main/cpp/ffmpegv2`. It applies the publication plugin.
- `videolib` — Android/native library (`com.cii.videolib`) scaffold. It has build configuration, an
  empty manifest, generated example tests, and a JNI/CMake native stub: `NativeLib` loads the
  `videolib` shared object and declares `external fun stringFromJNI()`, implemented by a Hello-from-C++
  `videolib.cpp` linked against `android` and `log`. There is no production video source yet.
- `benchmark` — self-instrumenting Android macrobenchmark module targeting `:app` and its
  `benchmark` build type.
- `plugin` — Kotlin/Gradle plugin project implementing `com.chiistudio.plugin` through
  `PublishConfigPlugin`; it configures Maven publications and repositories after evaluation.
- `aidlc-src` and `.aidlc` — AI-DLC toolkit source/runtime support, not Android runtime modules.

Production code belongs under `<module>/src/main`, JVM tests under `src/test`, Android tests under
`src/androidTest`, and Android resources under `src/main/res`. The root includes all seven Gradle
modules directly. The optional composite-build module list in `settings.gradle.kts` is currently
empty, and there are no declared project dependencies among `core`, `camera`, and `videolib` or from
them into `app`.

## 3. Architecture

The repository is organized by reusable module and technical boundary rather than by one global
application architecture. Mirror the owning module:

- `core` implements unidirectional MVVM state flow. `BaseViewModel<S, A, E>` receives actions,
  processes `VMMutation` values sequentially through a `Channel`, exposes durable `StateFlow<S>`,
  and exposes one-shot effects separately. `ReentrantMutex` adds same-coroutine reentrancy around a
  coroutine `Mutex`.
- `network` centers on `IClient`/`BaseClient`. The weather example layers `BearWeatherClient`,
  delegating auth/header wrappers, and `BearWeatherService`, with a Hilt module and qualifiers.
  `InitNetwork` is a process-global holder for consumer-supplied token callbacks. This area is
  partially scaffolded; use it as a local shape, not proof of complete production behavior.
- `camera` flows from `GLPreview`/`CameraBuilder` to the `NativeRenderer` JNI facade, then through
  process-global `CameraController` and `EGLRenderer` instances. Camera2 NDK frames arrive through
  `AImageReader`, are normalized to FFmpeg `AVFrame` YUV420P, and are drawn by the EGL/GLES shader
  pipeline. Snapshot callbacks read the rendered framebuffer. Recording renders the filtered frame
  into an offscreen FBO, reads it through double-buffered PBOs, converts it off the EGL thread, and
  sends it to `VideoEncoder` (`AMediaCodec` H.264 + `AMediaMuxer` MP4).
- `plugin` is build logic, not runtime DI or application architecture. It derives publication
  coordinates/POM data from Gradle properties and configures Maven Local plus the private Maven
  repository.

Do not claim repository/domain/use-case boundaries that are absent. When adding a new abstraction,
justify it with the owning module's public contract and at least one concrete consumer.

## 4. Networking / data access

`network` declares Ktor 2.3.9 with the OkHttp engine, bearer auth, `HttpSend`, request/response
logging, content negotiation, and kotlinx serialization dependencies. `BaseClient` installs
logging, `HttpSend`, and bearer auth, and exposes JSON requests plus binary multipart upload through
`IClient`; it does not currently install content negotiation. Token loading is delegated to
`ITokenManager`, and the computed `httpClient` property creates a new client on each access, so
client sharing, plugin installation, and closure must be checked together.

`IRetryToken` defines pre-request expiry checks, response-based retry decisions, and refresh.
`RetryTokenManager` separately deduplicates concurrent refresh work by sharing one `Deferred` behind
a `Mutex`. Preserve single-flight behavior, cancellation/error propagation, and reset of the active
job when changing it.

The current weather implementation is incomplete: `WeatherRetryToken` methods are unimplemented,
the header decorator has no behavior, and `BaseClient.defaultHeader` is not applied to requests.
The sample app does not demonstrate a complete authenticated service call. Validate actual wiring
and add focused tests rather than treating the scaffold as a finished reference.

There is no Retrofit, Room, database, cache, repository layer, socket/realtime transport, or
persistent storage in current production source. Ktor `LogLevel.ALL` can expose headers or bodies;
logging/auth changes require an explicit sensitive-data review.

## 5. UI state management

`BaseViewModel` is the reusable state-management convention. Keep immutable screen state in
`StateFlow`, actions in the action `SharedFlow`, ordered state transitions in the mutation channel,
and transient events in the effect `SharedFlow`. `handleAction` starts work; the suspending
`handleMutation` returns the next state. Preserve `viewModelScope` cancellation and mutation order,
and test concurrency/cancellation when changing buffering or dispatch behavior.

The two app activities are standalone coroutine demonstrations using `lifecycleScope`; their
logging, random delays, and blocking `Thread.sleep` are not reusable presentation conventions.

Camera state spans Kotlin and native code. `CameraBuilder` supplies initial shader, mode, lens, and
per-(mode, lens) resolution settings before surface startup. Persistent Camera2 controls such as
AF/AE regions, focus lock, and exposure compensation live on `CameraController` and must be
re-applied via `applyControlState()` whenever a capture request is rebuilt.

## 6. View layer — XML / SurfaceView

The sample app uses `AppCompatActivity`, `activity_main.xml`, Android Views, and edge-to-edge window
insets. There are Compose versions in the catalog, but no Compose dependency or `@Composable` in
the current modules. Preserve the touched surface's XML/View ownership unless a Compose migration
is explicitly requested.

`camera` exposes `GLPreview`, a programmatically constructed `SurfaceView` with
`SurfaceHolder.Callback2`. On surface creation it initializes native rendering, pushes the
resolution map, selects lens/mode, and starts capture. Snapshot and recording-stop results are
marshalled back to the main looper. The JNI `nativeCleanup` entry point is currently empty even
though `surfaceDestroyed` calls it; do not assume the current surface teardown releases camera/EGL
resources, and verify lifecycle behavior for any related change.

Library manifests are empty. A consuming host owns permissions, runtime permission UX, output-path
creation, activity/fragment lifecycle integration, and any user-facing controls. Keep visible text
in resources when adding sample UI and keep touch coordinates in view pixels for `focusAt`.

## 7. Dependency injection — Hilt in network

Only `network` applies Hilt and kapt. `WeatherServiceModule` installs into `SingletonComponent` and
uses `@WeatherClient`, `@WeatherAuth`, and `@WeatherHeader` qualifiers for singleton bindings.
Constructors also use `@Inject`, while token implementations are supplied manually through the
`InitNetwork` singleton.

The sample `app` does not apply the Hilt plugin and defines no `@HiltAndroidApp` application or
entry point. Treat the network module as consumer-facing library wiring and validate aggregation,
qualifiers, and concrete/provider types in a real host before extending it. `core`, `camera`, and
`videolib` have no DI framework. Do not introduce repository-wide Hilt wiring for a module-local need.

## 8. Native code, coroutines, storage, and build / publishing

- There is no application persistence layer. File writes occur in camera/bitmap utilities and MP4
  recording paths; callers own valid locations, permissions, cleanup, and failure handling.
- Coroutine code is concentrated in `core`, `network`, and sample activities. Preserve structured
  cancellation and dispatcher ownership. `RetryTokenManager` currently owns an IO scope; changes to
  its lifetime are API/behavior changes and need concurrent-call tests.
- The root uses Gradle wrapper 8.11.1, AGP 8.9.1, Kotlin 2.2.10, Kotlin DSL, AndroidX, and a version
  catalog. There are no product flavors. The app has `debug`, `release`, and `benchmark` build types;
  the dedicated benchmark module enables only its benchmark variant.
- `camera` requires CMake 3.22.1 and NDK 29.0.14206865, builds for `arm64-v8a` and `armeabi-v7a`,
  links Camera2 NDK, Media NDK, Android, log, EGL, GLESv3, JNI graphics, and FFmpeg 57-era shared
  libraries, and carries 16 KB page-size linker/CMake settings. Vendored FFmpeg trees also contain
  x86/x86_64 binaries, but current Gradle ABI filters package only the two ARM ABIs.
- `JNILibraryLoader.initData()` loads the FFmpeg libraries and then `camera`; `GLPreview` does not
  call the loader. A host must load the native libraries before invoking `NativeRenderer`.
- Camera Gradle compilation targets JVM 17. CMake declares C++17, also appends `-std=c++11`, and
  hardcodes a Debug native build type; do not change language/toolchain settings incidentally—resolve
  and verify the effective settings as part of any native build or release change.
- Root, `core`, `camera`, and `plugin` settings read `GITHUB_USERNAME`, `GITHUB_ACCESS_TOKEN`, and
  `GITHUB_PUBLISH` while configuring the private Maven repository. Keep credentials in the
  environment and out of tracked files/logs. `plugin/settings.gradle.kts` currently has a leading
  slash before `pluginManagement`, so validate any standalone plugin build rather than assuming it
  configures successfully. `core` and `camera` apply publication configuration; any Maven
  Local/private publication remains a protected operation.
- Root `:plugin` is included as a regular subproject, not an included plugin build. Changing its
  source does not automatically replace the version-catalog plugin resolution; validate the
  intended local/private plugin-resolution workflow without publishing unless separately approved.

## 9. Naming conventions

Use official Kotlin style, four-space indentation, `PascalCase` types, `camelCase` functions and
properties, `UPPER_SNAKE_CASE` constants, lowercase module names, and `snake_case` Android
resources. Match adjacent trailing-comma and C/C++ formatting rather than reformatting whole files.

Keep code in the owning namespace: `com.chiistudio.library`, `com.chiistudio.core`,
`com.chiistudio.network`, `com.chiistudio.camerandk`, `com.cii.videolib`,
`com.example.benchmark`, or `com.chiistudio.plugin`. Preserve established public/legacy names such
as `IClient`, `ITokenManager`, `camerandk`, and `BitmapExtention` unless a separately approved
migration covers source, consumers, JNI symbols, and publication compatibility.

JNI exports must exactly match `Java_com_chiistudio_camerandk_jni_NativeRenderer_<method>` and the
Kotlin method/callback signatures. Keep native ownership and callback thread documented at the
boundary.

## 10. Testing stack

JVM tests use JUnit 4. `core` and `network` also depend on `kotlinx-coroutines-test`; `core` has real
behavior tests for `BaseViewModel` ordering/cancellation scenarios and `ReentrantMutex` exclusion.
`RetryTokenUnitTest` currently exercises calls without assertions. Most app, network, camera, and
videolib `Example*Test` files are generated smoke tests rather than behavior coverage.

Android tests use AndroidX JUnit/Espresso. Camera rendering, focus, capture, ABI loading, and
recording require a supported device/emulator and cannot be established by JVM tests alone. The
`benchmark` module uses Macrobenchmark, UI Automator, cold startup, and package
`com.chiistudio.library`.

Prefer the narrowest task, such as `:core:testDebugUnitTest`, `:network:testDebugUnitTest`,
`:camera:assembleDebug`, `:app:assembleDebug`, or the specific connected/benchmark task. Gradle
configuration requires the private-repository environment variables even when the selected task
does not consume a private artifact. No repository-wide formatter or linter is configured; use the
relevant compile/test plus focused source review. Report device, SDK/NDK, credential, or native
toolchain blockers honestly.

## 11. High-risk areas

- Public Android library APIs and publication coordinates/POM metadata can affect consumers outside
  this repository; a lack of in-repo callers does not establish safety.
- `camera` has process-global native renderer/controller instances. Mode, lens, resolution, focus,
  surface, and recording changes cross Kotlin, JNI, Camera2 callbacks, the EGL executor, the record
  conversion worker, and the MediaCodec worker. Preserve thread affinity, frame/ref ownership,
  JavaVM attach/detach, callback global-reference cleanup, and stop-before-release ordering.
- Every Camera2 request creation or reconfiguration (open, mode, lens, resolution/session, or
  quality) must keep the quality/control apply order so sticky AF/AE/EV state survives. Mode or lens
  switching is rejected while recording; preserve or deliberately redesign that state rule.
- Recording must stay on the post-filter EGL FBO/PBO path if output is expected to match preview.
  Feeding the raw `AImageReader` frame to the encoder as well would bypass filters and duplicate
  frames/PTS. GL resource creation/destruction must occur with the EGL context current.
- JNI/native cleanup, library load order, Kotlin callback signatures, FFmpeg buffer lifetimes,
  MediaCodec/Muxer finalization, output paths, ARM ABI packaging, and 16 KB alignment are shared
  failure surfaces. Validate on each supported ABI for native changes.
- Network bearer tokens, refresh concurrency, mutable global hooks, `LogLevel.ALL`, upload bodies,
  qualifiers, and singleton lifetimes are security/concurrency-sensitive. Never log or commit real
  credentials/tokens.
- Root settings, version catalog/plugin resolution, module SDK/JVM exceptions, app/benchmark package
  IDs, and publication configuration are coupled build surfaces. Publishing, signing, upload, and
  distribution are never routine verification.
- Several areas are explicit prototypes or incomplete scaffolds (`videolib`, weather token/header,
  `BaseAdapter`, missing Kotlin exposure of the controller's photo-capture path, and native surface
  cleanup). Do not document or build on missing behavior as though it already exists; add an
  explicit contract and tests when completing one of these paths.
