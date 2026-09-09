#include "video_playback.h"

#include <android/log.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
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

void VideoPlayback::releaseSurface() {
    std::thread activeWorker;
    PlaybackTerminalCallback callback;
    uint64_t failedAttemptId = 0;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        surfaceReady_ = false;
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

bool VideoPlayback::waitUntil(
        uint64_t attemptId,
        std::chrono::steady_clock::time_point deadline) {
    std::unique_lock<std::mutex> lock(waitMutex_);
    waitCv_.wait_until(lock, deadline, [this, attemptId] {
        return isCancelled(attemptId);
    });
    return !isCancelled(attemptId);
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

    bool clockStarted = false;
    bool hasSourceTimestamp = false;
    bool presentedAnyFrame = false;
    int64_t firstSourceUs = 0;
    int64_t lastPresentationUs = -fallbackFrameDurationUs;
    std::chrono::steady_clock::time_point clockOrigin;
    std::vector<uint8_t> rgba;

    auto receiveFrames = [&]() -> std::optional<PlaybackErrorCode> {
        while (!isCancelled(attemptId)) {
            const int receiveResult = avcodec_receive_frame(resources.codec, resources.frame);
            if (receiveResult == AVERROR(EAGAIN) || receiveResult == AVERROR_EOF) {
                return std::nullopt;
            }
            if (receiveResult < 0) {
                return PlaybackErrorCode::Decode;
            }

            int64_t presentationUs;
            const int64_t timestamp = resources.frame->best_effort_timestamp;
            if (timestamp != AV_NOPTS_VALUE) {
                const int64_t sourceUs = av_rescale_q(timestamp, stream->time_base,
                                                     kMicrosecondTimeBase);
                if (!hasSourceTimestamp) {
                    firstSourceUs = sourceUs;
                    hasSourceTimestamp = true;
                }
                presentationUs = std::max<int64_t>(0, sourceUs - firstSourceUs);
                presentationUs = std::max(presentationUs, lastPresentationUs);
            } else {
                presentationUs = std::max<int64_t>(0,
                                                   lastPresentationUs + fallbackFrameDurationUs);
            }
            lastPresentationUs = presentationUs;

            if (!clockStarted) {
                clockOrigin = std::chrono::steady_clock::now();
                clockStarted = true;
            }
            const auto deadline = clockOrigin + std::chrono::microseconds(presentationUs);
            if (!waitUntil(attemptId, deadline)) {
                return PlaybackErrorCode::Decode;
            }

            const int width = resources.frame->width;
            const int height = resources.frame->height;
            if (width <= 0 || height <= 0) {
                return PlaybackErrorCode::Decode;
            }
            resources.sws = sws_getCachedContext(
                    resources.sws,
                    width,
                    height,
                    static_cast<AVPixelFormat>(resources.frame->format),
                    width,
                    height,
                    AV_PIX_FMT_RGBA,
                    SWS_BILINEAR,
                    nullptr,
                    nullptr,
                    nullptr);
            if (resources.sws == nullptr) {
                return PlaybackErrorCode::Decode;
            }
            const int rgbaSize = av_image_get_buffer_size(AV_PIX_FMT_RGBA, width, height, 1);
            if (rgbaSize <= 0) {
                return PlaybackErrorCode::Decode;
            }
            rgba.resize(static_cast<size_t>(rgbaSize));
            uint8_t *destinationData[4] = {};
            int destinationLinesize[4] = {};
            if (av_image_fill_arrays(destinationData, destinationLinesize, rgba.data(),
                                     AV_PIX_FMT_RGBA, width, height, 1) < 0 ||
                sws_scale(resources.sws,
                          resources.frame->data,
                          resources.frame->linesize,
                          0,
                          height,
                          destinationData,
                          destinationLinesize) <= 0) {
                return PlaybackErrorCode::Decode;
            }

            bool presented = false;
            {
                std::lock_guard<std::mutex> renderLock(rendererMutex_);
                if (!isCancelled(attemptId)) {
                    presented = renderer_.pushFrame(rgba.data(), width, height);
                }
            }
            av_frame_unref(resources.frame);
            if (isCancelled(attemptId)) {
                return PlaybackErrorCode::Decode;
            }
            if (!presented) {
                return PlaybackErrorCode::Render;
            }
            presentedAnyFrame = true;
        }
        return std::nullopt;
    };

    int readResult = 0;
    while (!isCancelled(attemptId) &&
           (readResult = av_read_frame(resources.format, resources.packet)) >= 0) {
        if (resources.packet->stream_index == videoStreamIndex) {
            const int sendResult = avcodec_send_packet(resources.codec, resources.packet);
            av_packet_unref(resources.packet);
            if (sendResult < 0) {
                return PlaybackErrorCode::Decode;
            }
            if (const auto error = receiveFrames()) {
                return error;
            }
        } else {
            av_packet_unref(resources.packet);
        }
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
    if (const auto error = receiveFrames()) {
        return error;
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
