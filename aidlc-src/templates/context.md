# Project Context — {{PROJECT_NAME}}

This source-validated context is the durable seed for `.aidlc/context.md`. The generated
`.aidlc/modules.json` is the executable module topology; this document explains architectural and
runtime contracts that the registry cannot express.

## 0. Project ground rules

- This is a library-oriented, multi-module Android workspace with a sample app, not one product
  feature module. Put changes in the narrowest owning module and do not invent a repository-wide
  clean-architecture or Compose structure.
- Treat Kotlin public APIs, Hilt bindings, Gradle plugin/publication behavior, JNI declarations,
  exported JNI names, C++ ownership, CMake linkage, ABI packaging, and bundled native libraries as
  cross-boundary contracts. Inspect every side and affected consumer before editing.
- Use the current code graph first where it covers the target, then validate against source. Its
  aggregate view includes vendored FFmpeg headers and AI-DLC JavaScript; a missing edge never proves
  a public library has no external consumer.
- Treat `camera/src/main/cpp/ffmpegv2` as vendored. Ignore build output (`build`, `.gradle`, `.cxx`,
  `.externalNativeBuild`) unless a generated/package artifact is explicitly under inspection.
- Preserve local module patterns. Do not combine requested work with dependency upgrades,
  publication changes, broad renames, or architectural rewrites.
- Focused compile/test checks are normal verification. Commit, push, sign, publish, upload, and
  distribute only with separate explicit approval.

## 1. What the app is

`{{PROJECT_NAME}}` is a Kotlin-first set of reusable Android experiments/libraries with a small
XML/AppCompat host app. `core` owns coroutine MVVM/state primitives, `network` owns Ktor/OkHttp and
token hooks, and `camera` owns Camera2 NDK/JNI/EGL filtering, capture, and recording. `plugin` is
Gradle publication build logic, `benchmark` targets the sample app, and `videolib` is a scaffold.

Production source mixes Kotlin, Java, and C++; camera also carries GLSL and vendored FFmpeg
headers/shared objects. There is no Compose UI, Room/database layer, product-level feature stack,
or single dependency-injection strategy. The catalog declares Kotlin 2.2.10, AGP 8.9.1, compile
SDK 36, target SDK 35, and min SDK 24. `core` compiles with SDK 35, `videolib` has min SDK 21, and
`camera` targets JVM 17 while other Android modules target JVM 11.

## 2. Modules / structure

- `app` — sample APK (`com.chiistudio.library`), directly depending on `:network`; its launcher
  demonstrates `RetryTokenManager`. It is an integration consumer, not the default code owner.
- `core` — public Android library (`com.chiistudio.core`) with `BaseViewModel`, `ReentrantMutex`,
  and an empty `BaseAdapter`; publication-enabled.
- `network` — public Android library (`com.chiistudio.network`) with Ktor clients, authentication
  and retry hooks, weather scaffolding, serialization dependencies, and Hilt wiring.
- `camera` — public Android/native library (`com.chiistudio.camerandk`) with Kotlin/JNI facade,
  Camera2 NDK, EGL/GLES, shaders, snapshots, recording, CMake, and FFmpeg inputs; publication-enabled.
- `videolib` — Android/native library (`com.cii.videolib`) scaffold with build/manifest/example tests
  and a JNI/CMake `NativeLib.stringFromJNI` stub, but no production video code yet.
- `benchmark` — self-instrumenting Macrobenchmark consumer targeting `:app`'s benchmark variant.
- `plugin` — Gradle plugin implementing `com.chiistudio.plugin` publication conventions.
- `aidlc-src` and `.aidlc` — development workflow source/runtime, not Android modules.

Production code lives under each module's `src/main`, JVM tests under `src/test`, Android tests
under `src/androidTest`, and resources under `src/main/res`. Validate the generated registry against
`settings.gradle.kts` and module build files whenever dependency/build topology changes.

## 3. Architecture

Architecture follows owning modules, not one global layering template:

- `core`: `BaseViewModel<S,A,E>` processes actions into ordered channel-backed mutations, exposes
  durable `StateFlow<S>`, and emits one-shot effects separately. `ReentrantMutex` supports
  same-coroutine reentrancy over a `Mutex`.
- `network`: `IClient`/`BaseClient`, weather client/decorators/service, Hilt qualifiers/module, and
  process-global `InitNetwork` callbacks. Several paths remain prototypes.
- `camera`: `GLPreview`/`CameraBuilder` → `NativeRenderer` JNI → process-global
  `CameraController`/`EGLRenderer`. Camera2 frames become YUV420P `AVFrame`, render through EGL/GLES,
  and record from the filtered FBO/PBO path into MediaCodec/Muxer.
- `plugin`: build logic deriving publication/POM data from Gradle properties; it is not runtime DI.

Do not claim repository/domain/use-case layers that source does not contain. A new shared
abstraction needs an owning public contract and at least one concrete consumer.

## 4. Networking / data access

`network` declares Ktor 2.3.9 with OkHttp, bearer auth, `HttpSend`, logging, content-negotiation and
serialization dependencies. `BaseClient` installs logging, `HttpSend`, and bearer auth, but not
content negotiation; its computed `httpClient` creates a client per access. Check lifetime,
plugins, token behavior, closure, and callers together.

`RetryTokenManager` deduplicates concurrent refresh work through one mutex-protected `Deferred`.
Preserve single-flight, cancellation/error propagation, and active-job reset. Weather token/header
behavior and default headers are incomplete scaffolds. Ktor `LogLevel.ALL` is sensitive. There is
no Retrofit, Room, repository, socket, cache, or persistent data layer in production source.

## 5. UI state management

`BaseViewModel` is the reusable state pattern: actions enter a `SharedFlow`, ordered mutations
produce immutable `StateFlow` state, and effects remain transient. Preserve `viewModelScope`,
cancellation, ordering, buffering semantics, and atomic/concurrent behavior.

Sample activities are coroutine demonstrations, not reusable architecture. Camera state crosses
Kotlin/native code. `CameraBuilder` sets initial shader/mode/lens/resolution; persistent Camera2
AF/AE/EV controls must be reapplied whenever requests are rebuilt.

## 6. View layer — XML / SurfaceView

The app uses `AppCompatActivity`, XML layouts, Android Views, and edge-to-edge insets. Catalogued
Compose versions are unused; preserve XML/View ownership unless migration is explicit.

`camera` exposes programmatic `GLPreview` (`SurfaceView`/`SurfaceHolder.Callback2`). It initializes
native rendering, resolution/lens/mode, and capture at surface creation, then marshals snapshot and
recording results to the main looper. `nativeCleanup` is currently empty; never assume surface
destruction releases camera/EGL state. Host apps own permissions, paths, lifecycle UX, and controls.

## 7. Dependency injection — Hilt in network

Only `network` applies Hilt/kapt. `WeatherServiceModule` installs into `SingletonComponent` with
qualified singleton bindings. The sample app has no Hilt application/entry point. Treat Hilt as a
consumer-facing network-module contract and verify host aggregation; do not introduce it globally.

## 8. Native code, coroutines, storage, and build / publishing

- Camera file/MP4 output paths are caller-owned; there is no application persistence layer.
- Root uses Gradle 8.11.1, AGP 8.9.1, Kotlin DSL, AndroidX, and a version catalog; there are no flavors.
- Camera uses CMake 3.22.1/NDK 29, packages ARM64/ARMv7, links Camera/Media NDK, EGL/GLES and FFmpeg,
  and carries 16 KB page settings. Vendored x86 binaries exist but are not in current ABI filters.
- `JNILibraryLoader` must load FFmpeg libraries then `camera`; `GLPreview` does not do so.
- CMake declares C++17 but also appends `-std=c++11` and hardcodes Debug. Resolve effective settings
  explicitly for toolchain work; do not change them incidentally.
- Repository configuration reads GitHub credentials from environment. Never put them in files/logs.
- `plugin/settings.gradle.kts` has a leading slash before `pluginManagement`; validate standalone
  behavior rather than assuming it works.
- `:plugin` is a regular subproject, not an included plugin build. Editing it does not automatically
  replace the published plugin used by `core`/`camera`. Verify resolution without publishing.

## 9. Naming conventions

Use Kotlin style, four spaces, `PascalCase` types, `camelCase` members, `UPPER_SNAKE_CASE` constants,
lowercase modules, and `snake_case` resources. Match local C/C++ formatting and avoid bulk reformat.
Keep module namespaces from the registry. Preserve legacy public names unless an approved migration
covers source, consumers, JNI, and publication. JNI exports must exactly match
`Java_com_chiistudio_camerandk_jni_NativeRenderer_<method>` and Kotlin signatures.

## 10. Testing stack

JVM tests use JUnit 4; `core` and `network` also use coroutines-test. Existing `core` tests cover
ordering/cancellation and mutex behavior; many `Example*Test` files are generated smoke tests.
Android tests use AndroidX JUnit/Espresso. Camera render/focus/capture/load/record behavior needs a
supported device/ABI. Benchmark uses Macrobenchmark/UI Automator against
`com.chiistudio.library`.

Prefer module checks from `.aidlc/modules.json`. Gradle configuration requires private-repository
environment variables even when artifacts are unused. No repository-wide linter exists; combine
the narrow compile/test with focused source review and report device/SDK/NDK/credential blockers.

## 11. High-risk areas

- Public library APIs/publication metadata may affect consumers outside this repository.
- Camera crosses process-global native owners, Camera2 callbacks, EGL, conversion and codec workers.
  Preserve thread affinity, frame/reference lifetime, JavaVM attachment, cleanup, and stop order.
- Request rebuilds apply quality then persistent control state; mode/lens changes remain rejected
  while recording; recording stays on the post-filter path; GL resources require a current context.
- JNI cleanup/load order/callbacks, FFmpeg buffers, MediaCodec/Muxer finalization, ARM ABIs and 16 KB
  alignment require compile/package plus supported-device verification.
- Token refresh concurrency, global hooks, full HTTP logging, qualifiers, and singleton lifetime are
  security/concurrency-sensitive.
- Settings, catalog/plugin resolution, SDK/JVM exceptions, app/benchmark package IDs, and publication
  configuration are coupled build surfaces.
- `videolib`, weather auth/header, `BaseAdapter`, photo capture exposure, and native cleanup contain
  scaffolding or missing behavior. Never treat missing behavior as a completed reference.
