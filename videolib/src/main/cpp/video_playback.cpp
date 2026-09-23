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
        struct stat info{};
        return path.find("://") == std::string::npos &&
               stat(path.c_str(), &info) == 0 &&
               S_ISREG(info.st_mode) &&
               access(path.c_str(), R_OK) == 0;
    }

    int interruptInput(void *opaque) {
        const auto *cancelled = static_cast<const std::atomic<bool> *>(opaque);
        return cancelled != nullptr && cancelled->load(std::memory_order_acquire) ? 1 : 0;
    }

    bool inRange(float value, float minimum, float maximum) {
        return std::isfinite(value) && value >= minimum && value <= maximum;
    }

    AppearanceApplyResult validateAppearance(const AppearanceSnapshot &appearance) {
        const auto &a = appearance.adjustments;
        if (!inRange(a.brightness, -0.5f, 0.5f) ||
            !inRange(a.contrast, 0.0f, 2.0f) ||
            !inRange(a.saturation, 0.0f, 2.0f) ||
            !inRange(a.exposure, -1.0f, 1.0f) ||
            !inRange(a.darks, 0.5f, 1.5f) ||
            !inRange(a.levelMinimum, -1.0f, 1.0f) ||
            !inRange(a.levelGamma, 0.5f, 1.5f) ||
            !inRange(a.levelMaximum, 0.5f, 1.5f) ||
            !inRange(a.vignette, 0.0f, 1.0f) ||
            !inRange(a.vibrance, -1.0f, 1.0f) ||
            !inRange(a.temperature, -0.5f, 0.5f) ||
            !inRange(a.hue, -1.0f, 1.0f) ||
            !inRange(a.highlights, -2.0f, 2.0f) ||
            !inRange(a.shadows, -1.0f, 1.0f) ||
            !inRange(a.lights, 0.0f, 2.0f) ||
            !inRange(a.clarity, -1.0f, 1.0f)) {
            return AppearanceApplyResult::failure(AppearanceError::InvalidValue);
        }
        if (a.levelMinimum >= a.levelMaximum) {
            return AppearanceApplyResult::failure(AppearanceError::InvalidLevels);
        }
        if (!appearance.filter) return AppearanceApplyResult::success();
        const auto &filter = *appearance.filter;
        if (filter.version != 1) {
            return AppearanceApplyResult::failure(AppearanceError::UnsupportedFilterVersion);
        }
        if (!std::isfinite(filter.opacity) || filter.opacity < 0.0f || filter.opacity > 1.0f) {
            return AppearanceApplyResult::failure(AppearanceError::InvalidFilterOpacity);
        }
        if (filter.source.empty() || filter.source.find("vec4") == std::string::npos ||
            filter.source.find("addFilter") == std::string::npos ||
            filter.source.find("#version") != std::string::npos ||
            filter.source.find("void main") != std::string::npos ||
            filter.source.find("u_texture") != std::string::npos ||
            filter.source.find("v_texCoord") != std::string::npos ||
            filter.source.find("fragColor") != std::string::npos) {
            return AppearanceApplyResult::failure(AppearanceError::InvalidFilterSource);
        }
        for (const auto &texture: filter.textures) {
            if (texture.width <= 0 || texture.height <= 0) {
                return AppearanceApplyResult::failure(AppearanceError::InvalidFilterTexture);
            }
            const uint64_t expected = static_cast<uint64_t>(texture.width) *
                                      static_cast<uint64_t>(texture.height) * 4U;
            if (expected > std::numeric_limits<size_t>::max() ||
                texture.rgba8888.size() != static_cast<size_t>(expected)) {
                return AppearanceApplyResult::failure(AppearanceError::InvalidFilterTexture);
            }
        }
        return AppearanceApplyResult::success();
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

    AppearanceSnapshot retainedAppearance;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        retainedAppearance = appearance_;
    }
    bool attached = false;
    {
        std::lock_guard<std::mutex> renderLock(rendererMutex_);
        attached = renderer_.surfaceAvailable(window, retainedAppearance);
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

AppearanceApplyResult VideoPlayback::applyAppearance(
        const AppearanceSnapshot &appearance) {
    AppearanceApplyResult validation = validateAppearance(appearance);
    if (!validation.accepted()) return validation;

    std::lock_guard<std::mutex> renderLock(rendererMutex_);
    bool surfaceReady = false;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (state_ == PlaybackState::Released) {
            return AppearanceApplyResult::failure(AppearanceError::Released);
        }
        surfaceReady = surfaceReady_;
        if (!surfaceReady) {
            if (appearance.filter && !(appearance.filter == appearance_.filter)) {
                return AppearanceApplyResult::failure(AppearanceError::SurfaceUnavailable);
            }
            appearance_ = appearance;
            return AppearanceApplyResult::success();
        }
    }

    AppearanceApplyResult result = renderer_.applyAppearance(appearance);
    if (!result.accepted()) return result;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (state_ == PlaybackState::Released || !surfaceReady_) {
            return AppearanceApplyResult::failure(
                    state_ == PlaybackState::Released
                    ? AppearanceError::Released
                    : AppearanceError::SurfaceUnavailable);
        }
        appearance_ = appearance;
    }
    return AppearanceApplyResult::success();
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

bool VideoPlayback::representFrame() {
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (state_ == PlaybackState::Released || !surfaceReady_) {
            return false;
        }
    }
    // Presentation only. Deliberately does not touch controlVersion_, waitCv_,
    // pendingSeek_, state_, or appearance_: a redraw must never wake the decode
    // loop, move the playhead, or change playback state. rendererMutex_ alone
    // serializes it against a concurrently presenting frame.
    std::lock_guard<std::mutex> renderLock(rendererMutex_);
    return renderer_.representFrame();
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
    currentKind_ = PlaybackKind::Single;
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

uint64_t VideoPlayback::playTimeline(std::vector<TimelineSegment> segments) {
    joinFinishedWorker();

    std::lock_guard<std::mutex> lock(stateMutex_);
    if (state_ == PlaybackState::Released || !surfaceReady_ || segments.empty() ||
        currentAttemptId_ != 0 || worker_.joinable()) {
        return 0;
    }

    uint64_t attemptId = nextAttemptId_++;
    if (attemptId == 0) {
        attemptId = nextAttemptId_++;
    }
    currentKind_ = PlaybackKind::Timeline;
    currentAttemptId_ = attemptId;
    terminalClaimed_ = false;
    pendingSeek_.reset();
    ++controlVersion_;
    cancelRequested_.store(false, std::memory_order_release);
    state_ = PlaybackState::Starting;

    try {
        worker_ = std::thread(&VideoPlayback::runTimeline, this, attemptId,
                              std::move(segments));
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
    PlaybackKind failedKind = PlaybackKind::Single;
    uint64_t failedAttemptId = 0;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        surfaceReady_ = false;
        pendingSeek_.reset();
        ++controlVersion_;
        if (state_ != PlaybackState::Released && currentAttemptId_ != 0 && !terminalClaimed_) {
            terminalClaimed_ = true;
            failedAttemptId = currentAttemptId_;
            failedKind = currentKind_;
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
        callback(failedAttemptId, failedKind, PlaybackErrorCode::Render, -1);
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
        const std::string &path,
        int64_t startMs,
        int64_t endMs,
        double speed,
        const std::optional<AppearanceSnapshot> &appearance,
        bool honorLooping) {
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

    // Per-segment appearance and speed for a timeline segment. Applied after
    // the segment's decoder is ready but before its first frame is presented,
    // so each segment renders with its own look and rate (AC-4, AC-5). The
    // single-clip path passes no appearance and speed 0.0 (leave as-is).
    if (appearance.has_value()) {
        std::lock_guard<std::mutex> renderLock(rendererMutex_);
        if (isCancelled(attemptId)) {
            return PlaybackErrorCode::Decode;
        }
        if (!renderer_.applyAppearance(*appearance).accepted()) {
            return PlaybackErrorCode::Render;
        }
        std::lock_guard<std::mutex> lock(stateMutex_);
        appearance_ = *appearance;
    }
    if (speed > 0.0) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        playbackSpeed_ = speed;
        ++controlVersion_;
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

    // Segment window in the stream-relative mediaUs space. startMs (A) is the
    // inclusive lower bound; endMs (B) is the exclusive upper bound, or a
    // negative value for "play to the natural end". The single-clip path passes
    // (0, -1), leaving both bounds inert.
    const int64_t segmentStartUs =
            startMs > 0
            ? (startMs > std::numeric_limits<int64_t>::max() / 1000
               ? std::numeric_limits<int64_t>::max()
               : startMs * 1000)
            : 0;
    const int64_t segmentEndUs =
            endMs >= 0
            ? (endMs > std::numeric_limits<int64_t>::max() / 1000
               ? std::numeric_limits<int64_t>::max()
               : endMs * 1000)
            : -1;

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

    enum class ScheduleResult {
        Present, Drop, Seek, Cancelled
    };
    enum class DecodeFlow {
        NeedInput, Seek, Cancelled, DecodeError, RenderError, EndOfSegment
    };

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
                    // Effect clock: media time within the kept interval, so the effect
                    // animates with playback and a seek lands on the same phase every
                    // time. mediaUs is already segment-relative.
                    presented = renderer_.pushFrame(
                            rgba.data(), width, height,
                            static_cast<float>(static_cast<double>(mediaUs) / 1e6));
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

            // Enforce the segment end (B): stop before the first frame whose
            // presentation timestamp reaches the exclusive upper bound so that
            // [A, B) is presented and the timeline advances (AC-3). A negative
            // segmentEndUs (single-clip / open-ended segment) never triggers.
            if (segmentEndUs >= 0 && mediaUs >= segmentEndUs) {
                av_frame_unref(resources.frame);
                return DecodeFlow::EndOfSegment;
            }

            // Enforce the segment start (A): drop frames decoded between the
            // seek keyframe and A so presentation begins at A (AC-2). The
            // single-clip path uses segmentStartUs 0 and never drops here.
            if (mediaUs < segmentStartUs) {
                av_frame_unref(resources.frame);
                continue;
            }

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

    // Enforce the segment start (A): seek backward to the keyframe at or before
    // A so the prefix is not decoded from zero; frames between the keyframe and
    // A are dropped below by the segmentStartUs skip. Single-clip attempts pass
    // startMs 0 and skip this entirely, preserving the whole-file path.
    if (segmentStartUs > 0) {
        if (!resetDecoder(startMs, false, 0, true)) {
            return PlaybackErrorCode::Decode;
        }
    }

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
            if (flow == DecodeFlow::EndOfSegment) {
                // Reached the exclusive end B: the segment presented [A, B).
                // Return success so a timeline advances to the next segment.
                return std::nullopt;
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
        if (drainFlow == DecodeFlow::EndOfSegment) {
            return std::nullopt;
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
            // honorLooping is false for timeline segments so an individual
            // segment never restarts on natural EOF; only the whole timeline
            // loops back to segment 0, orchestrated by runTimeline.
            shouldLoop = !seekAtEof && honorLooping && looping_;
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
        // Single-clip attempt: the whole file, no trim, natural speed, no
        // per-attempt appearance override, and looping honored. These defaults
        // reproduce the pre-timeline single-clip path byte-for-byte (AC-8).
        error = decodeAttempt(attemptId, path, /*startMs=*/0, /*endMs=*/-1,
                              /*speed=*/0.0, /*appearance=*/std::nullopt,
                              /*honorLooping=*/true);
    } catch (...) {
        LOGE("Unhandled native failure while decoding playback attempt");
        error = PlaybackErrorCode::Decode;
    }
    if (!isCancelled(attemptId)) {
        finishAttempt(attemptId, PlaybackKind::Single, error, -1);
    }
}

void VideoPlayback::runTimeline(uint64_t attemptId,
                                std::vector<TimelineSegment> segments) {
    // One worker sequences every segment back-to-back on the shared surface.
    // Each segment is a bounded decodeAttempt with its own window, speed, and
    // appearance and with segment-level looping disabled (honorLooping=false);
    // only the whole timeline loops, restarting from segment 0 when looping_ is
    // set. Exactly one terminal event is emitted for the whole timeline.
    do {
        for (size_t i = 0; i < segments.size(); ++i) {
            if (isCancelled(attemptId)) {
                return;
            }
            const TimelineSegment &segment = segments[i];
            std::optional<PlaybackErrorCode> error;
            try {
                error = decodeAttempt(attemptId, segment.path, segment.startMs,
                                      segment.endMs, segment.speed,
                                      std::optional<AppearanceSnapshot>(segment.appearance),
                                      /*honorLooping=*/false);
            } catch (...) {
                LOGE("Unhandled native failure while decoding timeline segment");
                error = PlaybackErrorCode::Decode;
            }
            if (error.has_value()) {
                // Fail-fast: end the whole timeline with the failing segment's
                // index (D-6). Suppress if the attempt was already claimed
                // (stop/release/surface-loss) to avoid a late callback.
                if (!isCancelled(attemptId)) {
                    finishAttempt(attemptId, PlaybackKind::Timeline, error,
                                  static_cast<int>(i));
                }
                return;
            }
        }

        // All segments completed. Restart from segment 0 when whole-timeline
        // looping is enabled and the attempt is still current (D-9).
        bool loopTimeline = false;
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            loopTimeline = looping_ && !terminalClaimed_ &&
                           currentAttemptId_ == attemptId &&
                           !cancelRequested_.load(std::memory_order_acquire) &&
                           surfaceReady_ && state_ != PlaybackState::Released;
        }
        if (!loopTimeline) {
            break;
        }
    } while (!isCancelled(attemptId));

    if (!isCancelled(attemptId)) {
        finishAttempt(attemptId, PlaybackKind::Timeline, std::nullopt, -1);
    }
}

void VideoPlayback::finishAttempt(
        uint64_t attemptId,
        PlaybackKind kind,
        std::optional<PlaybackErrorCode> error,
        int segmentIndex) {
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
        callback(attemptId, kind, error, segmentIndex);
    }
}
