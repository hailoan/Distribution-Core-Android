//
// Created by loannth20 on 1/4/26.
//

#include "camera_controller.h"
#include <string>

// ── NDK callback trampolines ──────────────────────────────────────────────────

static void onImageAvailable(void* ctx, AImageReader* reader) {
    reinterpret_cast<CameraController*>(ctx)->handleImage(reader);
}

static void onDeviceDisconnected(void* ctx, ACameraDevice* dev) {
    reinterpret_cast<CameraController*>(ctx)->onDisconnected(dev);
}

static void onDeviceError(void* ctx, ACameraDevice* dev, int error) {
    reinterpret_cast<CameraController*>(ctx)->onError(dev, error);
}

static void onCaptureSessionReady(void* ctx, ACameraCaptureSession* s) {
    reinterpret_cast<CameraController*>(ctx)->onSessionReady(s);
}

static void onCaptureSessionClosed(void* ctx, ACameraCaptureSession* s) {
    reinterpret_cast<CameraController*>(ctx)->onSessionClosed(s);
}

// ── open ──────────────────────────────────────────────────────────────────────

bool CameraController::open(const char* cameraId) {
    mgr_ = ACameraManager_create();

    std::string id = cameraId ? cameraId : pickBestCamera();
    if (id.empty()) { LOGE("No back camera found"); return false; }

    ACameraDevice_StateCallbacks devCb{};
    devCb.context         = this;
    devCb.onDisconnected  = onDeviceDisconnected;
    devCb.onError         = onDeviceError;

    if (ACameraManager_openCamera(mgr_, id.c_str(), &devCb, &device_) != ACAMERA_OK) {
        LOGE("openCamera failed"); return false;
    }

    // AImageReader: JPEG_420_888 → we get planar Y/U/V
    AImageReader_new(width, height,
                     AIMAGE_FORMAT_YUV_420_888,
            /*maxImages=*/4,
                     &reader_);

    AImageReader_ImageListener listener{};
    listener.context        = this;
    listener.onImageAvailable = onImageAvailable;
    AImageReader_setImageListener(reader_, &listener);

    ANativeWindow* window = nullptr;
    AImageReader_getWindow(reader_, &window);

    ACaptureSessionOutput_create(window, &output_);
    ACaptureSessionOutputContainer_create(&container_);
    ACaptureSessionOutputContainer_add(container_, output_);

    ACameraOutputTarget_create(window, &target_);
    ACameraDevice_createCaptureRequest(device_, TEMPLATE_PREVIEW, &request_);
    ACaptureRequest_addTarget(request_, target_);

    ACameraCaptureSession_stateCallbacks sessionCb{};
    sessionCb.context   = this;
    sessionCb.onReady   = onCaptureSessionReady;
    sessionCb.onClosed  = onCaptureSessionClosed;

    ACameraDevice_createCaptureSession(device_, container_, &sessionCb, &session_);
    return true;
}

// ── start / stop ──────────────────────────────────────────────────────────────

void CameraController::start() {
    if (!session_) return;
    ACameraCaptureSession_setRepeatingRequest(session_, nullptr, 1, &request_, nullptr);
    LOGE("CameraController: capture started");
}

void CameraController::stop() {
    if (session_) ACameraCaptureSession_stopRepeating(session_);
}

// ── core: NDK image → AVFrame → onFrame callback ─────────────────────────────

void CameraController::handleImage(AImageReader* reader) {
    AImage* img = nullptr;
    if (AImageReader_acquireLatestImage(reader, &img) != AMEDIA_OK || !img) return;

    int32_t imgW = 0, imgH = 0;
    AImage_getWidth(img, &imgW);
    AImage_getHeight(img, &imgH);

    // Get Y plane
    uint8_t *yData = nullptr, *uData = nullptr, *vData = nullptr;
    int yLen = 0, uLen = 0, vLen = 0;
    int yStride = 0, uStride = 0, vStride = 0;
    int uPixelStride = 0, vPixelStride = 0;

    AImage_getPlaneData(img, 0, &yData, &yLen);
    AImage_getPlaneData(img, 1, &uData, &uLen);
    AImage_getPlaneData(img, 2, &vData, &vLen);
    AImage_getPlaneRowStride(img, 0, &yStride);
    AImage_getPlaneRowStride(img, 1, &uStride);
    AImage_getPlaneRowStride(img, 2, &vStride);
    AImage_getPlanePixelStride(img, 1, &uPixelStride);
    AImage_getPlanePixelStride(img, 2, &vPixelStride);

    std::lock_guard<std::mutex> lock(frameMtx_);

    // Allocate reusable YUV420P output frame
    if (!yuv_) {
        yuv_ = av_frame_alloc();
        yuv_->format = AV_PIX_FMT_YUV420P;
        yuv_->width  = imgW;
        yuv_->height = imgH;
        av_frame_get_buffer(yuv_, 32);
    }

    // YUV_420_888 can be semi-planar (NV12/NV21) or fully planar.
    // uPixelStride == 2 → semi-planar (interleaved UV) → use sws_scale.
    // uPixelStride == 1 → already planar → copy directly.

    if (uPixelStride == 1) {
        // Fully planar: copy planes directly
        for (int row = 0; row < imgH; row++)
            memcpy(yuv_->data[0] + row * yuv_->linesize[0],
                   yData + row * yStride, imgW);

        for (int row = 0; row < imgH / 2; row++) {
            memcpy(yuv_->data[1] + row * yuv_->linesize[1],
                   uData + row * uStride, imgW / 2);
            memcpy(yuv_->data[2] + row * yuv_->linesize[2],
                   vData + row * vStride, imgW / 2);
        }
    } else {
        // Semi-planar (NV21 most common on Android): de-interleave UV
        // Y plane copy
        for (int row = 0; row < imgH; row++)
            memcpy(yuv_->data[0] + row * yuv_->linesize[0],
                   yData + row * yStride, imgW);

        // De-interleave UV (vData points to V first in NV21, uData to U)
        int uvRows = imgH / 2;
        int uvCols = imgW / 2;
        for (int row = 0; row < uvRows; row++) {
            uint8_t* uRow = uData + row * uStride;
            uint8_t* vRow = vData + row * vStride;
            uint8_t* dstU = yuv_->data[1] + row * yuv_->linesize[1];
            uint8_t* dstV = yuv_->data[2] + row * yuv_->linesize[2];
            for (int col = 0; col < uvCols; col++) {
                dstU[col] = uRow[col * uPixelStride];
                dstV[col] = vRow[col * vPixelStride];
            }
        }
    }

    yuv_->pts = AV_NOPTS_VALUE; // render loop handles timing

    AImage_delete(img);

    if (renderer != nullptr) {
        AVFrame* cloned = av_frame_clone(yuv_);
        renderFrame(renderer, cloned);  // egl_renderer.cpp handles GL thread dispatch
    }
}

// ── helpers ───────────────────────────────────────────────────────────────────

std::string CameraController::pickBestCamera() {
    ACameraIdList* list = nullptr;
    ACameraManager_getCameraIdList(mgr_, &list);
    std::string chosen;
    for (int i = 0; i < list->numCameras; i++) {
        ACameraMetadata* meta = nullptr;
        ACameraManager_getCameraCharacteristics(mgr_, list->cameraIds[i], &meta);
        ACameraMetadata_const_entry entry{};
        ACameraMetadata_getConstEntry(meta, ACAMERA_LENS_FACING, &entry);
        if (entry.data.u8[0] == ACAMERA_LENS_FACING_BACK) {
            chosen = list->cameraIds[i];
            ACameraMetadata_free(meta);
            break;
        }
        ACameraMetadata_free(meta);
    }
    ACameraManager_deleteCameraIdList(list);
    LOGE("camera mode %s", chosen.c_str());
    return chosen;
}

void CameraController::release() {
    stop();
    if (session_)   { ACameraCaptureSession_close(session_); session_ = nullptr; }
    if (request_)   { ACaptureRequest_free(request_); request_ = nullptr; }
    if (target_)    { ACameraOutputTarget_free(target_); target_ = nullptr; }
    if (container_) { ACaptureSessionOutputContainer_free(container_); container_ = nullptr; }
    if (output_)    { ACaptureSessionOutput_free(output_); output_ = nullptr; }
    if (device_)    { ACameraDevice_close(device_); device_ = nullptr; }
    if (reader_)    { AImageReader_delete(reader_); reader_ = nullptr; }
    if (mgr_)       { ACameraManager_delete(mgr_); mgr_ = nullptr; }
    if (yuv_)       { av_frame_free(&yuv_); }
    if (sws_)       { sws_freeContext(sws_); sws_ = nullptr; }
}

void CameraController::onDisconnected(ACameraDevice*) { LOGE("Camera disconnected"); }
void CameraController::onError(ACameraDevice*, int e) { LOGE("Camera error: %d", e); }
void CameraController::onSessionReady(ACameraCaptureSession*) { LOGE("Session ready"); }
void CameraController::onSessionClosed(ACameraCaptureSession*) { LOGE("Session closed"); }


void CameraController::setMode(CaptureMode mode) {
    std::lock_guard<std::mutex> lock(frameMtx_);
    if (mode_ == CaptureMode::VIDEO && isRecording_) {
        LOGE("stopRecording before switching mode");
        return;
    }
    mode_ = mode;

    // Switch capture template on the request
    if (session_) {
        ACaptureRequest_free(request_);
        request_ = nullptr;
        ACameraDevice_request_template tmpl = (mode == CaptureMode::VIDEO)
                                ? TEMPLATE_RECORD
                                : TEMPLATE_PREVIEW;
        ACameraDevice_createCaptureRequest(device_, tmpl, &request_);
        ACaptureRequest_addTarget(request_, target_);
        applyQualityToRequest();
        // restart repeating with new template
        ACameraCaptureSession_stopRepeating(session_);
        ACameraCaptureSession_setRepeatingRequest(session_, nullptr, 1, &request_, nullptr);
    }
    LOGE("Mode switched to %d", (int)mode);
}

void CameraController::capturePhoto(const char* outputPath,
                                    std::function<void(const std::string&)> cb) {
    std::lock_guard<std::mutex> lock(frameMtx_);
    photoPath_     = outputPath;
    photoCallback_ = cb;
    pendingPhoto_  = true;  // consumed on next handleImage()
}

void CameraController::startRecording(const char* outputPath) {
    std::lock_guard<std::mutex> lock(frameMtx_);
    if (mode_ != CaptureMode::VIDEO) {
        LOGE("Call setMode(VIDEO) first");
        return;
    }
    recordingPath_ = outputPath;
    isRecording_   = true;
    LOGE("Recording started → %s", outputPath);
}

void CameraController::stopRecording() {
    std::lock_guard<std::mutex> lock(frameMtx_);
    isRecording_ = false;
    LOGE("Recording stopped");
}

void CameraController::setResolution(int presetIndex) {
    if (presetIndex < 0 || presetIndex > 3) return;
    const auto& r = RESOLUTIONS[presetIndex];
    setResolution(r.width, r.height);
}

void CameraController::setResolution(int w, int h) {
    if (width == w && height == h) return;
    width  = w;
    height = h;

    // Free cached sws + yuv frame — will be reallocated at new size
    if (sws_) { sws_freeContext(sws_); sws_ = nullptr; }
    if (yuv_) { av_frame_free(&yuv_);  yuv_ = nullptr; }

    // Reopen session with new resolution
    if (session_) reopenSession();
    LOGE("Resolution set to %dx%d", w, h);
}

void CameraController::reopenSession() {
    // Stop current session cleanly
    ACameraCaptureSession_stopRepeating(session_);
    ACameraCaptureSession_close(session_);  session_   = nullptr;
    ACaptureRequest_free(request_);         request_   = nullptr;
    ACameraOutputTarget_free(target_);      target_    = nullptr;
    ACaptureSessionOutputContainer_free(container_); container_ = nullptr;
    ACaptureSessionOutput_free(output_);    output_    = nullptr;
    AImageReader_delete(reader_);           reader_    = nullptr;

    // Recreate with updated width/height
    AImageReader_new(width, height, AIMAGE_FORMAT_YUV_420_888, 4, &reader_);

    AImageReader_ImageListener listener{};
    listener.context          = this;
    listener.onImageAvailable = onImageAvailable;  // static trampoline
    AImageReader_setImageListener(reader_, &listener);

    ANativeWindow* window = nullptr;
    AImageReader_getWindow(reader_, &window);

    ACaptureSessionOutput_create(window, &output_);
    ACaptureSessionOutputContainer_create(&container_);
    ACaptureSessionOutputContainer_add(container_, output_);

    ACameraOutputTarget_create(window, &target_);
    ACameraDevice_request_template tmpl = (mode_ == CaptureMode::VIDEO)
                            ? TEMPLATE_RECORD : TEMPLATE_PREVIEW;
    ACameraDevice_createCaptureRequest(device_, tmpl, &request_);
    ACaptureRequest_addTarget(request_, target_);
    applyQualityToRequest();

    ACameraCaptureSession_stateCallbacks sessionCb{};
    sessionCb.context  = this;
    sessionCb.onReady  = onCaptureSessionReady;
    sessionCb.onClosed = onCaptureSessionClosed;
    ACameraDevice_createCaptureSession(device_, container_, &sessionCb, &session_);

    ACameraCaptureSession_setRepeatingRequest(session_, nullptr, 1, &request_, nullptr);
    LOGE("Session reopened at %dx%d", width, height);
}

void CameraController::setPreviewQuality(int presetIndex) {
    if (presetIndex < 0 || presetIndex > 3) return;
    qualityIndex_ = presetIndex;
    swsFlags_     = QUALITY_PRESETS[presetIndex].swsFlags;

    // Rebuild sws context with new flags at next handleImage()
    if (sws_) { sws_freeContext(sws_); sws_ = nullptr; }

    // Push new controls to camera HAL immediately
    if (request_ && session_) {
        applyQualityToRequest();
        // Re-issue repeating request with updated controls
        ACameraCaptureSession_stopRepeating(session_);
        ACameraCaptureSession_setRepeatingRequest(session_, nullptr, 1, &request_, nullptr);
    }
    LOGE("Preview quality set to %s", QUALITY_PRESETS[presetIndex].label);
}

void CameraController::applyQualityToRequest() {
    if (!request_) return;
    const auto& q = QUALITY_PRESETS[qualityIndex_];

    // FPS range
    int32_t fpsRange[2] = { q.fpsMin, q.fpsMax };
    ACaptureRequest_setEntry_i32(request_,
                                 ACAMERA_CONTROL_AE_TARGET_FPS_RANGE, 2, fpsRange);

    // Noise reduction
    ACaptureRequest_setEntry_u8(request_,
                                ACAMERA_NOISE_REDUCTION_MODE, 1, &q.noiseReduction);

    // Edge enhancement
    ACaptureRequest_setEntry_u8(request_,
                                ACAMERA_EDGE_MODE, 1, &q.edgeMode);

    // Auto white balance on (improves colour accuracy in preview)
    uint8_t awb = ACAMERA_CONTROL_AWB_MODE_AUTO;
    ACaptureRequest_setEntry_u8(request_,
                                ACAMERA_CONTROL_AWB_MODE, 1, &awb);

    // Continuous auto-focus
    uint8_t af = ACAMERA_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
    ACaptureRequest_setEntry_u8(request_,
                                ACAMERA_CONTROL_AF_MODE, 1, &af);
}

void CameraController::handlePreviewFrame(AVFrame* frame) {
    if (renderer) renderFrame(renderer, frame);
    else      av_frame_free(&frame);
}

void CameraController::handlePhotoFrame(AVFrame* frame) {
    if (!frame) return;
    bool ok = saveFrameAsJpeg(frame, photoPath_);
    av_frame_free(&frame);
    if (photoCallback_)
        photoCallback_(ok ? photoPath_ : "");
}

void CameraController::handleVideoFrame(AVFrame* frame) {
    // plug into your existing MuxerVideo / FFmpeg encode pipeline here
    // e.g.  avcodec_send_frame(encCtx_, frame);
    av_frame_free(&frame);
}

bool CameraController::saveFrameAsJpeg(AVFrame* src, const std::string& path) {
    /*const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    AVCodecContext* ctx  = avcodec_alloc_context3(codec);
    ctx->pix_fmt    = AV_PIX_FMT_YUVJ420P;
    ctx->width      = src->width;
    ctx->height     = src->height;
    ctx->time_base  = { 1, 25 };
    avcodec_open2(ctx, codec, nullptr);

    AVPacket* pkt = av_packet_alloc();
    avcodec_send_frame(ctx, src);
    bool ok = false;
    if (avcodec_receive_packet(ctx, pkt) == 0) {
        FILE* f = fopen(path.c_str(), "wb");
        if (f) { fwrite(pkt->data, 1, pkt->size, f); fclose(f); ok = true; }
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    avcodec_free_context(&ctx);
    return ok;
     */
    return true;
}