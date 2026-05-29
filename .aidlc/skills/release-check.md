# Release Check Skill

You are a Release Manager for **Distribution-Core-Android**.

Input: `output/INTEGRATION-TEST.md`, `output/CODE-REVIEW.md`

---

## Publishing context

| Item | Value |
|---|---|
| Publish plugin | `com.chiistudio.plugin` (`:plugin` module) |
| Maven repo | Private GitHub Maven (`GITHUB_PUBLISH` env var) |
| Coordinates | `com.chiistudio:<module>:<version>` |
| Required env vars | `GITHUB_USERNAME`, `GITHUB_ACCESS_TOKEN`, `GITHUB_PUBLISH` |
| `isMinifyEnabled` | `false` for all modules — library project; consumers apply ProGuard |

---

## `:core` / `:network` AAR checklist
- Version string bumped in each affected module's `build.gradle.kts`.
- `libs.versions.toml` consistent — no transitive version conflicts.
- No `Log.e("LOAN", ...)` or `Log.*` in release paths of `:core` or `:network`.
- No `TODO("Not yet implemented")` in `ITokenManager` / `IRetryToken` implementations — crash at first call.
- `consumer-rules.pro` covers new public API in case consumers enable minification.
- `InitNetwork` wiring steps documented for consumers in release notes or README.

---

## `:camera` AAR / NDK checklist
- **All four ABIs present:** arm64-v8a, armeabi-v7a, x86, x86_64 must each have:
  - All eight FFmpeg `.so` files (`libavcodec-57`, `libswresample-2`, `libavdevice-57`, `libavfilter-6`, `libavformat-57`, `libavutil-55`, `libpostproc-54`, `libswscale-4`).
  - The `libcamera.so` built by CMake for each ABI.
- **16 KB page alignment:** `CMAKE_ANDROID_PAGE_SIZE 16384` present in `CMakeLists.txt` — required for Android 15+ compatibility (`git grep -n CMAKE_ANDROID_PAGE_SIZE`).
- **JNI symbol check:** every `external fun` in `NativeRenderer.kt` has a matching `Java_com_chiistudio_camerandk_jni_NativeRenderer_<name>` symbol in `libcamera.so` — verify with `nm -D libcamera.so | grep NativeRenderer`.
- **`JNILibraryLoader.initData()`** documented in integration guide — consumers must call it before any `NativeRenderer` method.
- **No debug symbols in release `.so`:** `CMAKE_BUILD_TYPE` should be `Release` for release builds (currently hardcoded `Debug` in CMakeLists — must be fixed before production publish).
- **Camera permission:** consumers must declare `<uses-permission android:name="android.permission.CAMERA"/>` and `RECORD_AUDIO` for video recording in their `AndroidManifest.xml`.
- `consumer-rules.pro` covers `NativeRenderer`, `JNILibraryLoader`, `GLPreview`, and model classes (`CameraBuilder`, `VideoConfigure`, etc.) if consumers minify.

---

## Build smoke
```bash
./gradlew :core:assembleRelease
./gradlew :network:assembleRelease
./gradlew :camera:assembleRelease   # triggers CMake build for all ABIs
./gradlew :core:test :network:test  # unit tests must pass
```

---

## Rollback
- Previous version still published in the GitHub Maven repo — consumers can pin to it.
- For `:camera` NDK issues (e.g. ABI missing, JNI crash), rollback is the only option after publish — get it right before publishing.

---

## Output format — save to `output/RELEASE-NOTE.md`
1. Release Checklist (module versions, env vars, `consumer-rules.pro`, NDK ABI completeness, page alignment, JNI symbols)
2. API Surface Changes (new public Kotlin methods / new `external fun` / new C++ headers since last release)
3. Risk Analysis
4. Rollback Plan
5. Release Notes (features, fixes, known issues)
6. Final Go / No-Go Decision
