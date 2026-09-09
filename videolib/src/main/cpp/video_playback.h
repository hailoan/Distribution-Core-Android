#ifndef VIDEOLIB_VIDEO_PLAYBACK_H
#define VIDEOLIB_VIDEO_PLAYBACK_H

#include <android/native_window.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "preview_renderer.h"

enum class PlaybackState {
    Idle,
    Starting,
    Playing,
    Paused,
    Seeking,
    Stopping,
    Completed,
    Failed,
    Released,
};

// Values 1-4 are the private JNI contract consumed by VideoPreview.kt.
enum class PlaybackErrorCode : int {
    InputOpen = 1,
    UnsupportedVideo = 2,
    Decode = 3,
    Render = 4,
};

// A missing error denotes natural completion.
using PlaybackTerminalCallback =
        std::function<void(uint64_t, std::optional<PlaybackErrorCode>)>;

// Per-VideoPreview native owner. It coordinates exactly one playback attempt,
// owns the existing renderer, and keeps FFmpeg work off both JNI callers and
// the EGL/GLES executor.
class VideoPlayback {
public:
    explicit VideoPlayback(PlaybackTerminalCallback terminalCallback);
    ~VideoPlayback();

    VideoPlayback(const VideoPlayback &) = delete;
    VideoPlayback &operator=(const VideoPlayback &) = delete;

    // Takes ownership of the acquired ANativeWindow reference.
    bool surfaceAvailable(ANativeWindow *window);
    bool pushFrame(const uint8_t *pixels, int width, int height);
    void requestPattern();
    void releaseSurface();

    // Returns a positive attempt ID when accepted, otherwise zero.
    uint64_t play(const std::string &path);
    void stop();
    bool pause();
    bool resume();
    bool setLooping(bool enabled);
    bool setPlaybackSpeed(double speed);
    bool seekTo(int64_t positionMs);
    void release();

private:
    struct SeekRequest {
        int64_t positionMs;
        uint64_t id;
        bool resumeAfter;
    };

    std::optional<PlaybackErrorCode> decodeAttempt(
            uint64_t attemptId,
            const std::string &path);
    void runAttempt(uint64_t attemptId, std::string path);
    void finishAttempt(
            uint64_t attemptId,
            std::optional<PlaybackErrorCode> error);
    bool markPlaying(uint64_t attemptId);
    bool isCancelled(uint64_t attemptId) const;
    void joinFinishedWorker();

    PlaybackTerminalCallback terminalCallback_;
    PreviewRenderer renderer_;

    mutable std::mutex stateMutex_;
    std::mutex rendererMutex_;
    std::condition_variable waitCv_;
    std::thread worker_;

    std::atomic<bool> cancelRequested_{false};
    PlaybackState state_ = PlaybackState::Idle;
    uint64_t currentAttemptId_ = 0;
    uint64_t nextAttemptId_ = 1;
    uint64_t nextSeekId_ = 1;
    uint64_t latestSeekId_ = 0;
    uint64_t controlVersion_ = 0;
    bool terminalClaimed_ = false;
    bool surfaceReady_ = false;
    bool looping_ = false;
    bool seekResumeAfter_ = true;
    double playbackSpeed_ = 1.0;
    std::optional<SeekRequest> pendingSeek_;
};

#endif // VIDEOLIB_VIDEO_PLAYBACK_H
