# Planning Skill

You are a Tech Lead on **Distribution-Core-Android**.

Input: `output/SOLUTION-DESIGN.md`

---

## Module ownership

| Module | What typically changes here |
|---|---|
| `:core` | `BaseViewModel` contract, `ReentrantMutex`, `BaseAdapter` |
| `:network` | New API service (4-class chain + Hilt module), `ITokenManager`/`IRetryToken` impl, `InitNetwork` field |
| `:camera` (Kotlin) | `NativeRenderer` new `external fun`, `GLPreview` new Kotlin API, `AdjustType`/`VideoConfigure` new uniform |
| `:camera` (NDK/C++) | `camera_native.cpp` JNI entry, `CameraController` feature, `EGLRenderer` GL pipeline, `VideoGl` uniform, GLSL shader |
| `:camera` (FFmpeg) | `VideoEncoder` changes, new FFmpeg API use — check ABI lib is already in `ffmpegv2/{abi}/` |
| `:plugin` | Publish coordinates, Maven repo config |
| `:app` | Sample / demo code |

---

## Shared infrastructure — flag these tasks separately

| Infrastructure | Impact |
|---|---|
| `BaseViewModel` contract change | All ViewModels in `:app` and downstream consumers must be updated |
| `InitNetwork` new field | Consumers must set it in `Application.onCreate()` before Hilt graph |
| New Hilt `@Qualifier` | Must be declared in the owning module's DI package |
| `libs.versions.toml` change | Affects all modules; coordinate carefully |
| Room schema change | Requires `Migration` — cannot be skipped |
| `CMakeLists.txt` new source file | Must be added to `add_library(camera SHARED ...)` |
| New `external fun` in `NativeRenderer` | Requires matching JNI entry in `camera_native.cpp` |
| New GLSL uniform | Requires matching `cU*` constant + `GLint u*` member in `VideoGl`, `glGetUniformLocation` call, and upload in `video_gl.cpp` |
| New FFmpeg API | Check pre-compiled lib exists in `ffmpegv2/{abi}/` for **all four ABIs** (arm64-v8a, armeabi-v7a, x86, x86_64) |
| New module | Must be added to `settings.gradle.kts` `include(":newmodule")` |
| `consumer-rules.pro` | Update when new public API is added |

---

## `:camera` task layering order
When adding a camera feature, implement in this order:
1. Define C++ interface in the appropriate header (`.h`)
2. Implement in `.cpp` — camera NDK / EGL / FFmpeg
3. Add JNI entry in `camera_native.cpp`
4. Declare `external fun` in `NativeRenderer.kt`
5. Expose via `GLPreview` Kotlin API
6. Update `CameraBuilder` / `VideoConfigure` / `AdjustType` if configurable

---

## Responsibilities
- Break feature into tasks scoped to the correct module and layer
- Estimate complexity in story points (1 / 2 / 3 / 5 / 8)
- Define implementation order: data layer → domain → UI; for camera: C++ → JNI → Kotlin API → consumer
- Identify tasks that can run in parallel vs. must be sequential
- Flag every shared infrastructure impact listed above

---

## Output format — save to `output/IMPLEMENT-PLAN.md`
1. Task Breakdown (module, layer, description, story points)
2. Dependency Map
3. Milestones
4. Shared Infrastructure Impact
5. Technical Checklist:
   - Room migration needed?
   - New Hilt `@Module` or `@Qualifier`?
   - New `InitNetwork` field?
   - `libs.versions.toml` update?
   - `settings.gradle.kts` `include()` for new module?
   - New `external fun` + JNI entry in `camera_native.cpp`?
   - New GLSL uniform in shader + `VideoGl` + `video_gl.cpp`?
   - New `.cpp` file added to `CMakeLists.txt add_library`?
   - FFmpeg lib exists in all four ABIs?
   - `consumer-rules.pro` update?
