//
// Created by Loannth5 on 2/4/25.
//

#ifndef MY_APPLICATION_EGL_RENDERER_H
#define MY_APPLICATION_EGL_RENDERER_H

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <unistd.h>
#include <atomic>
#include <functional>
#include <vector>
#include "queue"
#include "mutex"
#include "condition_variable"
#include "thread"
#include "video_gl.h"

extern "C" {
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

using namespace std;

// Callback invoked on the EGL thread with a tightly-packed RGBA8888 buffer.
// Ownership of the buffer stays with the renderer — copy in the callback if needed.
typedef std::function<void(const uint8_t *pixels, int width, int height)> CapturePixelsCallback;

// Callback invoked on the record-encode worker thread (NOT the EGL thread)
// with a freshly-allocated YUV420P AVFrame. The frame is owned by the worker:
// after the callback returns, the renderer frees its reference. The consumer
// must clone (av_frame_clone / av_frame_ref) to retain the data — the
// underlying buffer survives via refcount.
typedef std::function<void(AVFrame *yuv420p)> RecordFrameCallback;

typedef struct EGLRenderer {
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    EGLConfig config;
    ANativeWindow *nativeWindow;
    EGLint w, h, format;
    SingleThreadExecutor *thread = new SingleThreadExecutor("egl_renderer");
    VideoGl *gl = new VideoGl();
    AVFrame *currentFrame;

    // One-shot capture request. Set by captureFramePixels(); consumed at the
    // end of the next renderFrame() on the EGL thread. Accessed only from
    // the EGL thread, so no locking required.
    std::atomic<bool> captureRequested{false};
    CapturePixelsCallback captureCallback;
    std::vector<uint8_t> captureBuffer;

    // ── Post-filter recording path ───────────────────────────────────────────
    // Continuous while recording: render the same shader pass into an offscreen
    // FBO sized to the camera frame, async-read via a double-buffered PBO ring.
    // The EGL thread copies the mapped PBO bytes into recordStaging and hands
    // off to recordEncodeThread, which runs sws_scale RGBA→YUV420P and fires
    // recordCallback. This keeps the heavyweight color conversion off the EGL
    // thread so it doesn't stall preview.
    //
    // GL-bound members (Fbo/Tex/Pbo) are touched only from the EGL thread.
    // recordSws and recordStaging are written by the EGL thread and read by
    // the encode worker — drop-on-busy backpressure (recordEncodePending) makes
    // sure they're never accessed concurrently.
    std::atomic<bool> recordRequested{false};
    RecordFrameCallback recordCallback;
    int           recordW = 0;
    int           recordH = 0;
    GLuint        recordFbo = 0;
    GLuint        recordTex = 0;        // RGBA8 colour attachment
    GLuint        recordPbo[2] = {0, 0};
    int           recordPboIdx = 0;
    bool          recordPboFilled[2] = {false, false}; // true once PBO[i] holds a real frame
    SwsContext   *recordSws = nullptr;
    std::vector<uint8_t>  recordStaging;          // RGBA bytes copied from PBO
    SingleThreadExecutor *recordEncodeThread = nullptr;
    std::atomic<int>      recordEncodePending{0}; // >0 → worker busy, drop frame
};


void onReady(EGLRenderer *egl);

void initVideoGL(EGLRenderer *egl, const char *vertex, const char *fragment);

bool initEGL(EGLRenderer *egl, const char *vertex, const char *fragment);

void updateFilter(EGLRenderer *egl, const char *vertex, const char *fragment,
                  std::vector<const uint8_t *> filters,
                  std::vector<jint> widths, std::vector<jint> heights);

void requestRenderer(EGLRenderer *egl);

// Update the preview rotation uniform applied in the fragment shader.
// Safe to call from any thread; the change is queued onto the EGL thread
// and picked up by the next renderFrame().
void setPreviewRotation(EGLRenderer *egl, float degrees);

void
startInitGL(EGLRenderer *egl, ANativeWindow *window, const char *vertex, const char *fragment);

void renderFrame(EGLRenderer *egl, AVFrame *frame);

/**
 * Request a one-shot readback of the default framebuffer. The callback fires
 * on the EGL thread after the next renderFrame() completes drawing. If no
 * frame arrives (camera stalled), the callback will not fire until one does.
 * Safe to call from any thread.
 */
void captureFramePixels(EGLRenderer *egl, CapturePixelsCallback callback);

/**
 * Begin continuous post-filter capture for video recording. Each subsequent
 * renderFrame() will additionally render the filtered output into an offscreen
 * FBO sized (encodeW, encodeH) (the un-rotated camera-frame dimensions), async
 * read it back via a double-buffered PBO, convert RGBA → YUV420P, and invoke
 * `callback` with the resulting AVFrame on the EGL thread.
 *
 * The first 1–2 frames after start typically produce no callback (PBO ring is
 * still priming). Safe to call from any thread; the request is queued onto
 * the EGL thread.
 *
 * The renderer owns the YUV AVFrame; consumers must clone or copy if they
 * need to retain it past the callback (VideoEncoder::encodeFrame() does this).
 */
void startRecordCapture(EGLRenderer *egl, int encodeW, int encodeH,
                        RecordFrameCallback callback);

/**
 * Stop continuous post-filter capture and release the FBO/PBO/sws resources
 * on the EGL thread. The callback registered by startRecordCapture() will
 * not fire after this returns onto the EGL thread.
 */
void stopRecordCapture(EGLRenderer *egl);

void cleanup(EGLRenderer *egl);

#endif //MY_APPLICATION_EGL_RENDERER_H
