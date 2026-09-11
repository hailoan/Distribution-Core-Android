#ifndef VIDEOLIB_FRAME_SOURCE_H
#define VIDEOLIB_FRAME_SOURCE_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "video_export.h" // ExportErrorCode

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

// Decode-only video source for export. Demuxes a local file, decodes the video
// stream, converts each frame to RGBA (libswscale), and delivers frames to a
// sink with a segment-relative, speed-scaled OUTPUT presentation timestamp.
//
// Unlike the preview path (video_playback.cpp) this does NOT wall-clock pace or
// drop frames: export writes every decoded frame in [startMs, endMs). Not
// thread-safe: one FrameSource is used by one export worker thread.
class FrameSource {
public:
    struct Frame {
        const uint8_t *rgba; // tightly packed RGBA8888, width*height*4 bytes
        int width;
        int height;
        int64_t ptsUs; // output PTS (microseconds), already speed-scaled + based
    };

    // Return false to stop decoding early (e.g. cancellation). A sink that
    // returns false makes decode() return ExportErrorCode::Decode-free success
    // only when the caller-owned cancel flag explains the stop; callers should
    // check cancellation themselves.
    using FrameSink = std::function<bool(const Frame &)>;

    explicit FrameSource(const std::atomic<bool> *cancel);

    ~FrameSource();

    FrameSource(const FrameSource &) = delete;

    FrameSource &operator=(const FrameSource &) = delete;

    // Open the file and initialize the video decoder. Returns nullopt on
    // success, or the appropriate error otherwise.
    std::optional<ExportErrorCode> open(const std::string &path);

    int width() const { return width_; }

    int height() const { return height_; }

    // Guessed frame rate (frames/second), always > 0.
    double frameRate() const { return frameRate_; }

    // Decode the half-open window [startMs, endMs) at `speed`, delivering RGBA
    // frames to `sink`. Output PTS = ptsBaseUs + (segmentRelativeMediaUs /
    // speed), so a timeline stays continuous across segments. endMs < 0 means
    // "to the natural end". Returns nullopt when every frame in the window was
    // delivered (or the sink stopped), otherwise an error.
    std::optional<ExportErrorCode> decode(int64_t startMs, int64_t endMs,
                                          double speed, int64_t ptsBaseUs,
                                          const FrameSink &sink);

    // The next free output PTS after the last delivered frame; used as the
    // ptsBaseUs of the following timeline segment for continuous PTS.
    int64_t nextPtsUs() const { return nextPtsUs_; }

private:
    std::optional<ExportErrorCode> deliverFrame(double speed, int64_t ptsBaseUs,
                                                int64_t segmentStartUs,
                                                const FrameSink &sink,
                                                bool *stopped);

    bool cancelled() const;

    const std::atomic<bool> *cancel_;

    AVFormatContext *format_ = nullptr;
    AVCodecContext *codec_ = nullptr;
    AVPacket *packet_ = nullptr;
    AVFrame *frame_ = nullptr;
    SwsContext *sws_ = nullptr;
    std::vector<uint8_t> rgba_;

    int videoStreamIndex_ = -1;
    int width_ = 0;
    int height_ = 0;
    double frameRate_ = 30.0;
    int64_t frameDurationUs_ = 33333;

    // Timeline bookkeeping across decode() (updated as frames are delivered).
    int64_t lastMediaUs_ = 0;
    bool hasTimelineStart_ = false;
    int64_t timelineStartUs_ = 0;
    int64_t lastOutputPtsUs_ = 0;
    int64_t nextPtsUs_ = 0;
};

#endif // VIDEOLIB_FRAME_SOURCE_H
