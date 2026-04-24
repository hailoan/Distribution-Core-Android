//
// Video recorder: AVFrame (YUV420P) → AMediaCodec (H.264) → AMediaMuxer (MP4).
// Runs on its own worker thread so the camera/EGL threads never block on encode.
//

#ifndef CAMERANDK_VIDEO_ENCODER_H
#define CAMERANDK_VIDEO_ENCODER_H

#pragma once

#include <atomic>
#include <functional>
#include <string>

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaMuxer.h>

#include "../utils/single_thread_executor.h"
#include "../utils/log-android.h"

extern "C" {
#include <libavutil/frame.h>
}

class VideoEncoder {
public:
    VideoEncoder();
    ~VideoEncoder();

    // Open the encoder and muxer, begin accepting frames.
    // Safe to call only when !isRunning(). Returns false on failure (muxer/codec
    // not created, file not openable); partial state is cleaned up on failure.
    // `orientation` in degrees (0/90/180/270) is written as an MP4 rotation
    // hint — players read it to display the stream right-side up.
    bool start(const std::string& outputPath,
               int width, int height,
               int fps, int bitrate,
               int orientation = 0);

    // Queue a YUV420P frame for encoding. Takes ownership: the encoder clones
    // the frame internally and the caller must still free their own reference.
    // No-op if !isRunning().
    void encodeFrame(AVFrame* frame);

    // Flush + finalize + close muxer. callback fires on the encoder thread
    // with the output path on success, empty string on failure.
    void stop(std::function<void(const std::string&)> callback);

    bool isRunning() const { return running_.load(std::memory_order_acquire); }

private:
    AMediaCodec* codec_       = nullptr;
    AMediaMuxer* muxer_       = nullptr;
    int          muxerFd_     = -1;
    ssize_t      trackIndex_  = -1;
    bool         muxerStarted_= false;

    int width_  = 0;
    int height_ = 0;
    int fps_    = 30;

    std::atomic<bool> running_{false};
    int64_t frameCount_ = 0;
    std::string path_;

    SingleThreadExecutor* thread_ = nullptr;

    // Drain output buffers from the codec and write to the muxer.
    // If endOfStream, loop until BUFFER_FLAG_END_OF_STREAM is seen.
    void drainOutput(bool endOfStream);

    // Convert a YUV420P AVFrame into NV12 directly into the codec's input
    // buffer at `dst`. dst must be at least (stride*height + stride*height/2).
    static void convertPlanarToNV12(const AVFrame* src,
                                    uint8_t* dst, int stride, int sliceHeight);

    // Release codec + muxer + fd. Safe to call from any state.
    void closeAll();
};

#endif //CAMERANDK_VIDEO_ENCODER_H
