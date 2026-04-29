---
name: Post-filter recording is GL-driven, not camera-callback-driven
description: VideoEncoder is fed by an offscreen FBO + PBO readback inside renderFrame, not by the AImageReader callback path
type: project
---

Recording uses an offscreen FBO sized to the camera frame inside `renderFrame()` — NOT the raw YUV from `handleImage()`. The camera callback no longer pushes frames into VideoEncoder.

**Why:** Recorded video must include the live shader filters (so what's recorded matches what the user sees in preview). Doing readback from the on-screen window surface would couple encode resolution to the SurfaceView size and break under rotation/letterbox; an offscreen FBO at camera dimensions keeps encode dims stable.

**How to apply:**
- The recording hook is `startRecordCapture()` / `stopRecordCapture()` in `egl_renderer.{h,cpp}`. `CameraController::startRecording()` arms it; the per-frame callback runs on the EGL thread and calls `encoder_.encodeFrame()`.
- The FBO render uses `rotation = 0` (saves/restores `gl->rotation` around the pass), so the encoder receives un-rotated camera-orientation frames. MP4 orientation hint handles display rotation downstream.
- Readback is double-buffered PBO (`recordPbo[2]`), which introduces ~1 frame of latency — the first frame after start is intentionally dropped while the ring primes.
- glReadPixels is bottom-up; the sws pass uses negative source linesize + a pointer to the last row to flip vertically into YUV420P.
- Do NOT add a second YUV→encoder push from `handleImage()` — that would double-encode and break PTS.
- Do NOT change FBO/PBO state without an active EGL context. `startRecordCapture` and cleanup paths call `eglMakeCurrent` first.
