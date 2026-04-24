//
// Video recorder implementation. See video_encoder.h.
//

#include "video_encoder.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>

#include <media/NdkMediaFormat.h>

extern "C" {
#include <libavutil/frame.h>
}

namespace {
// MediaCodec MIME + color format constants are not all exposed as symbols by
// the NDK; use the canonical string/int values documented in MediaFormat.
constexpr const char* kMimeH264               = "video/avc";
constexpr int32_t     kColorFormatSemiPlanar  = 21;     // COLOR_FormatYUV420SemiPlanar (NV12)
constexpr int32_t     kBFrameInterval         = 1;      // keyframe every 1s
constexpr int64_t     kDequeueTimeoutUs       = 10000;  // 10 ms

// AMediaCodec_BUFFER_FLAG_END_OF_STREAM / KEY_FRAME are exported by the NDK
// but guard for headers that predate them.
#ifndef AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM
#define AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM 4
#endif
#ifndef AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG
#define AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG 2
#endif
}

VideoEncoder::VideoEncoder() = default;

VideoEncoder::~VideoEncoder() {
    if (thread_) {
        delete thread_;
        thread_ = nullptr;
    }
    closeAll();
}

bool VideoEncoder::start(const std::string& outputPath,
                         int width, int height,
                         int fps, int bitrate) {
    if (running_.load(std::memory_order_acquire)) {
        LOGE("VideoEncoder::start called while already running");
        return false;
    }
    if (width <= 0 || height <= 0 || fps <= 0 || bitrate <= 0) {
        LOGE("VideoEncoder::start invalid params %dx%d@%d bitrate=%d",
             width, height, fps, bitrate);
        return false;
    }

    // H.264 encoders require even dimensions; round down.
    width_  = width  & ~1;
    height_ = height & ~1;
    fps_    = fps;
    path_   = outputPath;
    frameCount_ = 0;

    // Create codec first so we can fail fast without a stale output file.
    codec_ = AMediaCodec_createEncoderByType(kMimeH264);
    if (!codec_) { LOGE("AMediaCodec_createEncoderByType failed"); closeAll(); return false; }

    AMediaFormat* fmt = AMediaFormat_new();
    AMediaFormat_setString(fmt, AMEDIAFORMAT_KEY_MIME, kMimeH264);
    AMediaFormat_setInt32 (fmt, AMEDIAFORMAT_KEY_WIDTH,  width_);
    AMediaFormat_setInt32 (fmt, AMEDIAFORMAT_KEY_HEIGHT, height_);
    AMediaFormat_setInt32 (fmt, AMEDIAFORMAT_KEY_BIT_RATE, bitrate);
    AMediaFormat_setInt32 (fmt, AMEDIAFORMAT_KEY_FRAME_RATE, fps_);
    AMediaFormat_setInt32 (fmt, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, kBFrameInterval);
    AMediaFormat_setInt32 (fmt, AMEDIAFORMAT_KEY_COLOR_FORMAT, kColorFormatSemiPlanar);

    media_status_t s = AMediaCodec_configure(codec_, fmt, nullptr, nullptr,
                                             AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
    AMediaFormat_delete(fmt);
    if (s != AMEDIA_OK) {
        LOGE("AMediaCodec_configure failed: %d", s);
        closeAll();
        return false;
    }

    if (AMediaCodec_start(codec_) != AMEDIA_OK) {
        LOGE("AMediaCodec_start failed");
        closeAll();
        return false;
    }

    // MediaMuxer needs an fd — it doesn't take a path directly.
    muxerFd_ = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (muxerFd_ < 0) {
        LOGE("open(%s) failed: %s", path_.c_str(), strerror(errno));
        closeAll();
        return false;
    }

    muxer_ = AMediaMuxer_new(muxerFd_, AMEDIAMUXER_OUTPUT_FORMAT_MPEG_4);
    if (!muxer_) {
        LOGE("AMediaMuxer_new failed");
        closeAll();
        return false;
    }

    // Track is added later, when the codec emits its INFO_OUTPUT_FORMAT_CHANGED
    // containing the real csd-0/csd-1 (SPS/PPS). Start the muxer at that point.
    trackIndex_   = -1;
    muxerStarted_ = false;

    if (!thread_) thread_ = new SingleThreadExecutor("video_encoder");

    running_.store(true, std::memory_order_release);
    LOGI("VideoEncoder started %dx%d@%dfps bitrate=%d → %s",
         width_, height_, fps_, bitrate, path_.c_str());
    return true;
}

void VideoEncoder::encodeFrame(AVFrame* frame) {
    if (!running_.load(std::memory_order_acquire) || !frame) {
        if (frame) av_frame_free(&frame);
        return;
    }
    // Clone a ref so the caller can free their frame immediately.
    AVFrame* owned = av_frame_clone(frame);
    av_frame_free(&frame);
    if (!owned) return;

    thread_->runInThread([this, owned]() mutable {
        AVFrame* f = owned;
        if (!running_.load(std::memory_order_acquire) || !codec_) {
            av_frame_free(&f);
            return;
        }

        ssize_t inIdx = AMediaCodec_dequeueInputBuffer(codec_, kDequeueTimeoutUs);
        if (inIdx < 0) {
            // Drop frame if encoder is backed up — preserves real-time cadence.
            av_frame_free(&f);
            drainOutput(false);
            return;
        }

        size_t capacity = 0;
        uint8_t* buf = AMediaCodec_getInputBuffer(codec_, inIdx, &capacity);
        const size_t needed = static_cast<size_t>(width_) * height_ * 3 / 2;
        if (!buf || capacity < needed) {
            LOGE("Input buffer too small: cap=%zu needed=%zu", capacity, needed);
            AMediaCodec_queueInputBuffer(codec_, inIdx, 0, 0, 0, 0);
            av_frame_free(&f);
            return;
        }

        convertPlanarToNV12(f, buf, width_, height_);

        const int64_t ptsUs = (frameCount_ * 1000000LL) / fps_;
        frameCount_++;
        AMediaCodec_queueInputBuffer(codec_, inIdx, 0, needed, ptsUs, 0);

        av_frame_free(&f);
        drainOutput(false);
    });
}

void VideoEncoder::stop(std::function<void(const std::string&)> callback) {
    if (!running_.load(std::memory_order_acquire)) {
        if (callback) callback("");
        return;
    }
    running_.store(false, std::memory_order_release);

    thread_->runInThread([this, cb = std::move(callback)]() mutable {
        // Signal end-of-stream via an empty input buffer with the EOS flag.
        if (codec_) {
            ssize_t inIdx = AMediaCodec_dequeueInputBuffer(codec_, kDequeueTimeoutUs * 10);
            if (inIdx >= 0) {
                const int64_t ptsUs = (frameCount_ * 1000000LL) / fps_;
                AMediaCodec_queueInputBuffer(
                        codec_, inIdx, 0, 0, ptsUs,
                        AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
            } else {
                LOGE("stop: could not dequeue input buffer for EOS");
            }
            drainOutput(true);
        }

        std::string result;
        if (muxerStarted_ && muxer_) {
            AMediaMuxer_stop(muxer_);
            result = path_;
        }
        closeAll();
        frameCount_ = 0;

        if (cb) cb(result);
    });
}

void VideoEncoder::drainOutput(bool endOfStream) {
    if (!codec_) return;

    while (true) {
        AMediaCodecBufferInfo info{};
        ssize_t outIdx = AMediaCodec_dequeueOutputBuffer(
                codec_, &info, endOfStream ? kDequeueTimeoutUs * 10 : 0);

        if (outIdx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
            if (!endOfStream) return;
            continue;
        }

        if (outIdx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            // Codec has produced its real output format with csd-0/csd-1 — add
            // the track and start the muxer. Must happen exactly once.
            if (!muxerStarted_ && muxer_) {
                AMediaFormat* outFmt = AMediaCodec_getOutputFormat(codec_);
                trackIndex_ = AMediaMuxer_addTrack(muxer_, outFmt);
                AMediaFormat_delete(outFmt);
                if (trackIndex_ >= 0 && AMediaMuxer_start(muxer_) == AMEDIA_OK) {
                    muxerStarted_ = true;
                } else {
                    LOGE("AMediaMuxer_start failed trackIdx=%zd", trackIndex_);
                }
            }
            continue;
        }

        if (outIdx == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
            continue;
        }

        if (outIdx < 0) {
            if (endOfStream) continue;
            return;
        }

        size_t outSize = 0;
        uint8_t* outBuf = AMediaCodec_getOutputBuffer(codec_, outIdx, &outSize);

        // Skip codec config packets — they are already embedded in the muxer
        // via the csd-* keys from the output format change.
        const bool isConfig = (info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) != 0;
        if (outBuf && !isConfig && info.size > 0 && muxerStarted_) {
            AMediaMuxer_writeSampleData(muxer_, trackIndex_, outBuf, &info);
        }
        AMediaCodec_releaseOutputBuffer(codec_, outIdx, false);

        if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) return;
    }
}

void VideoEncoder::convertPlanarToNV12(const AVFrame* src,
                                       uint8_t* dst, int width, int height) {
    // Y plane: tightly packed copy (handle stride).
    const uint8_t* sy = src->data[0];
    const int syStride = src->linesize[0];
    for (int row = 0; row < height; row++) {
        std::memcpy(dst + row * width, sy + row * syStride, width);
    }

    // UV plane: interleave U,V rows. YUV420P UV is half-res in both dims.
    uint8_t* duv = dst + width * height;
    const uint8_t* su = src->data[1];
    const uint8_t* sv = src->data[2];
    const int suStride = src->linesize[1];
    const int svStride = src->linesize[2];
    const int uvH = height / 2;
    const int uvW = width  / 2;
    for (int row = 0; row < uvH; row++) {
        uint8_t* d = duv + row * width;
        const uint8_t* u = su + row * suStride;
        const uint8_t* v = sv + row * svStride;
        for (int col = 0; col < uvW; col++) {
            d[2 * col    ] = u[col];
            d[2 * col + 1] = v[col];
        }
    }
}

void VideoEncoder::closeAll() {
    if (codec_) {
        AMediaCodec_stop(codec_);
        AMediaCodec_delete(codec_);
        codec_ = nullptr;
    }
    if (muxer_) {
        AMediaMuxer_delete(muxer_);
        muxer_ = nullptr;
    }
    if (muxerFd_ >= 0) {
        ::close(muxerFd_);
        muxerFd_ = -1;
    }
    trackIndex_   = -1;
    muxerStarted_ = false;
}
