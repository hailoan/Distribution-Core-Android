#include "video_playback.h"

#include <android/log.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libswscale/swscale.h>
}

#define LOG_TAG "videolib.playback"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

constexpr AVRational kMicrosecondTimeBase{1, 1000000};
constexpr int64_t kDefaultFrameDurationUs = 33333;
constexpr int64_t kMaximumWaitChunkUs = 60000000;

struct AttemptResources {
    AVFormatContext *format = nullptr;
    AVCodecContext *codec = nullptr;
    AVPacket *packet = nullptr;
    AVFrame *frame = nullptr;
    SwsContext *sws = nullptr;
    bool inputOpened = false;

    ~AttemptResources() {
        sws_freeContext(sws);
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codec);
        if (format != nullptr) {
            if (inputOpened) {
                avformat_close_input(&format);
            } else {
                avformat_free_context(format);
                format = nullptr;
            }
        }
    }
};

bool isReadableLocalFile(const std::string &path) {
    struct stat info {};
    return path.find("://") == std::string::npos &&
           stat(path.c_str(), &info) == 0 &&
           S_ISREG(info.st_mode) &&
           access(path.c_str(), R_OK) == 0;
}

int interruptInput(void *opaque) {
    const auto *cancelled = static_cast<const std::atomic<bool> *>(opaque);
    return cancelled != nullptr && cancelled->load(std::memory_order_acquire) ? 1 : 0;
}

} // namespace

VideoPlayback::VideoPlayback(PlaybackTerminalCallback terminalCallback)
        : terminalCallback_(std::move(terminalCallback)) {}

VideoPlayback::~VideoPlayback() {
    release();
}

bool VideoPlayback::surfaceAvailable(ANativeWindow *window) {
    if (window == nullptr) {
        return false;
    }

    bool hadSurface = false;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (state_ == PlaybackState::Released) {
            ANativeWindow_release(window);
            return false;
        }
        hadSurface = surfaceReady_;
    }
    if (hadSurface) {
        releaseSurface();
    }

    bool attached = false;
    {
        std::lock_guard<std::mutex> renderLock(rendererMutex_);
        attached = renderer_.surfaceAvailable(window);
    }
    bool released = false;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        released = state_ == PlaybackState::Released;
        surfaceReady_ = attached && !released;
    }
    if (released && attached) {
        std::lock_guard<std::mutex> renderLock(rendererMutex_);
        renderer_.releaseSurface();
    }
    return attached && !released;
}

bool VideoPlayback::pushFrame(const uint8_t *pixels, int width, int height) {
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (state_ == PlaybackState::Released || !surfaceReady_) {
            return false;
        }
    }
    std::lock_guard<std::mutex> renderLock(rendererMutex_);
    return renderer_.pushFrame(pixels, width, height);
}

void VideoPlayback::requestPattern() {
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (state_ == PlaybackState::Released || !surfaceReady_) {
            return;
        }
    }
    std::lock_guard<std::mutex> renderLock(rendererMutex_);
    renderer_.requestPattern();
}

void VideoPlayback::joinFinishedWorker() {
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

uint64_t VideoPlayback::play(const std::string &path) {
    joinFinishedWorker();

    std::lock_guard<std::mutex> lock(stateMutex_);
    if (state_ == PlaybackState::Released || !surfaceReady_ || path.empty() ||
        currentAttemptId_ != 0 || worker_.joinable()) {
        return 0;
    }

    uint64_t attemptId = nextAttemptId_++;
    if (attemptId == 0) {
        attemptId = nextAttemptId_++;
    }
    currentAttemptId_ = attemptId;
    terminalClaimed_ = false;
    pendingSeek_.reset();
    ++controlVersion_;
    cancelRequested_.store(false, std::memory_order_release);
    state_ = PlaybackState::Starting;

    try {
        worker_ = std::thread(&VideoPlayback::runAttempt, this, attemptId, path);
    } catch (...) {
        currentAttemptId_ = 0;
        terminalClaimed_ = false;
        state_ = PlaybackState::Idle;
        return 0;
    }
    return attemptId;
}

void VideoPlayback::stop() {
    std::thread activeWorker;
    bool cancelledActive = false;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (state_ == PlaybackState::Released) {
            return;
        }
        if (currentAttemptId_ != 0 && !terminalClaimed_) {
            terminalClaimed_ = true;
            currentAttemptId_ = 0;
            state_ = PlaybackState::Stopping;
            cancelRequested_.store(true, std::memory_order_release);
            pendingSeek_.reset();
            ++controlVersion_;
            cancelledActive = true;
        }
        if (worker_.joinable() && worker_.get_id() != std::this_thread::get_id()) {
            activeWorker = std::move(worker_);
        }
    }
    waitCv_.notify_all();
    if (activeWorker.joinable()) {
        activeWorker.join();
    }
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (cancelledActive && state_ != PlaybackState::Released) {
            state_ = PlaybackState::Idle;
        }
        cancelRequested_.store(false, std::memory_order_release);
    }
}

bool VideoPlayback::pause() {
    bool stateChanged = false;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (!surfaceReady_ || currentAttemptId_ == 0 || terminalClaimed_ ||
            (state_ != PlaybackState::Playing && state_ != PlaybackState::Paused)) {
            return false;
        }
        if (state_ == PlaybackState::Playing) {
            state_ = PlaybackState::Paused;
            ++controlVersion_;
            stateChanged = true;
        }
    }
    if (stateChanged) {
        waitCv_.notify_all();
    }

    // Synchronize with any presentation that had already passed its state check.
    std::lock_guard<std::mutex> renderLock(rendererMutex_);
    return true;
}

bool VideoPlayback::resume() {
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (!surfaceReady_ || currentAttemptId_ == 0 || terminalClaimed_ ||
            (state_ != PlaybackState::Paused && state_ != PlaybackState::Playing)) {
            return false;
        }
        if (state_ == PlaybackState::Playing) {
            return true;
        }
        state_ = PlaybackState::Playing;
        ++controlVersion_;
    }
    waitCv_.notify_all();
    return true;
}

bool VideoPlayback::setLooping(bool enabled) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (state_ == PlaybackState::Released) {
        return false;
    }
    looping_ = enabled;
    ++controlVersion_;
    waitCv_.notify_all();
    return true;
}

bool VideoPlayback::setPlaybackSpeed(double speed) {
    if (!std::isfinite(speed) || speed < 0.1) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (state_ == PlaybackState::Released) {
            return false;
        }
        playbackSpeed_ = speed;
        ++controlVersion_;
    }
    waitCv_.notify_all();
    return true;
}

bool VideoPlayback::seekTo(int64_t positionMs) {
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (!surfaceReady_ || currentAttemptId_ == 0 || terminalClaimed_ ||
            (state_ != PlaybackState::Playing && state_ != PlaybackState::Paused &&
             state_ != PlaybackState::Seeking)) {
            return false;
        }
        if (state_ != PlaybackState::Seeking) {
            seekResumeAfter_ = state_ == PlaybackState::Playing;
        }
        uint64_t seekId = nextSeekId_++;
        if (seekId == 0) {
            seekId = nextSeekId_++;
        }
        latestSeekId_ = seekId;
        pendingSeek_ = SeekRequest{positionMs, seekId, seekResumeAfter_};
        state_ = PlaybackState::Seeking;
        ++controlVersion_;
    }
    waitCv_.notify_all();
    return true;
}

void VideoPlayback::releaseSurface() {
    std::thread activeWorker;
    PlaybackTerminalCallback callback;
    uint64_t failedAttemptId = 0;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        surfaceReady_ = false;
        pendingSeek_.reset();
        ++controlVersion_;
        if (state_ != PlaybackState::Released && currentAttemptId_ != 0 && !terminalClaimed_) {
            terminalClaimed_ = true;
            failedAttemptId = currentAttemptId_;
            currentAttemptId_ = 0;
            state_ = PlaybackState::Stopping;
            cancelRequested_.store(true, std::memory_order_release);
            callback = terminalCallback_;
        }
        if (worker_.joinable() && worker_.get_id() != std::this_thread::get_id()) {
            activeWorker = std::move(worker_);
        }
    }
    waitCv_.notify_all();
    if (activeWorker.joinable()) {
        activeWorker.join();
    }
    {
        std::lock_guard<std::mutex> renderLock(rendererMutex_);
        renderer_.releaseSurface();
    }
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (failedAttemptId != 0 && state_ != PlaybackState::Released) {
            state_ = PlaybackState::Failed;
        }
        cancelRequested_.store(false, std::memory_order_release);
    }
    if (failedAttemptId != 0 && callback) {
        callback(failedAttemptId, PlaybackErrorCode::Render);
    }
}

void VideoPlayback::release() {
    std::thread activeWorker;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (state_ == PlaybackState::Released) {
            return;
        }
        terminalClaimed_ = true;
        currentAttemptId_ = 0;
        surfaceReady_ = false;
        pendingSeek_.reset();
        ++controlVersion_;
        state_ = PlaybackState::Released;
        cancelRequested_.store(true, std::memory_order_release);
        if (worker_.joinable() && worker_.get_id() != std::this_thread::get_id()) {
            activeWorker = std::move(worker_);
        }
    }
    waitCv_.notify_all();
    if (activeWorker.joinable()) {
        activeWorker.join();
    }
    std::lock_guard<std::mutex> renderLock(rendererMutex_);
    renderer_.releaseSurface();
}

bool VideoPlayback::markPlaying(uint64_t attemptId) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (cancelRequested_.load(std::memory_order_acquire) || terminalClaimed_ ||
        currentAttemptId_ != attemptId || !surfaceReady_ ||
        state_ == PlaybackState::Released) {
        return false;
    }
    state_ = PlaybackState::Playing;
    return true;
}

bool VideoPlayback::isCancelled(uint64_t attemptId) const {
    if (cancelRequested_.load(std::memory_order_acquire)) {
        return true;
    }
    std::lock_guard<std::mutex> lock(stateMutex_);
    return terminalClaimed_ || currentAttemptId_ != attemptId ||
           state_ == PlaybackState::Released || !surfaceReady_;
}

std::optional<PlaybackErrorCode> VideoPlayback::decodeAttempt(
        uint64_t attemptId,
        const std::string &path) {
    if (!isReadableLocalFile(path)) {
        return PlaybackErrorCode::InputOpen;
    }

    AttemptResources resources;
    resources.format = avformat_alloc_context();
    if (resources.format == nullptr) {
        return PlaybackErrorCode::InputOpen;
    }
    resources.format->interrupt_callback.callback = interruptInput;
    resources.format->interrupt_callback.opaque = &cancelRequested_;

    if (avformat_open_input(&resources.format, path.c_str(), nullptr, nullptr) < 0) {
        return PlaybackErrorCode::InputOpen;
    }
    resources.inputOpened = true;
    if (isCancelled(attemptId)) {
        return PlaybackErrorCode::Decode;
    }
    if (avformat_find_stream_info(resources.format, nullptr) < 0) {
        return PlaybackErrorCode::UnsupportedVideo;
    }

    const int videoStreamIndex = av_find_best_stream(
            resources.format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStreamIndex < 0) {
        return PlaybackErrorCode::UnsupportedVideo;
    }
    AVStream *stream = resources.format->streams[videoStreamIndex];
    const AVCodec *decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if (decoder == nullptr) {
        return PlaybackErrorCode::UnsupportedVideo;
    }
    resources.codec = avcodec_alloc_context3(decoder);
    if (resources.codec == nullptr ||
        avcodec_parameters_to_context(resources.codec, stream->codecpar) < 0 ||
        avcodec_open2(resources.codec, decoder, nullptr) < 0) {
        return PlaybackErrorCode::UnsupportedVideo;
    }
    resources.packet = av_packet_alloc();
    resources.frame = av_frame_alloc();
    if (resources.packet == nullptr || resources.frame == nullptr) {
        return PlaybackErrorCode::Decode;
    }
    if (!markPlaying(attemptId)) {
        return PlaybackErrorCode::Decode;
    }

    AVRational guessedRate = av_guess_frame_rate(resources.format, stream, nullptr);
    int64_t fallbackFrameDurationUs = kDefaultFrameDurationUs;
    if (guessedRate.num > 0 && guessedRate.den > 0) {
        fallbackFrameDurationUs = std::max<int64_t>(
                1, av_rescale_q(1, av_inv_q(guessedRate), kMicrosecondTimeBase));
    }

    int64_t durationUs = AV_NOPTS_VALUE;
    if (resources.format->duration > 0 && resources.format->duration != AV_NOPTS_VALUE) {
        durationUs = av_rescale_q(
                resources.format->duration, AV_TIME_BASE_Q, kMicrosecondTimeBase);
    } else if (stream->duration > 0 && stream->duration != AV_NOPTS_VALUE) {
        durationUs = av_rescale_q(stream->duration, stream->time_base, kMicrosecondTimeBase);
    }

    bool clockStarted = false;
    bool hasTimelineStart = stream->start_time != AV_NOPTS_VALUE;
    bool presentedAnyFrame = false;
    bool presentedThisPass = false;
    bool seeking = false;
    bool seekResumeAfter = true;
    uint64_t activeSeekId = 0;
    uint64_t observedControlVersion = 0;
    int64_t timelineStartUs = hasTimelineStart
                              ? av_rescale_q(stream->start_time, stream->time_base,
                                             kMicrosecondTimeBase)
                              : 0;
    int64_t seekTargetUs = 0;
    int64_t fallbackBaseUs = 0;
    int64_t lastMediaUs = -fallbackFrameDurationUs;
    int64_t lastPresentedUs = 0;
    int64_t clockAnchorMediaUs = 0;
    double appliedSpeed = 1.0;
    std::chrono::steady_clock::time_point clockAnchorWall;
    std::vector<uint8_t> rgba;

    enum class ScheduleResult { Present, Drop, Seek, Cancelled };
    enum class DecodeFlow { NeedInput, Seek, Cancelled, DecodeError, RenderError };

    auto isActiveLocked = [&]() {
        return !cancelRequested_.load(std::memory_order_acquire) &&
               !terminalClaimed_ && currentAttemptId_ == attemptId &&
               surfaceReady_ && state_ != PlaybackState::Released;
    };

    auto takePendingSeek = [&]() -> std::optional<SeekRequest> {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (!isActiveLocked() || !pendingSeek_.has_value()) {
            return std::nullopt;
        }
        auto request = pendingSeek_;
        pendingSeek_.reset();
        return request;
    };

    auto resetDecoder = [&](int64_t requestedPositionMs, bool publicSeek,
                            uint64_t seekId, bool resumeAfter) -> bool {
        int64_t requestedUs = 0;
        if (requestedPositionMs > 0) {
            requestedUs = requestedPositionMs > std::numeric_limits<int64_t>::max() / 1000
                          ? std::numeric_limits<int64_t>::max()
                          : requestedPositionMs * 1000;
        }
        if (durationUs != AV_NOPTS_VALUE) {
            const int64_t lastPlayableUs = durationUs > fallbackFrameDurationUs
                                           ? durationUs - fallbackFrameDurationUs
                                           : std::max<int64_t>(0, durationUs);
            requestedUs = std::min(requestedUs, lastPlayableUs);
        }

        const int64_t relativeTimestamp = av_rescale_q(
                requestedUs, kMicrosecondTimeBase, stream->time_base);
        int64_t targetTimestamp = relativeTimestamp;
        if (stream->start_time != AV_NOPTS_VALUE) {
            if (relativeTimestamp > 0 &&
                stream->start_time > std::numeric_limits<int64_t>::max() - relativeTimestamp) {
                targetTimestamp = std::numeric_limits<int64_t>::max();
            } else {
                targetTimestamp = stream->start_time + relativeTimestamp;
            }
        }
        if (av_seek_frame(resources.format, videoStreamIndex, targetTimestamp,
                          AVSEEK_FLAG_BACKWARD) < 0) {
            return false;
        }
        avcodec_flush_buffers(resources.codec);
        av_packet_unref(resources.packet);
        av_frame_unref(resources.frame);

        seeking = publicSeek;
        activeSeekId = seekId;
        seekResumeAfter = resumeAfter;
        seekTargetUs = requestedUs;
        fallbackBaseUs = requestedUs;
        lastMediaUs = requestedUs - fallbackFrameDurationUs;
        clockStarted = false;
        return true;
    };

    auto scheduleFrame = [&](int64_t mediaUs) -> ScheduleResult {
        if (seeking) {
            std::lock_guard<std::mutex> lock(stateMutex_);
            if (!isActiveLocked()) {
                return ScheduleResult::Cancelled;
            }
            if (pendingSeek_.has_value() || latestSeekId_ != activeSeekId) {
                return ScheduleResult::Seek;
            }
            return state_ == PlaybackState::Seeking
                   ? ScheduleResult::Present
                   : ScheduleResult::Cancelled;
        }

        std::unique_lock<std::mutex> lock(stateMutex_);
        while (true) {
            if (!isActiveLocked()) {
                return ScheduleResult::Cancelled;
            }
            if (pendingSeek_.has_value() || state_ == PlaybackState::Seeking) {
                return ScheduleResult::Seek;
            }
            if (state_ == PlaybackState::Paused) {
                const uint64_t version = controlVersion_;
                waitCv_.wait(lock, [&] {
                    return !isActiveLocked() || controlVersion_ != version ||
                           pendingSeek_.has_value() || state_ != PlaybackState::Paused;
                });
                continue;
            }
            if (state_ != PlaybackState::Playing) {
                return ScheduleResult::Cancelled;
            }

            const auto now = std::chrono::steady_clock::now();
            if (!clockStarted || observedControlVersion != controlVersion_) {
                clockAnchorMediaUs = presentedAnyFrame ? lastPresentedUs : mediaUs;
                clockAnchorWall = now;
                appliedSpeed = playbackSpeed_;
                observedControlVersion = controlVersion_;
                clockStarted = true;
            }
            const int64_t mediaDeltaUs = std::max<int64_t>(0, mediaUs - clockAnchorMediaUs);
            const long double delayUs = static_cast<long double>(mediaDeltaUs) / appliedSpeed;
            const long double elapsedUs = std::chrono::duration<
                    long double, std::micro>(now - clockAnchorWall).count();
            const long double remainingUs = delayUs - elapsedUs;
            if (remainingUs > 0) {
                const int64_t waitChunkUs = static_cast<int64_t>(std::min<long double>(
                        remainingUs, kMaximumWaitChunkUs));
                const auto deadline = now + std::chrono::microseconds(
                        std::max<int64_t>(1, waitChunkUs));
                const uint64_t version = controlVersion_;
                waitCv_.wait_until(lock, deadline, [&] {
                    return !isActiveLocked() || controlVersion_ != version ||
                           pendingSeek_.has_value() || state_ != PlaybackState::Playing;
                });
                if (controlVersion_ != version || pendingSeek_.has_value() ||
                    state_ != PlaybackState::Playing || !isActiveLocked()) {
                    continue;
                }
                continue;
            }

            const long double latenessUs = std::max<long double>(0, elapsedUs - delayUs);
            const int64_t scaledFrameUs = std::max<int64_t>(
                    1, static_cast<int64_t>(fallbackFrameDurationUs / appliedSpeed));
            if (presentedThisPass && appliedSpeed > 1.0 && latenessUs > scaledFrameUs) {
                return ScheduleResult::Drop;
            }
            return ScheduleResult::Present;
        }
    };

    auto presentFrame = [&](int64_t mediaUs) -> DecodeFlow {
        while (true) {
            const ScheduleResult schedule = scheduleFrame(mediaUs);
            if (schedule == ScheduleResult::Seek) {
                return DecodeFlow::Seek;
            }
            if (schedule == ScheduleResult::Cancelled) {
                return DecodeFlow::Cancelled;
            }
            if (schedule == ScheduleResult::Drop) {
                return DecodeFlow::NeedInput;
            }

            const int width = resources.frame->width;
            const int height = resources.frame->height;
            if (width <= 0 || height <= 0) {
                return DecodeFlow::DecodeError;
            }
            resources.sws = sws_getCachedContext(
                    resources.sws, width, height,
                    static_cast<AVPixelFormat>(resources.frame->format),
                    width, height, AV_PIX_FMT_RGBA, SWS_BILINEAR,
                    nullptr, nullptr, nullptr);
            if (resources.sws == nullptr) {
                return DecodeFlow::DecodeError;
            }
            const int rgbaSize = av_image_get_buffer_size(AV_PIX_FMT_RGBA, width, height, 1);
            if (rgbaSize <= 0) {
                return DecodeFlow::DecodeError;
            }
            rgba.resize(static_cast<size_t>(rgbaSize));
            uint8_t *destinationData[4] = {};
            int destinationLinesize[4] = {};
            if (av_image_fill_arrays(destinationData, destinationLinesize, rgba.data(),
                                     AV_PIX_FMT_RGBA, width, height, 1) < 0 ||
                sws_scale(resources.sws, resources.frame->data, resources.frame->linesize,
                          0, height, destinationData, destinationLinesize) <= 0) {
                return DecodeFlow::DecodeError;
            }

            bool mayPresent = false;
            bool seekSuperseded = false;
            bool retryAfterPause = false;
            bool presented = false;
            {
                std::lock_guard<std::mutex> renderLock(rendererMutex_);
                {
                    std::lock_guard<std::mutex> lock(stateMutex_);
                    if (isActiveLocked()) {
                        seekSuperseded = pendingSeek_.has_value() ||
                                           (seeking && latestSeekId_ != activeSeekId);
                        retryAfterPause = !seeking && state_ == PlaybackState::Paused;
                        mayPresent = !seekSuperseded &&
                                     ((seeking && state_ == PlaybackState::Seeking) ||
                                      (!seeking && state_ == PlaybackState::Playing));
                    }
                }
                if (mayPresent) {
                    presented = renderer_.pushFrame(rgba.data(), width, height);
                }
            }
            if (seekSuperseded) {
                return DecodeFlow::Seek;
            }
            if (retryAfterPause) {
                continue;
            }
            if (!mayPresent) {
                return DecodeFlow::Cancelled;
            }
            if (!presented) {
                return DecodeFlow::RenderError;
            }

            presentedAnyFrame = true;
            presentedThisPass = true;
            lastPresentedUs = mediaUs;
            if (seeking) {
                {
                    std::lock_guard<std::mutex> lock(stateMutex_);
                    if (!isActiveLocked()) {
                        return DecodeFlow::Cancelled;
                    }
                    if (pendingSeek_.has_value() || latestSeekId_ != activeSeekId) {
                        return DecodeFlow::Seek;
                    }
                    state_ = seekResumeAfter ? PlaybackState::Playing : PlaybackState::Paused;
                    ++controlVersion_;
                    observedControlVersion = controlVersion_;
                    appliedSpeed = playbackSpeed_;
                }
                seeking = false;
                clockAnchorMediaUs = mediaUs;
                clockAnchorWall = std::chrono::steady_clock::now();
                clockStarted = true;
                waitCv_.notify_all();
            }
            return DecodeFlow::NeedInput;
        }
    };

    auto receiveFrames = [&]() -> DecodeFlow {
        while (!isCancelled(attemptId)) {
            const int receiveResult = avcodec_receive_frame(resources.codec, resources.frame);
            if (receiveResult == AVERROR(EAGAIN) || receiveResult == AVERROR_EOF) {
                return DecodeFlow::NeedInput;
            }
            if (receiveResult < 0) {
                return DecodeFlow::DecodeError;
            }

            int64_t mediaUs;
            const int64_t timestamp = resources.frame->best_effort_timestamp;
            if (timestamp != AV_NOPTS_VALUE) {
                const int64_t sourceUs = av_rescale_q(
                        timestamp, stream->time_base, kMicrosecondTimeBase);
                if (!hasTimelineStart) {
                    timelineStartUs = sourceUs;
                    hasTimelineStart = true;
                }
                mediaUs = std::max<int64_t>(0, sourceUs - timelineStartUs);
                mediaUs = std::max(mediaUs, lastMediaUs);
            } else {
                mediaUs = std::max(fallbackBaseUs, lastMediaUs + fallbackFrameDurationUs);
            }
            lastMediaUs = mediaUs;

            if (seeking && mediaUs < seekTargetUs) {
                bool superseded = false;
                {
                    std::lock_guard<std::mutex> lock(stateMutex_);
                    superseded = pendingSeek_.has_value() || latestSeekId_ != activeSeekId;
                }
                av_frame_unref(resources.frame);
                if (superseded) {
                    return DecodeFlow::Seek;
                }
                continue;
            }
            const DecodeFlow flow = presentFrame(mediaUs);
            av_frame_unref(resources.frame);
            if (flow != DecodeFlow::NeedInput) {
                return flow;
            }
        }
        return DecodeFlow::Cancelled;
    };

    while (!isCancelled(attemptId)) {
        if (const auto request = takePendingSeek()) {
            if (!resetDecoder(request->positionMs, true, request->id, request->resumeAfter)) {
                return PlaybackErrorCode::Decode;
            }
        }

        int readResult = av_read_frame(resources.format, resources.packet);
        if (readResult >= 0) {
            if (resources.packet->stream_index != videoStreamIndex) {
                av_packet_unref(resources.packet);
                continue;
            }
            const int sendResult = avcodec_send_packet(resources.codec, resources.packet);
            av_packet_unref(resources.packet);
            if (sendResult < 0) {
                return PlaybackErrorCode::Decode;
            }
            const DecodeFlow flow = receiveFrames();
            if (flow == DecodeFlow::Seek) {
                continue;
            }
            if (flow == DecodeFlow::Cancelled) {
                return PlaybackErrorCode::Decode;
            }
            if (flow == DecodeFlow::DecodeError) {
                return PlaybackErrorCode::Decode;
            }
            if (flow == DecodeFlow::RenderError) {
                return PlaybackErrorCode::Render;
            }
            continue;
        }
        av_packet_unref(resources.packet);
        if (isCancelled(attemptId)) {
            return PlaybackErrorCode::Decode;
        }
        if (readResult != AVERROR_EOF) {
            return PlaybackErrorCode::Decode;
        }

        const int drainResult = avcodec_send_packet(resources.codec, nullptr);
        if (drainResult < 0 && drainResult != AVERROR_EOF) {
            return PlaybackErrorCode::Decode;
        }
        const DecodeFlow drainFlow = receiveFrames();
        if (drainFlow == DecodeFlow::Seek) {
            continue;
        }
        if (drainFlow == DecodeFlow::Cancelled) {
            return PlaybackErrorCode::Decode;
        }
        if (drainFlow == DecodeFlow::DecodeError) {
            return PlaybackErrorCode::Decode;
        }
        if (drainFlow == DecodeFlow::RenderError) {
            return PlaybackErrorCode::Render;
        }
        if (const auto request = takePendingSeek()) {
            if (!resetDecoder(request->positionMs, true, request->id, request->resumeAfter)) {
                return PlaybackErrorCode::Decode;
            }
            continue;
        }
        if (seeking) {
            return PlaybackErrorCode::Decode;
        }
        if (!presentedThisPass) {
            return PlaybackErrorCode::Decode;
        }

        bool shouldLoop = false;
        bool seekAtEof = false;
        {
            std::unique_lock<std::mutex> lock(stateMutex_);
            while (isActiveLocked() && state_ == PlaybackState::Paused &&
                   !pendingSeek_.has_value()) {
                const uint64_t version = controlVersion_;
                waitCv_.wait(lock, [&] {
                    return !isActiveLocked() || controlVersion_ != version ||
                           pendingSeek_.has_value() || state_ != PlaybackState::Paused;
                });
            }
            if (!isActiveLocked()) {
                return PlaybackErrorCode::Decode;
            }
            seekAtEof = pendingSeek_.has_value() || state_ == PlaybackState::Seeking;
            shouldLoop = !seekAtEof && looping_;
            if (!seekAtEof && !shouldLoop) {
                // Close the acceptance window before finishAttempt emits completion.
                state_ = PlaybackState::Completed;
            }
        }
        if (seekAtEof) {
            continue;
        }
        if (!shouldLoop) {
            break;
        }
        if (!resetDecoder(0, false, 0, true)) {
            return PlaybackErrorCode::Decode;
        }
        fallbackBaseUs = 0;
        lastMediaUs = -fallbackFrameDurationUs;
        lastPresentedUs = 0;
        presentedThisPass = false;
    }
    return presentedAnyFrame
           ? std::nullopt
           : std::optional<PlaybackErrorCode>(PlaybackErrorCode::Decode);
}

void VideoPlayback::runAttempt(uint64_t attemptId, std::string path) {
    std::optional<PlaybackErrorCode> error;
    try {
        error = decodeAttempt(attemptId, path);
    } catch (...) {
        LOGE("Unhandled native failure while decoding playback attempt");
        error = PlaybackErrorCode::Decode;
    }
    if (!isCancelled(attemptId)) {
        finishAttempt(attemptId, error);
    }
}

void VideoPlayback::finishAttempt(
        uint64_t attemptId,
        std::optional<PlaybackErrorCode> error) {
    PlaybackTerminalCallback callback;
    const bool releaseFailedSurface = error == PlaybackErrorCode::Render;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (terminalClaimed_ || currentAttemptId_ != attemptId ||
            state_ == PlaybackState::Released) {
            return;
        }
        terminalClaimed_ = true;
        currentAttemptId_ = 0;
        pendingSeek_.reset();
        ++controlVersion_;
        state_ = error.has_value() ? PlaybackState::Failed : PlaybackState::Completed;
        if (releaseFailedSurface) {
            surfaceReady_ = false;
        }
        callback = terminalCallback_;
    }
    if (releaseFailedSurface) {
        std::lock_guard<std::mutex> renderLock(rendererMutex_);
        renderer_.releaseSurface();
    }
    if (callback) {
        callback(attemptId, error);
    }
}
