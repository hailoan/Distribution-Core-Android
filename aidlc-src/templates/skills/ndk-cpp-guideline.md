---
name: NDK C++ Guideline
description: Protect the JNI boundary, native toolchain/STL config, worker-thread lifecycle, reference and native-object ownership, and the single-target CMake shape when NDK/C++/JNI surfaces are touched. Read-only guidance.
---

## NDK C++ Guideline

Load only when a change touches native C++/JNI surfaces in `camera`: the `external fun`
declarations (`jni/NativeRenderer.kt`), the C++ JNI entry points (`camera_native.cpp`), the native
toolchain/STL/CMake config, worker-thread or native-object lifecycle, or a Kotlin consumer of those
contracts. This is a detail behind `native-boundary-guideline`; keep that umbrella guard in effect,
and load `ffmpeg-guideline` / `opengles-guideline` for the FFmpeg or GL specifics.

The JNI boundary is hand-mangled, with no `RegisterNatives` fallback. Each C++ export is a literal
`Java_com_chiistudio_camerandk_jni_NativeRenderer_<method>` in `camera_native.cpp` matched by name
to a Kotlin `external fun` on the `NativeRenderer` object. Renaming the package, class, or any
method — or moving `NativeRenderer` — silently breaks the symbol link (an `UnsatisfiedLinkError` at
call time, not a compile error). Change both sides together, and keep the JNI signature exactly in
sync with the Kotlin parameter/return types and every callback signature (e.g.
`CaptureCallback.onCaptured` is `"([BII)V"`, `RecordingCallback.onStopped` is
`"(Ljava/lang/String;)V"`). `JNI_OnLoad` caches `JavaVM *g_jvm` and returns `JNI_VERSION_1_6`;
preserve that cache — native worker threads depend on it.

Reference and buffer discipline is manual — get the ownership rules right. Local refs obtained in a
loop or helper (`GetObjectClass`, `CallObjectMethod`, `FindClass`, …) must be released with
`DeleteLocalRef` to avoid overflowing the local frame (see `convertListOfByteBuffers` /
`convertListToIntVector`). A callback object retained past the current call must be promoted with
`NewGlobalRef` and released with `DeleteGlobalRef` after use. `GetStringUTFChars` must be paired
with `ReleaseStringUTFChars`. Direct `ByteBuffer` access (`GetDirectBufferAddress` /
`GetDirectBufferCapacity`) is zero-copy — the Kotlin buffers must be **direct** and must outlive
the native call; do not read a heap `ByteBuffer` through that path.

Cross-thread callbacks must attach and detach correctly. Native callbacks fire on worker threads
(the EGL thread, the encoder thread), not the JNI caller thread. Each uses the
`g_jvm->GetEnv` / `AttachCurrentThread` / invoke / `DetachCurrentThread` dance with a
`NewGlobalRef` on the callback, released after invocation. Never cache or reuse a `JNIEnv*` across
threads — it is thread-local; re-derive it from `g_jvm` on the thread that runs the callback.

Respect the threading primitive and native-object lifetime. Off-main work runs on
`SingleThreadExecutor` (`utils/single_thread_executor.h`): a mutex+condvar task queue whose
destructor sets `stop`, notifies, and `join()`s the worker — so deleting an executor drains and
blocks until its thread exits. Order teardown so nothing still references resources the executor's
worker touches. Native singletons (`EGLRenderer *renderer`, `CameraController *g_camera`) are
heap-allocated at load and are effectively process-lifetime; `nativeCleanup`/`nativeRender` are
empty stubs, so do not assume a native teardown path exists. Guard shared native state with the
existing mutexes (e.g. `frameMtx_`) rather than adding new global locks.

Keep the toolchain and CMake contract intact. NDK is `29.0.14206865`, CMake `3.22.1`, STL
`c++_static` (`-DANDROID_STL=c++_static`), `CMAKE_ANDROID_API 24` / `-DANDROID_PLATFORM=android-24`.
Note the standard is contradictory: `CMAKE_CXX_STANDARD 17` is set but `CMAKE_CXX_FLAGS` also forces
`-std=c++11`, while the code uses C++17 features (init-capture lambdas, `std::atomic`); it compiles
under the effective toolchain default, so do not "tidy" the `-std` flag without verifying the
build. All first-party sources build one shared target, `add_library(camera SHARED ...)` →
`libcamera.so`, linking NDK libs (`camera2ndk`, `mediandk`, `android`, `log`, `EGL`, `GLESv3`,
`jnigraphics`) plus the imported FFmpeg targets; add a new `.cpp` to that target's source list, not
a new library. Keep the ARM `abiFilters` and 16 KB page settings (`CMAKE_ANDROID_PAGE_SIZE 16384`,
`--page-size=16384`) aligned — see `ffmpeg-guideline` for the packaging half. Log through the
`utils/log-android.h` macros (`LOGI`/`LOGE`, tag `ChiiStudio`), not raw `__android_log_print`.

Verification must distinguish proof levels. A native/Gradle compile proves JNI signatures, symbol
names, and linkage resolve; only a supported-device/ABI run proves callback attach/detach, thread
lifecycle, and native runtime behavior. Never claim runtime behavior from compilation alone.
