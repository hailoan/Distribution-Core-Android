---
name: camerandk pipeline shape for capture hooks
description: Structural facts about the camerandk GL/Camera2 pipeline that govern where capture hooks must live
type: project
---

The `com.chiistudio.camerandk` library pipeline that capture features must hook into.

**Why:** Capture features in this repo must never redesign the pipeline. Knowing the exact drawing boundary and thread ownership is what makes capture one-shot flags safe and prevents the common mistake of spinning up a second EGL context.

**How to apply:** When asked to add snapshot/record/encode features, hook into these existing seams rather than duplicating them:

- `EGLRenderer` is a POD struct in `src/main/cpp/gl/egl_renderer.h`. It owns `EGLDisplay/Surface/Context`, the `ANativeWindow`, viewport `w/h`, the `VideoGl*`, the `currentFrame`, and a `SingleThreadExecutor *thread` named `"egl_renderer"`. Capture state (atomic flag + callback + reusable `std::vector<uint8_t>`) lives on this struct — no separate capture object.
- Every GL call must go through `egl->thread->runInThread(...)`. That thread also owns the EGL context. `captureFramePixels` posts onto it rather than racing with `renderFrame`.
- `renderFrame(EGLRenderer*, AVFrame*)` is the single drawing entry point per frame. Its tail is: `bindUniformYUV → setAttributes (ends with glFinish) → eglSwapBuffers`. The capture readback must sit between `setAttributes` and `eglSwapBuffers` — that is the only moment where the back buffer holds a complete frame and is still readable.
- `camera_native.cpp` holds the global singletons `renderer` and `g_camera`. JNI entry names follow `Java_com_chiistudio_camerandk_jni_NativeRenderer_<method>`. Kotlin callback interop requires caching `JavaVM` in `JNI_OnLoad` (the JNI file previously had none — capture was what introduced it).
- Kotlin `NativeRenderer` is an `object` (singleton) and uses `fun interface` for callbacks — the JNI side looks up the SAM method by its declared name (e.g., `onCaptured`), NOT `invoke`.
- `glReadPixels` returns pixels with origin at bottom-left; the Kotlin layer (`GLPreview`) is responsible for the vertical flip before `Bitmap.copyPixelsFromBuffer`.
