#include "h264_encoder.h"

#include <android/log.h>
#include <media/NdkMediaFormat.h>

#include <cstring>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
}

#define LOG_TAG "videolib.export.h264"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {
    constexpr const char *kMimeH264 = "video/avc";
    constexpr int32_t kColorFormatSemiPlanar = 21; // COLOR_FormatYUV420SemiPlanar (NV12)
    constexpr int32_t kIFrameIntervalSec = 1;
    constexpr int64_t kDequeueTimeoutUs = 10000;

#ifndef AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM
#define AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM 4
#endif
#ifndef AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG
#define AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG 2
#endif
#ifndef AMEDIACODEC_BUFFER_FLAG_KEY_FRAME
#define AMEDIACODEC_BUFFER_FLAG_KEY_FRAME 1
#endif
}

H264Encoder::~H264Encoder() {
    closeCodec();
    sws_freeContext(sws_);
    sws_ = nullptr;
}

bool H264Encoder::start(int width, int height, int fps, int bitrate,
                        ConfigSink configSink, SampleSink sampleSink) {
    if (started_ || width <= 0 || height <= 0 || fps <= 0 || bitrate <= 0) {
        return false;
    }
    width_ = width & ~1;   // H.264 requires even dimensions
    height_ = height & ~1;
    configSink_ = std::move(configSink);
    sampleSink_ = std::move(sampleSink);

    codec_ = AMediaCodec_createEncoderByType(kMimeH264);
    if (codec_ == nullptr) {
        LOGE("createEncoderByType failed");
        return false;
    }
    AMediaFormat *fmt = AMediaFormat_new();
    AMediaFormat_setString(fmt, AMEDIAFORMAT_KEY_MIME, kMimeH264);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_WIDTH, width_);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_HEIGHT, height_);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_BIT_RATE, bitrate);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_FRAME_RATE, fps);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, kIFrameIntervalSec);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_COLOR_FORMAT, kColorFormatSemiPlanar);
    media_status_t s = AMediaCodec_configure(codec_, fmt, nullptr, nullptr,
                                             AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
    AMediaFormat_delete(fmt);
    if (s != AMEDIA_OK) {
        LOGE("configure failed: %d", s);
        closeCodec();
        return false;
    }
    if (AMediaCodec_start(codec_) != AMEDIA_OK) {
        LOGE("start failed");
        closeCodec();
        return false;
    }
    started_ = true;
    return true;
}

bool H264Encoder::encode(const uint8_t *rgba, int width, int height, int64_t ptsUs) {
    if (!started_ || aborted_ || rgba == nullptr) {
        return false;
    }
    // Convert RGBA (bottom-left origin from GL) -> NV12 upright via a negative
    // source stride, matching the camera record path. libswscale handles the
    // flip: point at the last row and use a negative linesize.
    sws_ = sws_getCachedContext(sws_, width_, height_, AV_PIX_FMT_RGBA,
                                width_, height_, AV_PIX_FMT_NV12, SWS_BILINEAR,
                                nullptr, nullptr, nullptr);
    if (sws_ == nullptr) {
        return false;
    }
    const int srcStride = width * 4;
    const uint8_t *srcData[4] = {rgba + static_cast<size_t>(height - 1) * srcStride, nullptr,
                                 nullptr, nullptr};
    const int srcLinesize[4] = {-srcStride, 0, 0, 0};

    const size_t needed = static_cast<size_t>(width_) * height_ * 3 / 2;
    nv12_.resize(needed);
    uint8_t *dstData[4] = {};
    int dstLinesize[4] = {};
    if (av_image_fill_arrays(dstData, dstLinesize, nv12_.data(), AV_PIX_FMT_NV12,
                             width_, height_, 1) < 0) {
        return false;
    }
    if (sws_scale(sws_, srcData, srcLinesize, 0, height_, dstData, dstLinesize) <= 0) {
        return false;
    }

    // Feed the NV12 buffer to the codec, blocking (offline) until accepted.
    while (true) {
        ssize_t inIdx = AMediaCodec_dequeueInputBuffer(codec_, kDequeueTimeoutUs);
        if (inIdx >= 0) {
            size_t capacity = 0;
            uint8_t *buf = AMediaCodec_getInputBuffer(codec_, inIdx, &capacity);
            if (buf == nullptr || capacity < needed) {
                LOGE("input buffer too small: cap=%zu needed=%zu", capacity, needed);
                AMediaCodec_queueInputBuffer(codec_, inIdx, 0, 0, 0, 0);
                return false;
            }
            std::memcpy(buf, nv12_.data(), needed);
            AMediaCodec_queueInputBuffer(codec_, inIdx, 0, needed, ptsUs, 0);
            break;
        }
        // No input buffer yet: drain output to make room, then retry.
        if (!drainOutput(false)) {
            return false;
        }
    }
    return drainOutput(false);
}

bool H264Encoder::finish() {
    if (!started_ || aborted_) {
        return !aborted_;
    }
    // Signal EOS with an empty input buffer, then drain to completion.
    ssize_t inIdx = AMediaCodec_dequeueInputBuffer(codec_, kDequeueTimeoutUs * 10);
    if (inIdx < 0) {
        // Retry a few times; a full input queue clears as output drains.
        for (int i = 0; i < 10 && inIdx < 0; ++i) {
            if (!drainOutput(false)) return false;
            inIdx = AMediaCodec_dequeueInputBuffer(codec_, kDequeueTimeoutUs * 10);
        }
        if (inIdx < 0) {
            LOGE("finish: could not dequeue input buffer for EOS");
            return false;
        }
    }
    AMediaCodec_queueInputBuffer(codec_, inIdx, 0, 0, 0,
                                 AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
    return drainOutput(true);
}

bool H264Encoder::drainOutput(bool endOfStream) {
    if (codec_ == nullptr || aborted_) {
        return !aborted_;
    }
    while (true) {
        AMediaCodecBufferInfo info{};
        ssize_t outIdx = AMediaCodec_dequeueOutputBuffer(
                codec_, &info, endOfStream ? kDequeueTimeoutUs * 10 : 0);
        if (outIdx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
            if (!endOfStream) return true;
            continue;
        }
        if (outIdx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            // Preferred, cross-device source of SPS/PPS: csd-0 (+ csd-1) in the
            // output format. Some encoders never emit a separate CODEC_CONFIG
            // buffer, so extract the codec-config here. csd values are Annex-B.
            if (!configDelivered_) {
                AMediaFormat *outFmt = AMediaCodec_getOutputFormat(codec_);
                std::vector<uint8_t> config;
                uint8_t *csd = nullptr;
                size_t csdSize = 0;
                if (AMediaFormat_getBuffer(outFmt, "csd-0", reinterpret_cast<void **>(&csd),
                                           &csdSize) && csd != nullptr && csdSize > 0) {
                    config.insert(config.end(), csd, csd + csdSize);
                }
                if (AMediaFormat_getBuffer(outFmt, "csd-1", reinterpret_cast<void **>(&csd),
                                           &csdSize) && csd != nullptr && csdSize > 0) {
                    config.insert(config.end(), csd, csd + csdSize);
                }
                AMediaFormat_delete(outFmt);
                if (!config.empty()) {
                    const bool ok = configSink_ && configSink_(config.data(), config.size());
                    configDelivered_ = true;
                    if (!ok) {
                        aborted_ = true;
                        return false;
                    }
                }
            }
            continue;
        }
        if (outIdx == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
            continue;
        }
        if (outIdx < 0) {
            if (endOfStream) continue;
            return true;
        }
        size_t outSize = 0;
        uint8_t *outBuf = AMediaCodec_getOutputBuffer(codec_, outIdx, &outSize);
        const bool isConfig = (info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) != 0;
        bool sinkOk = true;
        if (outBuf != nullptr && info.size > 0) {
            if (isConfig) {
                // Codec-config (SPS/PPS, Annex-B). Deliver once for extradata.
                if (!configDelivered_) {
                    sinkOk = configSink_ && configSink_(outBuf + info.offset,
                                                        static_cast<size_t>(info.size));
                    configDelivered_ = true;
                }
            } else {
                H264Encoder::EncodedSample sample{
                        outBuf + info.offset,
                        static_cast<size_t>(info.size),
                        info.presentationTimeUs,
                        (info.flags & AMEDIACODEC_BUFFER_FLAG_KEY_FRAME) != 0};
                sinkOk = sampleSink_ && sampleSink_(sample);
            }
        }
        AMediaCodec_releaseOutputBuffer(codec_, outIdx, false);
        if (!sinkOk) {
            aborted_ = true;
            return false;
        }
        if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
            return true;
        }
    }
}

void H264Encoder::closeCodec() {
    if (codec_ != nullptr) {
        if (started_) {
            AMediaCodec_stop(codec_);
        }
        AMediaCodec_delete(codec_);
        codec_ = nullptr;
    }
    started_ = false;
}
