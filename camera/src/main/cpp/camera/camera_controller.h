//
// Created by loannth20 on 1/4/26.
//

#ifndef CAMERAVINTAGE_CAMERA_CONTROLLER_H
#define CAMERAVINTAGE_CAMERA_CONTROLLER_H

#pragma once

#include "../gl/egl_renderer.h"
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraCaptureSession.h>
#include <media/NdkImageReader.h>
#include <mutex>
#include <string>
#include <functional>

extern "C" {
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

// ── Feature 1: capture mode ───────────────────────────────────────────────────

enum class CaptureMode {
    PREVIEW,   // live preview only → renderFrame each frame
    PHOTO,     // single shot      → capture one frame, write JPEG, back to PREVIEW
    VIDEO      // recording        → encode each frame via MuxerVideo pipeline
};

// ── Feature 2: resolution presets ────────────────────────────────────────────

struct Resolution {
    int width;
    int height;
    const char* label;
};

// Ordered low → high quality. Pick one via setResolution().
static constexpr Resolution RESOLUTIONS[] = {
        { 640,  480,  "480p"  },
        { 1280, 720,  "720p"  },
        { 1920, 1080, "1080p" },
        { 3840, 2160, "4K"    },
};

// ── Feature 3: GL preview quality ────────────────────────────────────────────

struct PreviewQuality {
    // sws_scale flag used when converting YUV → YUV420P
    int swsFlags;
    // ACAMERA_CONTROL_AE_TARGET_FPS_RANGE applied to capture request
    int fpsMin;
    int fpsMax;
    // ACAMERA_NOISE_REDUCTION_MODE applied to capture request
    uint8_t noiseReduction; // ACAMERA_NOISE_REDUCTION_MODE_*
    // ACAMERA_EDGE_MODE applied to capture request
    uint8_t edgeMode;       // ACAMERA_EDGE_MODE_*
    const char* label;
};

static constexpr PreviewQuality QUALITY_PRESETS[] = {
        // label        swsFlags                           fps     NR                                    edge
        { SWS_FAST_BILINEAR,                          15, 30, ACAMERA_NOISE_REDUCTION_MODE_OFF,        ACAMERA_EDGE_MODE_OFF,        "low"    },
        { SWS_BILINEAR,                               24, 30, ACAMERA_NOISE_REDUCTION_MODE_FAST,       ACAMERA_EDGE_MODE_FAST,       "medium" },
        { SWS_BILINEAR | SWS_FULL_CHR_H_INT,          30, 30, ACAMERA_NOISE_REDUCTION_MODE_HIGH_QUALITY, ACAMERA_EDGE_MODE_HIGH_QUALITY, "high" },
        { SWS_BICUBIC  | SWS_FULL_CHR_H_INT | SWS_ACCURATE_RND, 30, 30, ACAMERA_NOISE_REDUCTION_MODE_HIGH_QUALITY, ACAMERA_EDGE_MODE_HIGH_QUALITY, "ultra" },
};

// ─────────────────────────────────────────────────────────────────────────────

class CameraController {
public:

    // ── Configuration (set before open()) ────────────────────────────────────

    int width  = 1280;
    int height = 720;
    EGLRenderer *renderer;

    // ── Feature 1: mode ──────────────────────────────────────────────────────

    // Switch mode at runtime — safe to call while running
    void setMode(CaptureMode mode);

    // Trigger a single photo capture (only valid in PHOTO mode)
    // callback receives the saved file path
    void capturePhoto(const char* outputPath,
                      std::function<void(const std::string&)> callback);

    // Start/stop video recording (only valid in VIDEO mode)
    void startRecording(const char* outputPath);
    void stopRecording();

    CaptureMode getMode() const { return mode_; }

    // ── Feature 2: resolution ────────────────────────────────────────────────

    // Apply a resolution preset — reopens the AImageReader + session
    // index: 0=480p  1=720p  2=1080p  3=4K
    void setResolution(int presetIndex);

    // Or set exact dimensions
    void setResolution(int w, int h);

    // ── Feature 3: preview quality ───────────────────────────────────────────

    // Apply a quality preset — updates sws flags + camera controls
    // index: 0=low  1=medium  2=high  3=ultra
    void setPreviewQuality(int presetIndex);

    // ── Lifecycle ────────────────────────────────────────────────────────────

    bool open(const char* cameraId = nullptr);
    void start();
    void stop();
    void release();

    // ── NDK callback handlers (public for C trampolines) ─────────────────────

    void handleImage(AImageReader* reader);
    void onDisconnected(ACameraDevice* dev);
    void onError(ACameraDevice* dev, int error);
    void onSessionReady(ACameraCaptureSession* s);
    void onSessionClosed(ACameraCaptureSession* s);

private:

    // ── Mode state ───────────────────────────────────────────────────────────
    CaptureMode  mode_           = CaptureMode::PREVIEW;
    bool         isRecording_    = false;
    std::string  recordingPath_;
    std::string  photoPath_;
    std::function<void(const std::string&)> photoCallback_;
    bool         pendingPhoto_   = false;  // set by capturePhoto(), consumed in handleImage()

    // ── Quality state ────────────────────────────────────────────────────────
    int          qualityIndex_   = 1;      // default: medium
    int          swsFlags_       = SWS_BILINEAR;

    // ── Camera NDK ───────────────────────────────────────────────────────────
    ACameraManager*                  mgr_       = nullptr;
    ACameraDevice*                   device_    = nullptr;
    ACaptureSessionOutput*           output_    = nullptr;
    ACaptureSessionOutputContainer*  container_ = nullptr;
    ACameraCaptureSession*           session_   = nullptr;
    ACaptureRequest*                 request_   = nullptr;
    ACameraOutputTarget*             target_    = nullptr;
    AImageReader*                    reader_    = nullptr;

    // ── Frame conversion ─────────────────────────────────────────────────────
    AVFrame*    yuv_      = nullptr;
    SwsContext* sws_      = nullptr;
    std::mutex  frameMtx_;

    // ── Internal helpers ─────────────────────────────────────────────────────
    std::string pickBestCamera();

    // Rebuild AImageReader + session with current width/height
    void reopenSession();

    // Apply current quality controls to the capture request
    void applyQualityToRequest();

    // Per-mode frame handling called from handleImage()
    void handlePreviewFrame(AVFrame* frame);
    void handlePhotoFrame(AVFrame* frame);
    void handleVideoFrame(AVFrame* frame);

    // Write AVFrame as JPEG to disk (for photo capture)
    bool saveFrameAsJpeg(AVFrame* frame, const std::string& path);
};

#endif //CAMERAVINTAGE_CAMERA_CONTROLLER_H
