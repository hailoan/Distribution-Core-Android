#include "video_export.h"

#include <android/log.h>
#include <unistd.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "frame_source.h"
#include "offscreen_renderer.h"
#include "h264_encoder.h"
#include "mp4_muxer.h"
#include "audio_transcoder.h"

#define LOG_TAG "videolib.export"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {
    // Resolution-derived H.264 bitrate heuristic, clamped. Implementation-local
    // per SOLUTION-DESIGN (D-7); not a normative contract.
    int estimateBitrate(int width, int height) {
        int64_t bitrate = static_cast<int64_t>(width) * height * 3;
        bitrate = std::max<int64_t>(bitrate, 1000000);   // >= 1 Mbps
        bitrate = std::min<int64_t>(bitrate, 20000000);  // <= 20 Mbps
        return static_cast<int>(bitrate);
    }

    int roundFps(double fps) {
        if (fps <= 0.0) return 30;
        return std::max(1, std::min(120, static_cast<int>(fps + 0.5)));
    }
}

VideoExport::VideoExport(ExportTerminalCallback terminalCallback)
        : terminalCallback_(std::move(terminalCallback)) {}

VideoExport::~VideoExport() {
    release();
}

bool VideoExport::isCancelled(uint64_t attemptId) const {
    if (cancelRequested_.load(std::memory_order_acquire)) {
        return true;
    }
    std::lock_guard<std::mutex> lock(stateMutex_);
    return terminalClaimed_ || currentAttemptId_ != attemptId ||
           state_ == ExportState::Released;
}

void VideoExport::joinFinishedWorker() {
    std::thread finished;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (currentAttemptId_ == 0 && worker_.joinable() &&
            worker_.get_id() != std::this_thread::get_id()) {
            finished = std::move(worker_);
        }
    }
    if (finished.joinable()) {
        finished.join();
    }
}

uint64_t VideoExport::start(ExportRequest request) {
    joinFinishedWorker();

    std::lock_guard<std::mutex> lock(stateMutex_);
    if (state_ == ExportState::Released || currentAttemptId_ != 0 || worker_.joinable() ||
        request.segments.empty() || request.outputPath.empty()) {
        return 0;
    }
    uint64_t attemptId = nextAttemptId_++;
    if (attemptId == 0) {
        attemptId = nextAttemptId_++;
    }
    currentAttemptId_ = attemptId;
    terminalClaimed_ = false;
    cancelRequested_.store(false, std::memory_order_release);
    state_ = ExportState::Preparing;
    try {
        worker_ = std::thread(&VideoExport::runExport, this, attemptId, std::move(request));
    } catch (...) {
        currentAttemptId_ = 0;
        state_ = ExportState::Idle;
        return 0;
    }
    return attemptId;
}

void VideoExport::cancel() {
    std::thread activeWorker;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (state_ == ExportState::Released) {
            return;
        }
        if (currentAttemptId_ != 0 && !terminalClaimed_) {
            terminalClaimed_ = true;
            currentAttemptId_ = 0;
            state_ = ExportState::Cancelled;
            cancelRequested_.store(true, std::memory_order_release);
        }
        if (worker_.joinable() && worker_.get_id() != std::this_thread::get_id()) {
            activeWorker = std::move(worker_);
        }
    }
    if (activeWorker.joinable()) {
        activeWorker.join();
    }
    std::lock_guard<std::mutex> lock(stateMutex_);
    cancelRequested_.store(false, std::memory_order_release);
    if (state_ != ExportState::Released) {
        state_ = ExportState::Idle;
    }
}

void VideoExport::release() {
    std::thread activeWorker;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (state_ == ExportState::Released) {
            return;
        }
        terminalClaimed_ = true;
        currentAttemptId_ = 0;
        cancelRequested_.store(true, std::memory_order_release);
        state_ = ExportState::Released;
        if (worker_.joinable() && worker_.get_id() != std::this_thread::get_id()) {
            activeWorker = std::move(worker_);
        }
    }
    if (activeWorker.joinable()) {
        activeWorker.join();
    }
}

// Render + encode the segment's video, then transcode + mux its audio. Video is
// processed first so the encoder surfaces its SPS/PPS (needed to write the MP4
// header) before any sample is muxed; ensureHeader() is invoked from the video
// sample sink. av_interleaved_write_frame reorders the two streams by DTS.
std::optional<ExportErrorCode> VideoExport::exportSegment(
        uint64_t attemptId, const ExportSegment &segment, OffscreenRenderer &renderer,
        H264Encoder &encoder, Mp4Muxer &muxer, AudioTranscoder *audio, int64_t *basePtsUs) {
    if (!renderer.applyAppearance(segment.appearance)) {
        return ExportErrorCode::Render;
    }

    FrameSource source(&cancelRequested_);
    if (auto err = source.open(segment.path)) {
        return err;
    }

    bool sinkError = false;
    ExportErrorCode failCode = ExportErrorCode::Decode;
    std::vector<uint8_t> filtered;

    auto err = source.decode(
            segment.startMs, segment.endMs, segment.speed, *basePtsUs,
            [&](const FrameSource::Frame &frame) -> bool {
                if (isCancelled(attemptId)) return false;
                if (!renderer.renderToRgba(frame.rgba, frame.width, frame.height, &filtered)) {
                    sinkError = true;
                    failCode = ExportErrorCode::Render;
                    return false;
                }
                if (!encoder.encode(filtered.data(), renderer.width(), renderer.height(),
                                    frame.ptsUs)) {
                    sinkError = true;
                    failCode = ExportErrorCode::Encode;
                    return false;
                }
                return true;
            });
    if (sinkError) return failCode;
    if (err.has_value()) return err;

    // Audio for this segment, offset to the same PTS base as the video.
    if (audio != nullptr && audio->hasAudio()) {
        auto audioErr = audio->transcode(
                segment.startMs, segment.endMs, *basePtsUs,
                [&](AVPacket *pkt, AVRational tb) -> bool {
                    if (isCancelled(attemptId)) return false;
                    return muxer.writeAudioPacket(pkt, tb);
                });
        if (audioErr.has_value()) {
            return audioErr;
        }
    }

    *basePtsUs = source.nextPtsUs();
    return std::nullopt;
}

void VideoExport::runExport(uint64_t attemptId, ExportRequest request) {
    std::optional<ExportErrorCode> error;
    int failedSegment = -1;
    bool outputOpened = false;

    // Probe segment 0 to size the shared encoder/muxer (dimensions, fps).
    FrameSource probe(&cancelRequested_);
    if (auto probeErr = probe.open(request.segments.front().path)) {
        finish(attemptId, probeErr, 0);
        return;
    }
    const int width = probe.width() & ~1;
    const int height = probe.height() & ~1;
    const int fps = roundFps(probe.frameRate());
    if (width <= 0 || height <= 0) {
        finish(attemptId, ExportErrorCode::UnsupportedVideo, 0);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (terminalClaimed_ || currentAttemptId_ != attemptId) {
            return; // cancelled/released during preparation
        }
        state_ = ExportState::Exporting;
    }

    OffscreenRenderer renderer;
    if (!renderer.init(width, height, request.segments.front().appearance)) {
        finish(attemptId, ExportErrorCode::Render, 0);
        return;
    }

    Mp4Muxer muxer;
    if (!muxer.open(request.outputPath)) {
        finish(attemptId, ExportErrorCode::Output, 0);
        return;
    }
    outputOpened = true;

    // Prepare segment 0's audio transcoder up front: its AAC encoder context is
    // needed to declare the muxer audio stream before the header is written.
    std::unique_ptr<AudioTranscoder> firstAudio;
    bool wantAudio = request.includeAudio;
    if (wantAudio) {
        firstAudio = std::make_unique<AudioTranscoder>(&cancelRequested_);
        if (auto aErr = firstAudio->open(request.segments.front().path,
                                         request.segments.front().speed)) {
            muxer.finish();
            ::unlink(request.outputPath.c_str());
            finish(attemptId, aErr, 0);
            return;
        }
        if (!firstAudio->hasAudio()) {
            wantAudio = false; // source has no audio (A-2): video-only
        }
    }

    // Header is written lazily on the first video sample: the encoder's config
    // sink adds the video stream (from SPS/PPS), then ensureHeader() adds the
    // audio stream (if any) and writes the container header exactly once.
    bool videoStreamReady = false;
    bool headerWritten = false;
    bool headerFailed = false;
    auto ensureHeader = [&]() -> bool {
        if (headerWritten) return true;
        if (wantAudio && firstAudio && firstAudio->hasAudio()) {
            if (!muxer.addAudioStream(firstAudio->encoderContext())) {
                headerFailed = true;
                return false;
            }
        }
        if (!muxer.writeHeader()) {
            headerFailed = true;
            return false;
        }
        headerWritten = true;
        return true;
    };

    H264Encoder encoder;
    const int bitrate = estimateBitrate(width, height);
    const bool encStarted = encoder.start(
            width, height, fps, bitrate,
            [&](const uint8_t *cfg, size_t size) -> bool {
                if (!muxer.addVideoStream(cfg, size, width, height, fps)) return false;
                videoStreamReady = true;
                return true;
            },
            [&](const H264Encoder::EncodedSample &sample) -> bool {
                if (!videoStreamReady) return false;
                if (!ensureHeader()) return false;
                return muxer.writeVideoSample(sample);
            });
    if (!encStarted) {
        muxer.finish();
        ::unlink(request.outputPath.c_str());
        finish(attemptId, ExportErrorCode::Encode, 0);
        return;
    }

    // Export each segment. basePtsUs accumulates for continuous timeline PTS.
    int64_t basePtsUs = 0;
    for (size_t i = 0; i < request.segments.size(); ++i) {
        if (isCancelled(attemptId)) {
            error = ExportErrorCode::Decode;
            failedSegment = static_cast<int>(i);
            break;
        }
        const ExportSegment &segment = request.segments[i];

        std::unique_ptr<AudioTranscoder> segAudio;
        AudioTranscoder *audio = nullptr;
        if (wantAudio) {
            if (i == 0) {
                audio = firstAudio.get();
            } else {
                segAudio = std::make_unique<AudioTranscoder>(&cancelRequested_);
                if (!segAudio->open(segment.path, segment.speed).has_value() &&
                    segAudio->hasAudio()) {
                    audio = segAudio.get();
                }
                // A later segment without usable audio contributes silence-free
                // video only; the container audio stream still exists (A-2).
            }
        }

        auto segErr = exportSegment(attemptId, segment, renderer, encoder, muxer,
                                    audio, &basePtsUs);
        if (segErr.has_value()) {
            error = segErr;
            failedSegment = static_cast<int>(i);
            break;
        }
        if (headerFailed) {
            error = ExportErrorCode::Mux;
            failedSegment = static_cast<int>(i);
            break;
        }
    }

    // Flush + finalize when the run succeeded and was not cancelled.
    if (!error.has_value() && !isCancelled(attemptId)) {
        if (!encoder.finish() || headerFailed) {
            error = headerFailed ? ExportErrorCode::Mux : ExportErrorCode::Encode;
        } else if (!headerWritten) {
            // No frame ever produced output (e.g. empty window): nothing to write.
            error = ExportErrorCode::Decode;
        } else if (!muxer.finish()) {
            error = ExportErrorCode::Mux;
        }
    } else {
        // Failed or cancelled: still close the muxer cleanly if a header exists.
        if (headerWritten) {
            muxer.finish();
        }
    }

    if (isCancelled(attemptId)) {
        if (outputOpened) ::unlink(request.outputPath.c_str());
        return; // terminal already claimed by cancel()/release()
    }
    if (error.has_value() && outputOpened) {
        ::unlink(request.outputPath.c_str());
    }
    finish(attemptId, error, error.has_value() ? failedSegment : -1);
}

void VideoExport::finish(uint64_t attemptId, std::optional<ExportErrorCode> error,
                         int segmentIndex) {
    ExportTerminalCallback callback;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (terminalClaimed_ || currentAttemptId_ != attemptId ||
            state_ == ExportState::Released) {
            return;
        }
        terminalClaimed_ = true;
        currentAttemptId_ = 0;
        state_ = error.has_value() ? ExportState::Failed : ExportState::Completed;
        callback = terminalCallback_;
    }
    if (callback) {
        callback(attemptId, error, segmentIndex);
    }
}
