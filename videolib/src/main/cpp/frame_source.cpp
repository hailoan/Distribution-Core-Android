#include "frame_source.h"

#include <android/log.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <limits>
#include <vector>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
}

#define LOG_TAG "videolib.export.source"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {
    constexpr AVRational kMicrosecondTimeBase{1, 1000000};
    constexpr int64_t kDefaultFrameDurationUs = 33333;

    bool isReadableLocalFile(const std::string &path) {
        struct stat info{};
        return path.find("://") == std::string::npos &&
               stat(path.c_str(), &info) == 0 &&
               S_ISREG(info.st_mode) &&
               access(path.c_str(), R_OK) == 0;
    }

    int interruptDecode(void *opaque) {
        const auto *cancelled = static_cast<const std::atomic<bool> *>(opaque);
        return cancelled != nullptr && cancelled->load(std::memory_order_acquire) ? 1 : 0;
    }

    int64_t clampMsToUs(int64_t ms) {
        if (ms <= 0) return 0;
        return ms > std::numeric_limits<int64_t>::max() / 1000
               ? std::numeric_limits<int64_t>::max()
               : ms * 1000;
    }
}

FrameSource::FrameSource(const std::atomic<bool> *cancel) : cancel_(cancel) {}

FrameSource::~FrameSource() {
    sws_freeContext(sws_);
    av_frame_free(&frame_);
    av_packet_free(&packet_);
    avcodec_free_context(&codec_);
    if (format_ != nullptr) {
        avformat_close_input(&format_);
    }
}

bool FrameSource::cancelled() const {
    return cancel_ != nullptr && cancel_->load(std::memory_order_acquire);
}

std::optional<ExportErrorCode> FrameSource::open(const std::string &path) {
    if (!isReadableLocalFile(path)) {
        return ExportErrorCode::InputOpen;
    }
    format_ = avformat_alloc_context();
    if (format_ == nullptr) {
        return ExportErrorCode::InputOpen;
    }
    format_->interrupt_callback.callback = interruptDecode;
    format_->interrupt_callback.opaque = const_cast<std::atomic<bool> *>(cancel_);

    if (avformat_open_input(&format_, path.c_str(), nullptr, nullptr) < 0) {
        return ExportErrorCode::InputOpen;
    }
    if (avformat_find_stream_info(format_, nullptr) < 0) {
        return ExportErrorCode::UnsupportedVideo;
    }
    videoStreamIndex_ = av_find_best_stream(format_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStreamIndex_ < 0) {
        return ExportErrorCode::UnsupportedVideo;
    }
    AVStream *stream = format_->streams[videoStreamIndex_];
    const AVCodec *decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if (decoder == nullptr) {
        return ExportErrorCode::UnsupportedVideo;
    }
    codec_ = avcodec_alloc_context3(decoder);
    if (codec_ == nullptr ||
        avcodec_parameters_to_context(codec_, stream->codecpar) < 0 ||
        avcodec_open2(codec_, decoder, nullptr) < 0) {
        return ExportErrorCode::UnsupportedVideo;
    }
    packet_ = av_packet_alloc();
    frame_ = av_frame_alloc();
    if (packet_ == nullptr || frame_ == nullptr) {
        return ExportErrorCode::Decode;
    }

    width_ = codec_->width;
    height_ = codec_->height;
    if (width_ <= 0 || height_ <= 0) {
        return ExportErrorCode::UnsupportedVideo;
    }

    AVRational rate = av_guess_frame_rate(format_, stream, nullptr);
    if (rate.num > 0 && rate.den > 0) {
        frameRate_ = static_cast<double>(rate.num) / static_cast<double>(rate.den);
        frameDurationUs_ = std::max<int64_t>(
                1, av_rescale_q(1, av_inv_q(rate), kMicrosecondTimeBase));
    } else {
        frameRate_ = 30.0;
        frameDurationUs_ = kDefaultFrameDurationUs;
    }
    return std::nullopt;
}

// Convert the current decoded frame_ to RGBA and hand it to the sink at the
// given output PTS. Returns Decode on conversion failure; sets *stopped when the
// sink asks to stop.
std::optional<ExportErrorCode> FrameSource::deliverFrame(
        double /*speed*/, int64_t /*ptsBaseUs*/, int64_t /*segmentStartUs*/,
        const FrameSink &sink, bool *stopped) {
    const int width = frame_->width;
    const int height = frame_->height;
    if (width <= 0 || height <= 0) {
        return ExportErrorCode::Decode;
    }
    sws_ = sws_getCachedContext(sws_, width, height,
                                static_cast<AVPixelFormat>(frame_->format),
                                width, height, AV_PIX_FMT_RGBA, SWS_BILINEAR,
                                nullptr, nullptr, nullptr);
    if (sws_ == nullptr) {
        return ExportErrorCode::Decode;
    }
    const int rgbaSize = av_image_get_buffer_size(AV_PIX_FMT_RGBA, width, height, 1);
    if (rgbaSize <= 0) {
        return ExportErrorCode::Decode;
    }
    rgba_.resize(static_cast<size_t>(rgbaSize));
    uint8_t *dstData[4] = {};
    int dstLinesize[4] = {};
    if (av_image_fill_arrays(dstData, dstLinesize, rgba_.data(), AV_PIX_FMT_RGBA,
                             width, height, 1) < 0 ||
        sws_scale(sws_, frame_->data, frame_->linesize, 0, height, dstData, dstLinesize) <= 0) {
        return ExportErrorCode::Decode;
    }

    Frame out{rgba_.data(), width, height, lastOutputPtsUs_};
    if (!sink(out)) {
        *stopped = true;
    }
    return std::nullopt;
}

std::optional<ExportErrorCode> FrameSource::decode(
        int64_t startMs, int64_t endMs, double speed, int64_t ptsBaseUs,
        const FrameSink &sink) {
    AVStream *stream = format_->streams[videoStreamIndex_];
    const int64_t segmentStartUs = clampMsToUs(startMs);
    const int64_t segmentEndUs = endMs >= 0 ? clampMsToUs(endMs) : -1;
    const double effectiveSpeed = speed > 0.0 ? speed : 1.0;

    // Seed the media clock origin from the stream start (fallback: first PTS).
    hasTimelineStart_ = stream->start_time != AV_NOPTS_VALUE;
    timelineStartUs_ = hasTimelineStart_
                       ? av_rescale_q(stream->start_time, stream->time_base, kMicrosecondTimeBase)
                       : 0;
    lastMediaUs_ = -frameDurationUs_;
    lastOutputPtsUs_ = ptsBaseUs - 1;
    nextPtsUs_ = ptsBaseUs;

    // Seek to the keyframe at or before A so the prefix is not decoded from 0.
    if (segmentStartUs > 0) {
        int64_t target = av_rescale_q(segmentStartUs, kMicrosecondTimeBase, stream->time_base);
        if (stream->start_time != AV_NOPTS_VALUE) {
            target += stream->start_time;
        }
        if (av_seek_frame(format_, videoStreamIndex_, target, AVSEEK_FLAG_BACKWARD) < 0) {
            return ExportErrorCode::Decode;
        }
        avcodec_flush_buffers(codec_);
    }

    bool stopped = false;
    bool deliveredAny = false;

    // Compute the segment-relative, speed-scaled output PTS for the current
    // frame_, enforce the [A, B) window, and deliver. Returns Decode on error,
    // sets *stopped at end-of-segment or on sink stop, advances the media clock.
    auto process = [&]() -> std::optional<ExportErrorCode> {
        int64_t mediaUs;
        const int64_t ts = frame_->best_effort_timestamp;
        if (ts != AV_NOPTS_VALUE) {
            const int64_t sourceUs = av_rescale_q(ts, stream->time_base, kMicrosecondTimeBase);
            if (!hasTimelineStart_) {
                timelineStartUs_ = sourceUs;
                hasTimelineStart_ = true;
            }
            mediaUs = std::max<int64_t>(0, sourceUs - timelineStartUs_);
            mediaUs = std::max(mediaUs, lastMediaUs_);
        } else {
            mediaUs = std::max<int64_t>(segmentStartUs, lastMediaUs_ + frameDurationUs_);
        }

        // Exclusive end B: stop before the first frame at/after B.
        if (segmentEndUs >= 0 && mediaUs >= segmentEndUs) {
            stopped = true;
            return std::nullopt;
        }
        // Inclusive start A: drop frames decoded between the seek keyframe and A.
        if (mediaUs < segmentStartUs) {
            lastMediaUs_ = mediaUs;
            return std::nullopt;
        }
        lastMediaUs_ = mediaUs;

        const int64_t relativeUs = mediaUs - segmentStartUs;
        int64_t outPtsUs = ptsBaseUs + static_cast<int64_t>(
                static_cast<double>(relativeUs) / effectiveSpeed);
        if (outPtsUs <= lastOutputPtsUs_) {
            outPtsUs = lastOutputPtsUs_ + 1;
        }
        lastOutputPtsUs_ = outPtsUs;
        nextPtsUs_ = outPtsUs + std::max<int64_t>(
                1, static_cast<int64_t>(frameDurationUs_ / effectiveSpeed));

        auto err = deliverFrame(effectiveSpeed, ptsBaseUs, segmentStartUs, sink, &stopped);
        if (err.has_value()) {
            return err;
        }
        deliveredAny = true;
        return std::nullopt;
    };

    auto drain = [&]() -> std::optional<ExportErrorCode> {
        while (!cancelled() && !stopped) {
            const int recv = avcodec_receive_frame(codec_, frame_);
            if (recv == AVERROR(EAGAIN) || recv == AVERROR_EOF) {
                return std::nullopt;
            }
            if (recv < 0) {
                return ExportErrorCode::Decode;
            }
            auto err = process();
            av_frame_unref(frame_);
            if (err.has_value()) {
                return err;
            }
        }
        return std::nullopt;
    };

    while (!cancelled() && !stopped) {
        const int readResult = av_read_frame(format_, packet_);
        if (readResult >= 0) {
            if (packet_->stream_index != videoStreamIndex_) {
                av_packet_unref(packet_);
                continue;
            }
            const int sendResult = avcodec_send_packet(codec_, packet_);
            av_packet_unref(packet_);
            if (sendResult < 0) {
                return ExportErrorCode::Decode;
            }
            if (auto err = drain()) {
                return err;
            }
            continue;
        }
        av_packet_unref(packet_);
        if (cancelled()) {
            return ExportErrorCode::Decode;
        }
        if (readResult != AVERROR_EOF) {
            return ExportErrorCode::Decode;
        }
        avcodec_send_packet(codec_, nullptr); // flush
        if (auto err = drain()) {
            return err;
        }
        break;
    }

    if (cancelled()) {
        return ExportErrorCode::Decode;
    }
    return (deliveredAny || stopped) ? std::optional<ExportErrorCode>(std::nullopt)
                                     : std::optional<ExportErrorCode>(ExportErrorCode::Decode);
}
