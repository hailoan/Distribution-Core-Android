//
// Created by Loannth5 on 2/4/25.
//

#include "egl_renderer.h"

#include <cstring>

extern "C" {
#include <libavutil/imgutils.h>
}

// ── Post-filter recording helpers (all called on the EGL thread) ─────────────

namespace {

// Free GL-bound and CPU-bound record resources. Caller must guarantee the
// encode worker is drained before this runs (otherwise the worker may still
// reference recordSws / recordStaging).
void releaseRecordResources(EGLRenderer *egl) {
    if (egl->recordPbo[0]) {
        glDeleteBuffers(2, egl->recordPbo);
        egl->recordPbo[0] = egl->recordPbo[1] = 0;
    }
    if (egl->recordFbo) {
        glDeleteFramebuffers(1, &egl->recordFbo);
        egl->recordFbo = 0;
    }
    if (egl->recordTex) {
        glDeleteTextures(1, &egl->recordTex);
        egl->recordTex = 0;
    }
    if (egl->recordSws) {
        sws_freeContext(egl->recordSws);
        egl->recordSws = nullptr;
    }
    egl->recordStaging.clear();
    egl->recordStaging.shrink_to_fit();
    egl->recordPboFilled[0] = egl->recordPboFilled[1] = false;
    egl->recordPboIdx = 0;
    egl->recordW = 0;
    egl->recordH = 0;
}

bool ensureRecordResources(EGLRenderer *egl, int w, int h) {
    if (egl->recordFbo && egl->recordW == w && egl->recordH == h) return true;
    // Size changed (or first init) — tear down and rebuild.
    releaseRecordResources(egl);

    egl->recordW = w;
    egl->recordH = h;

    // Colour texture
    glGenTextures(1, &egl->recordTex);
    glBindTexture(GL_TEXTURE_2D, egl->recordTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // FBO
    glGenFramebuffers(1, &egl->recordFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, egl->recordFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, egl->recordTex, 0);
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (st != GL_FRAMEBUFFER_COMPLETE) {
        LOGI("record FBO incomplete: 0x%x", st);
        releaseRecordResources(egl);
        return false;
    }

    // Double-buffered PBO ring for async readback.
    glGenBuffers(2, egl->recordPbo);
    const GLsizeiptr pboBytes = (GLsizeiptr) w * h * 4;
    for (int i = 0; i < 2; ++i) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, egl->recordPbo[i]);
        glBufferData(GL_PIXEL_PACK_BUFFER, pboBytes, nullptr, GL_STREAM_READ);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    // Staging RGBA buffer the EGL thread memcpys mapped-PBO bytes into before
    // dispatching to the encode worker. Single buffer is safe because the
    // worker is drop-on-busy: the EGL thread only writes when pending == 0.
    egl->recordStaging.resize((size_t) w * h * 4);

    // sws RGBA → YUV420P. The PBO contents are origin-bottom-left from GL;
    // we flip vertically below by negating the source linesize.
    egl->recordSws = sws_getContext(w, h, AV_PIX_FMT_RGBA,
                                    w, h, AV_PIX_FMT_YUV420P,
                                    SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (!egl->recordSws) {
        LOGI("record sws_getContext failed");
        releaseRecordResources(egl);
        return false;
    }
    return true;
}

// Render the same shader pass that just drew to the screen, into the offscreen
// record FBO at camera-frame dimensions with rotation=0. Then issue an async
// readback into the next PBO and, if the previous PBO already holds a frame,
// map it, sws-convert RGBA→YUV420P, and fire the record callback.
void renderRecordPass(EGLRenderer *egl, AVFrame *frame) {
    if (!egl->recordCallback || !frame) return;
    if (!ensureRecordResources(egl, egl->recordW, egl->recordH)) return;

    // Pass 1 — render filtered frame into FBO with rotation neutralised so
    // the encoder receives the un-rotated camera-orientation frame (matches
    // the YUV path the encoder used to receive directly).
    const float savedRotation = egl->gl->rotation;
    egl->gl->rotation = 0.0f;

    glBindFramebuffer(GL_FRAMEBUFFER, egl->recordFbo);
    glViewport(0, 0, egl->recordW, egl->recordH);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // bindUniformYUV() re-uploads YUV textures + sets all uniforms. Reusing
    // it is the simplest correct path; the cost is one extra YUV upload per
    // frame which is well within budget at preview rates.
    bindUniformYUV(egl->gl, frame->width, frame->height, frame);
    setAttributes(egl->gl);   // ends with glFinish()

    // Restore rotation for the screen pass that follows.
    egl->gl->rotation = savedRotation;

    // Async readback into PBO[idx]. The previous-iteration PBO is then mapped;
    // its contents are fully ready by now (the GPU has finished the prior
    // pass before we issued this one).
    const int idx  = egl->recordPboIdx;
    const int prev = idx ^ 1;

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, egl->recordPbo[idx]);
    glReadPixels(0, 0, egl->recordW, egl->recordH,
                 GL_RGBA, GL_UNSIGNED_BYTE, /*offset*/ nullptr);
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        LOGI("record glReadPixels failed: 0x%x", err);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }
    egl->recordPboFilled[idx] = true;

    // Drain the previous PBO if it already holds a frame. The PBO map →
    // memcpy → unmap happens here on the EGL thread; the heavyweight
    // sws_scale + callback runs on the encode worker so we don't stall
    // preview. Drop-on-busy: if the worker hasn't finished the previous
    // frame, skip dispatch this tick rather than queuing up.
    if (egl->recordPboFilled[prev]) {
        const bool busy = egl->recordEncodePending.load(std::memory_order_acquire) > 0;
        glBindBuffer(GL_PIXEL_PACK_BUFFER, egl->recordPbo[prev]);
        const GLsizeiptr pboBytes = (GLsizeiptr) egl->recordW * egl->recordH * 4;
        if (!busy) {
            void *mapped = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, pboBytes,
                                            GL_MAP_READ_BIT);
            if (mapped) {
                if (egl->recordStaging.size() < (size_t) pboBytes) {
                    egl->recordStaging.resize((size_t) pboBytes);
                }
                std::memcpy(egl->recordStaging.data(), mapped, (size_t) pboBytes);
                glUnmapBuffer(GL_PIXEL_PACK_BUFFER);

                const int rw = egl->recordW;
                const int rh = egl->recordH;
                egl->recordEncodePending.fetch_add(1, std::memory_order_release);
                egl->recordEncodeThread->runInThread([egl, rw, rh] {
                    AVFrame *out = av_frame_alloc();
                    if (out) {
                        out->format = AV_PIX_FMT_YUV420P;
                        out->width  = rw;
                        out->height = rh;
                        if (av_frame_get_buffer(out, 32) >= 0) {
                            // glReadPixels is origin-bottom-left; flip via
                            // negative source stride.
                            const int srcStride = rw * 4;
                            const uint8_t *srcBase = egl->recordStaging.data();
                            const uint8_t *srcLast = srcBase
                                                     + (size_t) srcStride * (rh - 1);
                            const uint8_t *srcSlices[1] = { srcLast };
                            int srcStrides[1] = { -srcStride };
                            sws_scale(egl->recordSws, srcSlices, srcStrides,
                                      0, rh, out->data, out->linesize);
                            out->pts = AV_NOPTS_VALUE;  // encoder counts its own pts
                            if (egl->recordCallback) {
                                egl->recordCallback(out);
                            }
                        }
                        av_frame_free(&out);
                    }
                    egl->recordEncodePending.fetch_sub(1, std::memory_order_release);
                });
            } else {
                LOGI("record glMapBufferRange returned null");
            }
        }
        // If busy, we leave PBO[prev] untouched — next render pass will read it
        // again (it still holds valid data). recordPboFilled[prev] stays true.
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    egl->recordPboIdx = prev;
}

}  // namespace

void onReady(EGLRenderer *egl) {
    egl->thread->runInThread([egl] {
//        rendererFrame(egl);
    });
}

void initVideoGL(EGLRenderer *egl, const char *vertex, const char *fragment) {
    egl->gl->textureIdY = loadTexture2D();
    egl->gl->textureIdU = loadTexture2D();
    egl->gl->textureIdV = loadTexture2D();
    initProgram(egl->gl, vertex, fragment);
}

bool initEGL(EGLRenderer *egl, const char *vertex, const char *fragment) {
    if (!egl->nativeWindow) {
        LOGI("Native window is NULL, cannot create EGL surface.");
        return false;
    }
    egl->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (egl->display == EGL_NO_DISPLAY) {
        LOGI("Failed to get EGL display");
        return false;
    }
    if (!eglInitialize(egl->display, nullptr, nullptr)) {
        LOGI("Failed to initialize EGL");
        return false;
    }

    EGLint configAttribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_DEPTH_SIZE, 16,
            EGL_NONE
    };
    EGLint numConfigs;
    if (!eglChooseConfig(egl->display, configAttribs, &egl->config, 1, &numConfigs)) {
        LOGI("Failed to choose EGL config");
        return false;
    }
    if (egl->config == nullptr) {
        LOGI("Unable to initialize EGLConfig");
        return -1;
    }
    eglGetConfigAttrib(egl->display, egl->config, EGL_NATIVE_VISUAL_ID, &egl->format);

    egl->surface = eglCreateWindowSurface(egl->display, egl->config, egl->nativeWindow, nullptr);
    if (egl->surface == EGL_NO_SURFACE) {
        LOGI("Failed to create EGL surface");
        return false;
    }

    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    egl->context = eglCreateContext(egl->display, egl->config, EGL_NO_CONTEXT, contextAttribs);
    if (egl->context == EGL_NO_CONTEXT) {
        LOGI("Failed to create EGL context");
        return false;
    }

    if (!eglMakeCurrent(egl->display, egl->surface, egl->surface, egl->context)) {
        LOGI("Failed to make EGL context current");
        return false;
    }

    eglQuerySurface(egl->display, egl->surface, EGL_WIDTH, &egl->w);
    eglQuerySurface(egl->display, egl->surface, EGL_HEIGHT, &egl->h);
    LOGI("EGL initialized successfully");

    initVideoGL(egl, vertex, fragment);
    return true;
}

void
updateFilter(EGLRenderer *egl, const char *vertex, const char *fragment,
             std::vector<const uint8_t *> filters,
             std::vector<jint> widths, std::vector<jint> heights) {
    egl->thread->runInThread([egl, vertex, fragment, filters, widths, heights] {
        egl->gl->filterSize = filters.size();

        for (int i = 0; i < filters.size(); i++) {
            egl->gl->textureFilterIds[i] = loadTextureFromPixel(filters[i], widths[i],
                                                                heights[i]);
        }
        initProgram(egl->gl, vertex, fragment);
        renderFrame(egl, egl->currentFrame);
    });
}

void requestRenderer(EGLRenderer *egl) {
    egl->thread->runInThread([egl] {
        if (egl->currentFrame) {
            renderFrame(egl, egl->currentFrame);
        }
    });
}

void setPreviewRotation(EGLRenderer *egl, float degrees) {
    if (egl == nullptr || egl->gl == nullptr) return;
    egl->thread->runInThread([egl, degrees] {
        egl->gl->rotation = degrees;
    });
}

void
startInitGL(EGLRenderer *egl, ANativeWindow *window, const char *vertex,
                 const char *fragment) {
    egl->nativeWindow = window;
    egl->thread->runInThread([egl, vertex, fragment] {
        initEGL(egl, vertex, fragment);
    });
}

void renderFrame(EGLRenderer *egl, AVFrame *frame) {
    if (egl == nullptr || frame == nullptr) return;
    egl->currentFrame = frame;
    egl->thread->runInThread([egl, frame] {
        if (eglMakeCurrent(egl->display, egl->surface, egl->surface, egl->context)) {
            // Pass 1 (optional): post-filter capture into offscreen FBO for
            // video recording. Runs first so the screen pass below isn't
            // disturbed by FBO bindings, and so the photo-capture readback
            // (which reads the screen back buffer) still sees the on-screen
            // image. The record pass uses rotation=0 internally.
            if (egl->recordRequested.load(std::memory_order_acquire)
                    && egl->recordCallback) {
                renderRecordPass(egl, frame);
            }

            // Pass 2: on-screen draw at window-surface size, with the real
            // preview rotation applied.
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, egl->w, egl->h);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            bindUniformYUV(egl->gl, frame->width, frame->height, frame);
            setAttributes(egl->gl);

            // Drain a pending capture request BEFORE eglSwapBuffers — the back
            // buffer still holds the freshly-drawn frame. setAttributes() ends
            // with glFinish(), so all draw calls are complete at this point.
            if (egl->captureRequested.load(std::memory_order_acquire)) {
                const int w = egl->w;
                const int h = egl->h;
                const size_t needed = static_cast<size_t>(w) * h * 4;
                if (w > 0 && h > 0) {
                    if (egl->captureBuffer.size() < needed) {
                        egl->captureBuffer.resize(needed);
                    }
                    glPixelStorei(GL_PACK_ALIGNMENT, 1);
                    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE,
                                 egl->captureBuffer.data());
                    GLenum err = glGetError();
                    if (err != GL_NO_ERROR) {
                        LOGI("glReadPixels failed: 0x%x", err);
                    }
                    if (egl->captureCallback) {
                        egl->captureCallback(egl->captureBuffer.data(), w, h);
                    }
                } else if (egl->captureCallback) {
                    egl->captureCallback(nullptr, 0, 0);
                }
                egl->captureCallback = nullptr;
                egl->captureRequested.store(false, std::memory_order_release);
            }

            eglSwapBuffers(egl->display, egl->surface);
        } else {
            LOGI("eglMakeCurrent fail %d", eglGetError());
        }
    });
}

void captureFramePixels(EGLRenderer *egl, CapturePixelsCallback callback) {
    if (egl == nullptr || !callback) return;
    // Hop onto the EGL thread so we serialize with renderFrame without racing.
    egl->thread->runInThread([egl, callback = std::move(callback)]() mutable {
        // If a prior request is still pending, replace it — last caller wins.
        egl->captureCallback = std::move(callback);
        egl->captureRequested.store(true, std::memory_order_release);
    });
}

void startRecordCapture(EGLRenderer *egl, int encodeW, int encodeH,
                        RecordFrameCallback callback) {
    if (egl == nullptr || !callback || encodeW <= 0 || encodeH <= 0) return;
    // Round to even — H.264 requires it and so does VideoEncoder downstream.
    const int w = encodeW & ~1;
    const int h = encodeH & ~1;
    egl->thread->runInThread([egl, w, h, callback = std::move(callback)]() mutable {
        // Make context current; resource creation needs an active GL context.
        if (!eglMakeCurrent(egl->display, egl->surface, egl->surface, egl->context)) {
            LOGI("startRecordCapture: eglMakeCurrent failed 0x%x", eglGetError());
            return;
        }
        egl->recordW = w;
        egl->recordH = h;
        if (!ensureRecordResources(egl, w, h)) {
            LOGI("startRecordCapture: failed to init record resources %dx%d", w, h);
            return;
        }
        if (!egl->recordEncodeThread) {
            egl->recordEncodeThread = new SingleThreadExecutor("record_encode");
        }
        egl->recordEncodePending.store(0, std::memory_order_release);
        egl->recordCallback = std::move(callback);
        egl->recordRequested.store(true, std::memory_order_release);
        LOGI("startRecordCapture: armed %dx%d", w, h);
    });
}

void stopRecordCapture(EGLRenderer *egl) {
    if (egl == nullptr) return;
    egl->thread->runInThread([egl] {
        // 1. Disarm so no future render pass dispatches new encode tasks.
        egl->recordRequested.store(false, std::memory_order_release);
        // 2. Drain the encode worker — its dtor processes pending tasks then
        //    joins. After this, no encode task references recordSws / staging.
        if (egl->recordEncodeThread) {
            delete egl->recordEncodeThread;
            egl->recordEncodeThread = nullptr;
        }
        egl->recordCallback = nullptr;
        egl->recordEncodePending.store(0, std::memory_order_release);
        // 3. Now safe to free GL + sws + staging.
        if (egl->display != EGL_NO_DISPLAY && egl->context != EGL_NO_CONTEXT) {
            eglMakeCurrent(egl->display, egl->surface, egl->surface, egl->context);
        }
        releaseRecordResources(egl);
        LOGI("stopRecordCapture: released");
    });
}

void cleanup(EGLRenderer *egl) {
    if (egl->display != EGL_NO_DISPLAY) {
        LOGI("EGL cleaned up");
        // Drop record resources before tearing the context down so the GL
        // objects are released against the still-current context.
        eglMakeCurrent(egl->display, egl->surface, egl->surface, egl->context);
        egl->recordRequested.store(false, std::memory_order_release);
        if (egl->recordEncodeThread) {
            delete egl->recordEncodeThread;
            egl->recordEncodeThread = nullptr;
        }
        egl->recordCallback = nullptr;
        egl->recordEncodePending.store(0, std::memory_order_release);
        releaseRecordResources(egl);

        eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (egl->context != EGL_NO_CONTEXT) {
            eglDestroyContext(egl->display, egl->context);
        }
        if (egl->surface != EGL_NO_SURFACE) {
            eglDestroySurface(egl->display, egl->surface);
        }
        eglTerminate(egl->display);
    }
    // Reset to no context
    egl->display = EGL_NO_DISPLAY;
    egl->surface = EGL_NO_SURFACE;
    egl->context = EGL_NO_CONTEXT;
    ANativeWindow_release(egl->nativeWindow);
    egl->nativeWindow = nullptr;
}