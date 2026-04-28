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
    if (!mgr_) mgr_ = ACameraManager_create();

    activeCameraId_ = cameraId ? cameraId : pickCameraByFacing(lens_);
    if (activeCameraId_.empty()) { LOGE("No camera found for facing %d", lens_); return false; }

    return openDevice();
}

bool CameraController::openDevice() {
    if (!mgr_ || activeCameraId_.empty()) return false;

    ACameraDevice_StateCallbacks devCb{};
    devCb.context         = this;
    devCb.onDisconnected  = onDeviceDisconnected;
    devCb.onError         = onDeviceError;

    if (ACameraManager_openCamera(mgr_, activeCameraId_.c_str(), &devCb, &device_) != ACAMERA_OK) {
        LOGE("openCamera failed"); return false;
    }

    // Refresh sensor active-array + AE compensation range for the new device.
    cacheCharacteristicsFor(activeCameraId_);

    // Honour the supported-size fallback for the selected camera.
    Resolution resolved = resolveSupportedSize(activeCameraId_, width, height);
    width  = resolved.width;
    height = resolved.height;

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
    ACameraDevice_request_template tmpl = (mode_ == CaptureMode::VIDEO)
                            ? TEMPLATE_RECORD : TEMPLATE_PREVIEW;
    ACameraDevice_createCaptureRequest(device_, tmpl, &request_);
    ACaptureRequest_addTarget(request_, target_);
    applyQualityToRequest();
    applyControlState();

    ACameraCaptureSession_stateCallbacks sessionCb{};
    sessionCb.context   = this;
    sessionCb.onReady   = onCaptureSessionReady;
    sessionCb.onClosed  = onCaptureSessionClosed;

    ACameraDevice_createCaptureSession(device_, container_, &sessionCb, &session_);

    // Push the per-camera sensor orientation into the GL preview so the
    // shader rotates upright in portrait. Back lenses are typically 90°
    // (preview rotation = -90°), front lenses 270° (preview rotation = -270°).
    if (renderer) {
        const int orient = sensorOrientationFor(activeCameraId_);
        setPreviewRotation(renderer, static_cast<float>(-orient));
    }
    return true;
}

void CameraController::closeDevice() {
    if (session_) {
        ACameraCaptureSession_stopRepeating(session_);
        ACameraCaptureSession_close(session_);
        session_ = nullptr;
    }
    if (request_)   { ACaptureRequest_free(request_); request_ = nullptr; }
    if (target_)    { ACameraOutputTarget_free(target_); target_ = nullptr; }
    if (container_) { ACaptureSessionOutputContainer_free(container_); container_ = nullptr; }
    if (output_)    { ACaptureSessionOutput_free(output_); output_ = nullptr; }
    if (device_)    { ACameraDevice_close(device_); device_ = nullptr; }
    if (reader_)    { AImageReader_delete(reader_); reader_ = nullptr; }
    if (sws_)       { sws_freeContext(sws_); sws_ = nullptr; }
    if (yuv_)       { av_frame_free(&yuv_); }
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

    // In VIDEO mode, also push the frame into the encoder. encodeFrame()
    // clones internally and drops cleanly when the encoder is backed up,
    // so this cannot stall the camera callback thread.
    if (mode_ == CaptureMode::VIDEO && isRecording_ && encoder_.isRunning()) {
        AVFrame* enc = av_frame_clone(yuv_);
        encoder_.encodeFrame(enc);
    }
}

// ── helpers ───────────────────────────────────────────────────────────────────

std::string CameraController::pickCameraByFacing(int facing) {
    if (!mgr_) return "";
    uint8_t wanted = (facing == 1)
                     ? (uint8_t)ACAMERA_LENS_FACING_FRONT
                     : (uint8_t)ACAMERA_LENS_FACING_BACK;

    ACameraIdList* list = nullptr;
    ACameraManager_getCameraIdList(mgr_, &list);
    std::string chosen;
    std::string fallback;
    for (int i = 0; i < list->numCameras; i++) {
        ACameraMetadata* meta = nullptr;
        ACameraManager_getCameraCharacteristics(mgr_, list->cameraIds[i], &meta);
        ACameraMetadata_const_entry entry{};
        if (ACameraMetadata_getConstEntry(meta, ACAMERA_LENS_FACING, &entry) == ACAMERA_OK) {
            if (fallback.empty()) fallback = list->cameraIds[i];
            if (entry.data.u8[0] == wanted) {
                chosen = list->cameraIds[i];
                ACameraMetadata_free(meta);
                break;
            }
        }
        ACameraMetadata_free(meta);
    }
    ACameraManager_deleteCameraIdList(list);
    if (chosen.empty()) chosen = fallback;
    LOGE("pickCameraByFacing(%d) → %s", facing, chosen.c_str());
    return chosen;
}

std::string CameraController::pickBestCamera() {
    return pickCameraByFacing(0);
}

int CameraController::sensorOrientationFor(const std::string& cameraId) {
    if (!mgr_ || cameraId.empty()) return 0;
    ACameraMetadata* meta = nullptr;
    if (ACameraManager_getCameraCharacteristics(mgr_, cameraId.c_str(), &meta) != ACAMERA_OK
            || !meta) {
        return 0;
    }
    int orientation = 0;
    ACameraMetadata_const_entry entry{};
    if (ACameraMetadata_getConstEntry(meta, ACAMERA_SENSOR_ORIENTATION, &entry) == ACAMERA_OK
            && entry.count > 0) {
        orientation = entry.data.i32[0];
    }
    ACameraMetadata_free(meta);
    return orientation;
}

Resolution CameraController::resolveSupportedSize(const std::string& cameraId,
                                                  int reqW, int reqH) {
    Resolution fallback{ reqW, reqH, "requested" };
    if (!mgr_ || cameraId.empty()) return fallback;

    ACameraMetadata* meta = nullptr;
    if (ACameraManager_getCameraCharacteristics(mgr_, cameraId.c_str(), &meta) != ACAMERA_OK || !meta) {
        return fallback;
    }

    ACameraMetadata_const_entry entry{};
    if (ACameraMetadata_getConstEntry(
            meta, ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &entry) != ACAMERA_OK) {
        ACameraMetadata_free(meta);
        return fallback;
    }

    // Entries are tuples of (format, width, height, input/output).
    // Output = 0, Input = 1. Match YUV_420_888 output streams only.
    const int32_t wantedFormat = AIMAGE_FORMAT_YUV_420_888;
    int bestLeW = 0, bestLeH = 0;             // largest ≤ requested
    int smallestW = 0, smallestH = 0;         // smallest overall (fallback-up)
    int exactW = 0, exactH = 0;
    const int64_t reqPixels = (int64_t)reqW * reqH;

    for (uint32_t i = 0; i < entry.count; i += 4) {
        int32_t fmt   = entry.data.i32[i];
        int32_t w     = entry.data.i32[i + 1];
        int32_t h     = entry.data.i32[i + 2];
        int32_t input = entry.data.i32[i + 3];
        if (input != 0 || fmt != wantedFormat) continue;

        if (w == reqW && h == reqH) { exactW = w; exactH = h; break; }

        const int64_t px = (int64_t)w * h;
        if (px <= reqPixels && px > (int64_t)bestLeW * bestLeH) {
            bestLeW = w; bestLeH = h;
        }
        if (smallestW == 0 || px < (int64_t)smallestW * smallestH) {
            smallestW = w; smallestH = h;
        }
    }
    ACameraMetadata_free(meta);

    if (exactW > 0) return Resolution{ exactW, exactH, "exact" };
    if (bestLeW > 0) {
        LOGE("Requested %dx%d unsupported; using %dx%d", reqW, reqH, bestLeW, bestLeH);
        return Resolution{ bestLeW, bestLeH, "fallback" };
    }
    if (smallestW > 0) {
        LOGE("No size ≤ %dx%d; using smallest supported %dx%d", reqW, reqH, smallestW, smallestH);
        return Resolution{ smallestW, smallestH, "fallback" };
    }
    return fallback;
}

void CameraController::release() {
    stop();
    closeDevice();
    if (mgr_) { ACameraManager_delete(mgr_); mgr_ = nullptr; }
}

void CameraController::onDisconnected(ACameraDevice*) { LOGE("Camera disconnected"); }
void CameraController::onError(ACameraDevice*, int e) { LOGE("Camera error: %d", e); }
void CameraController::onSessionReady(ACameraCaptureSession*) { LOGE("Session ready"); }
void CameraController::onSessionClosed(ACameraCaptureSession*) { LOGE("Session closed"); }


void CameraController::setMode(CaptureMode mode) {
    if (mode_ == CaptureMode::VIDEO && isRecording_) {
        LOGE("stopRecording before switching mode");
        return;
    }
    mode_ = mode;

    // Decide the target dimensions for (new mode, current lens).
    int targetW = width;
    int targetH = height;
    auto it = resolutionMap_.find({ (int)mode_, lens_ });
    if (it != resolutionMap_.end()) {
        Resolution r = resolveSupportedSize(activeCameraId_,
                                            it->second.width,
                                            it->second.height);
        targetW = r.width;
        targetH = r.height;
    }

    const bool dimsChanged = (targetW != width) || (targetH != height);

    if (dimsChanged) {
        {
            std::lock_guard<std::mutex> lock(frameMtx_);
            width  = targetW;
            height = targetH;
        }
        if (session_) reopenSession();    // picks template from mode_
    } else if (session_) {
        // Same size → only swap the template on the live request.
        std::lock_guard<std::mutex> lock(frameMtx_);
        ACaptureRequest_free(request_);
        request_ = nullptr;
        ACameraDevice_request_template tmpl = (mode == CaptureMode::VIDEO)
                                ? TEMPLATE_RECORD
                                : TEMPLATE_PREVIEW;
        ACameraDevice_createCaptureRequest(device_, tmpl, &request_);
        ACaptureRequest_addTarget(request_, target_);
        applyQualityToRequest();
        applyControlState();
        ACameraCaptureSession_stopRepeating(session_);
        ACameraCaptureSession_setRepeatingRequest(session_, nullptr, 1, &request_, nullptr);
    }
    LOGE("Mode switched to %d (%dx%d)", (int)mode, width, height);
}

void CameraController::setLens(int lens) {
    if (lens != 0 && lens != 1) return;
    if (lens == lens_ && device_) return;

    if (mode_ == CaptureMode::VIDEO && isRecording_) {
        LOGE("stopRecording before switching lens");
        return;
    }

    lens_ = lens;

    // Apply the resolution registered for (current mode, new lens) before
    // we reopen the device so openDevice() sizes the reader correctly.
    auto it = resolutionMap_.find({ (int)mode_, lens_ });
    if (it != resolutionMap_.end()) {
        width  = it->second.width;
        height = it->second.height;
    }

    // Close and reopen on the new facing.
    closeDevice();
    activeCameraId_ = pickCameraByFacing(lens_);
    if (activeCameraId_.empty()) {
        LOGE("setLens(%d): no matching camera id", lens_);
        return;
    }
    if (openDevice()) {
        ACameraCaptureSession_setRepeatingRequest(session_, nullptr, 1, &request_, nullptr);
    }
    LOGE("Lens switched to %d (camera %s)", lens_, activeCameraId_.c_str());
}

void CameraController::setResolutionForModeLens(int mode, int lens, int w, int h) {
    resolutionMap_[{ mode, lens }] = Resolution{ w, h, "requested" };
    // If this entry matches the currently-active combination, apply immediately.
    if (mode == (int)mode_ && lens == lens_) {
        setResolution(w, h);
    }
}

void CameraController::applyResolutionForCurrent() {
    auto it = resolutionMap_.find({ (int)mode_, lens_ });
    if (it == resolutionMap_.end()) return;
    setResolution(it->second.width, it->second.height);
}

void CameraController::capturePhoto(const char* outputPath,
                                    std::function<void(const std::string&)> cb) {
    std::lock_guard<std::mutex> lock(frameMtx_);
    photoPath_     = outputPath;
    photoCallback_ = cb;
    pendingPhoto_  = true;  // consumed on next handleImage()
}

bool CameraController::startRecording(const char* outputPath, int bitrate, int orientation) {
    std::lock_guard<std::mutex> lock(frameMtx_);
    if (mode_ != CaptureMode::VIDEO) {
        LOGE("startRecording: call setMode(VIDEO) first");
        return false;
    }
    if (isRecording_) {
        LOGE("startRecording: already recording");
        return false;
    }

    // Use the HAL upper FPS from the active quality preset, fall back to 30.
    const int fps = (qualityIndex_ >= 0 && qualityIndex_ < 4)
                    ? QUALITY_PRESETS[qualityIndex_].fpsMax
                    : 30;
    // Default bitrate: ~0.12 bpp at target fps → ~6 Mbps for 1080p30.
    const int br = (bitrate > 0)
                   ? bitrate
                   : static_cast<int>(static_cast<int64_t>(width) * height * fps * 12 / 100);

    if (!encoder_.start(outputPath, width, height, fps, br, orientation)) {
        LOGE("startRecording: encoder open failed");
        return false;
    }
    recordingPath_ = outputPath;
    isRecording_   = true;
    LOGI("Recording started %dx%d@%d %dbps rot=%d → %s",
         width, height, fps, br, orientation, outputPath);
    return true;
}

void CameraController::stopRecording(std::function<void(const std::string&)> cb) {
    {
        std::lock_guard<std::mutex> lock(frameMtx_);
        if (!isRecording_) {
            if (cb) cb("");
            return;
        }
        isRecording_ = false;
    }
    // Finalize outside the frame lock — encoder drains on its own thread.
    encoder_.stop(std::move(cb));
    LOGI("Recording stop requested");
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
    applyControlState();

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
        applyControlState();
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
    // AF mode is owned by applyControlState() so the caller's lock/unlock
    // intent (and tap-to-focus regions) survives quality preset changes.
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

// ── Camera control: tap-to-focus / AF lock / AE compensation ─────────────────

void CameraController::cacheCharacteristicsFor(const std::string& cameraId) {
    if (!mgr_ || cameraId.empty()) return;
    ACameraMetadata* meta = nullptr;
    if (ACameraManager_getCameraCharacteristics(mgr_, cameraId.c_str(), &meta) != ACAMERA_OK
            || !meta) {
        return;
    }

    ACameraMetadata_const_entry e{};

    // Sensor active array — used for view→sensor coordinate mapping.
    if (ACameraMetadata_getConstEntry(meta, ACAMERA_SENSOR_INFO_ACTIVE_ARRAY_SIZE, &e) == ACAMERA_OK
            && e.count >= 4) {
        sensorActive_[0] = e.data.i32[0];
        sensorActive_[1] = e.data.i32[1];
        sensorActive_[2] = e.data.i32[2];
        sensorActive_[3] = e.data.i32[3];
    }

    // Sensor orientation (cached for focusAt mapping).
    sensorOrientationCached_ = sensorOrientationFor(cameraId);

    // Lens facing — front lens needs a horizontal flip in the view→sensor map.
    lensIsFront_ = false;
    if (ACameraMetadata_getConstEntry(meta, ACAMERA_LENS_FACING, &e) == ACAMERA_OK && e.count > 0) {
        lensIsFront_ = (e.data.u8[0] == ACAMERA_LENS_FACING_FRONT);
    }

    // AE compensation range (HAL steps).
    if (ACameraMetadata_getConstEntry(meta, ACAMERA_CONTROL_AE_COMPENSATION_RANGE, &e) == ACAMERA_OK
            && e.count >= 2) {
        aeCompMin_ = e.data.i32[0];
        aeCompMax_ = e.data.i32[1];
    } else {
        aeCompMin_ = 0;
        aeCompMax_ = 0;
    }
    // Clamp any previously-set value into the new range.
    if (aeCompCurrent_ < aeCompMin_) aeCompCurrent_ = aeCompMin_;
    if (aeCompCurrent_ > aeCompMax_) aeCompCurrent_ = aeCompMax_;

    ACameraMetadata_free(meta);
    LOGE("Sensor active=[%d,%d,%d,%d] orient=%d front=%d AE range=[%d,%d]",
         sensorActive_[0], sensorActive_[1], sensorActive_[2], sensorActive_[3],
         sensorOrientationCached_, (int)lensIsFront_, aeCompMin_, aeCompMax_);
}

void CameraController::applyControlState() {
    if (!request_) return;

    // AF mode: continuous when unlocked (matches mode), AUTO + IDLE when locked.
    uint8_t afMode;
    if (focusLocked_) {
        // AUTO + TRIGGER_IDLE keeps the lens parked at its last converged
        // position with no further hunts. CONTINUOUS_* would still re-evaluate.
        afMode = ACAMERA_CONTROL_AF_MODE_AUTO;
    } else {
        afMode = (mode_ == CaptureMode::PHOTO)
                 ? ACAMERA_CONTROL_AF_MODE_CONTINUOUS_PICTURE
                 : ACAMERA_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
    }
    ACaptureRequest_setEntry_u8(request_, ACAMERA_CONTROL_AF_MODE, 1, &afMode);

    uint8_t afTrigger = ACAMERA_CONTROL_AF_TRIGGER_IDLE;
    ACaptureRequest_setEntry_u8(request_, ACAMERA_CONTROL_AF_TRIGGER, 1, &afTrigger);

    // AF / AE regions if a tap-to-focus has been issued.
    if (hasAfRegion_) {
        ACaptureRequest_setEntry_i32(request_, ACAMERA_CONTROL_AF_REGIONS, 5, afRegion_);
        ACaptureRequest_setEntry_i32(request_, ACAMERA_CONTROL_AE_REGIONS, 5, aeRegion_);
    }

    // AE compensation in HAL steps.
    int32_t ev = aeCompCurrent_;
    ACaptureRequest_setEntry_i32(request_, ACAMERA_CONTROL_AE_EXPOSURE_COMPENSATION, 1, &ev);
}

void CameraController::focusAt(float viewX, float viewY, float viewW, float viewH) {
    if (!session_ || !request_ || viewW <= 0.f || viewH <= 0.f) return;

    const int32_t saL = sensorActive_[0];
    const int32_t saT = sensorActive_[1];
    const int32_t saR = sensorActive_[2];
    const int32_t saB = sensorActive_[3];
    const int32_t saW = saR - saL;
    const int32_t saH = saB - saT;
    if (saW <= 0 || saH <= 0) return;

    // Normalize touch into [0,1] view-space.
    float nx = viewX / viewW;
    float ny = viewY / viewH;
    if (nx < 0.f) nx = 0.f; else if (nx > 1.f) nx = 1.f;
    if (ny < 0.f) ny = 0.f; else if (ny > 1.f) ny = 1.f;

    // Front lens preview is mirrored horizontally for the user — undo it.
    if (lensIsFront_) nx = 1.f - nx;

    // Rotate from view space to sensor space using ACAMERA_SENSOR_ORIENTATION.
    // The sensor orientation is the clockwise angle the image must be rotated
    // to appear upright in the view, so going view→sensor rotates the *opposite*
    // way: 90 → (sx = ny, sy = 1 - nx); 270 → (sx = 1 - ny, sy = nx).
    float sx, sy;
    switch (sensorOrientationCached_) {
        case 90:  sx = ny;        sy = 1.f - nx;  break;
        case 180: sx = 1.f - nx;  sy = 1.f - ny;  break;
        case 270: sx = 1.f - ny;  sy = nx;        break;
        default:  sx = nx;        sy = ny;        break;
    }

    // ~10% of the sensor short edge → reasonable AF/AE box.
    const int32_t boxHalf = (saW < saH ? saW : saH) / 20;
    int32_t cx = saL + (int32_t)(sx * saW);
    int32_t cy = saT + (int32_t)(sy * saH);
    int32_t xmin = cx - boxHalf; if (xmin < saL) xmin = saL;
    int32_t ymin = cy - boxHalf; if (ymin < saT) ymin = saT;
    int32_t xmax = cx + boxHalf; if (xmax > saR) xmax = saR;
    int32_t ymax = cy + boxHalf; if (ymax > saB) ymax = saB;

    afRegion_[0] = aeRegion_[0] = xmin;
    afRegion_[1] = aeRegion_[1] = ymin;
    afRegion_[2] = aeRegion_[2] = xmax;
    afRegion_[3] = aeRegion_[3] = ymax;
    afRegion_[4] = aeRegion_[4] = 1000;  // weight: HAL accepts 0..1000
    hasAfRegion_ = true;

    // Tap-to-focus implies an explicit re-focus, so unlock if previously locked.
    focusLocked_ = false;

    // For AF_TRIGGER_START to re-converge, the AF mode must allow it. Use AUTO
    // for the trigger so the next capture forces a fresh AF cycle, then resume
    // continuous on the repeating request.
    uint8_t autoMode = ACAMERA_CONTROL_AF_MODE_AUTO;
    uint8_t triggerStart = ACAMERA_CONTROL_AF_TRIGGER_START;

    ACaptureRequest_setEntry_u8(request_, ACAMERA_CONTROL_AF_MODE, 1, &autoMode);
    ACaptureRequest_setEntry_u8(request_, ACAMERA_CONTROL_AF_TRIGGER, 1, &triggerStart);
    ACaptureRequest_setEntry_i32(request_, ACAMERA_CONTROL_AF_REGIONS, 5, afRegion_);
    ACaptureRequest_setEntry_i32(request_, ACAMERA_CONTROL_AE_REGIONS, 5, aeRegion_);

    // Submit a single capture to fire the AF_TRIGGER_START edge.
    ACameraCaptureSession_capture(session_, nullptr, 1, &request_, nullptr);

    // Re-arm the repeating request with continuous AF + regions still set.
    applyControlState();
    ACameraCaptureSession_stopRepeating(session_);
    ACameraCaptureSession_setRepeatingRequest(session_, nullptr, 1, &request_, nullptr);

    LOGE("focusAt view=(%.1f,%.1f)/(%.1f,%.1f) → sensor=[%d,%d,%d,%d]",
         viewX, viewY, viewW, viewH, xmin, ymin, xmax, ymax);
}

void CameraController::lockFocus(bool locked) {
    focusLocked_ = locked;
    if (!session_ || !request_) return;
    applyControlState();
    ACameraCaptureSession_stopRepeating(session_);
    ACameraCaptureSession_setRepeatingRequest(session_, nullptr, 1, &request_, nullptr);
    LOGE("lockFocus(%d)", (int)locked);
}

void CameraController::setExposureCompensation(int ev) {
    if (ev < aeCompMin_) ev = aeCompMin_;
    if (ev > aeCompMax_) ev = aeCompMax_;
    aeCompCurrent_ = ev;
    if (!session_ || !request_) return;

    int32_t v = aeCompCurrent_;
    ACaptureRequest_setEntry_i32(request_, ACAMERA_CONTROL_AE_EXPOSURE_COMPENSATION, 1, &v);
    ACameraCaptureSession_stopRepeating(session_);
    ACameraCaptureSession_setRepeatingRequest(session_, nullptr, 1, &request_, nullptr);
    LOGE("setExposureCompensation %d (range [%d,%d])", aeCompCurrent_, aeCompMin_, aeCompMax_);
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