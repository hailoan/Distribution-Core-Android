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
#include <vector>

#include "preview_renderer.h"
#include "appearance.h"

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

// Distinguishes a single-clip attempt (play) from an ordered timeline attempt
// (playTimeline) so the JNI bridge routes the terminal event to the matching
// Kotlin callback family.
enum class PlaybackKind { Single, Timeline };

// One trimmed segment of a sequential timeline. startMs is the inclusive start
// (A); endMs is the exclusive end (B), or a negative value for "the natural end
// of the file". speed and appearance apply only while this segment presents.
struct TimelineSegment {
    std::string path;
    int64_t startMs = 0;
    int64_t endMs = -1;
    double speed = 1.0;
    AppearanceSnapshot appearance;
};

// A missing error denotes natural completion. segmentIndex identifies the
// failing timeline segment (0-based); it is -1 for single-clip attempts and for
// timeline completion.
using PlaybackTerminalCallback =
        std::function<void(uint64_t, PlaybackKind, std::optional<PlaybackErrorCode>, int)>;

// Per-VideoPreview native owner. It coordinates exactly one playback attempt
// (single clip or an ordered timeline), owns the existing renderer, and keeps
// FFmpeg work off both JNI callers and the EGL/GLES executor.
class VideoPlayback {
public:
    explicit VideoPlayback(PlaybackTerminalCallback terminalCallback);

    ~VideoPlayback();

    VideoPlayback(const VideoPlayback &) = delete;

    VideoPlayback &operator=(const VideoPlayback &) = delete;

    // Takes ownership of the acquired ANativeWindow reference.
    bool surfaceAvailable(ANativeWindow *window);

    bool pushFrame(const uint8_t *pixels, int width, int height);

    AppearanceApplyResult applyAppearance(const AppearanceSnapshot &appearance);

    // Redraws the retained frame through the current appearance. Opt-in and
    // presentation-only: it does not decode, seek, move the playhead, or alter
    // playback state, so an appearance accepted while paused becomes visible.
    // Returns false when released, surfaceless, or no frame has been presented.
    bool representFrame();

    void requestPattern();

    void releaseSurface();

    // Returns a positive attempt ID when accepted, otherwise zero.
    uint64_t play(const std::string &path);

    // Starts an ordered timeline attempt: each segment's [startMs, endMs)
    // interval is presented back-to-back on the shared surface with its own
    // appearance and speed. Returns a positive attempt ID when accepted,
    // otherwise zero. Exactly one terminal event is reported.
    uint64_t playTimeline(std::vector<TimelineSegment> segments);

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

    // Presents one clip. When [startMs, endMs) is a bounded window the decoder
    // seeks to startMs, skips until it, and stops before endMs; the default
    // window (0, -1) reproduces the whole-file single-clip path. appearance,
    // when present, is applied before the first presented frame; looping_ is
    // honored only when honorLooping is true (single-clip attempts).
    std::optional<PlaybackErrorCode> decodeAttempt(
            uint64_t attemptId,
            const std::string &path,
            int64_t startMs,
            int64_t endMs,
            double speed,
            const std::optional<AppearanceSnapshot> &appearance,
            bool honorLooping);

    void runAttempt(uint64_t attemptId, std::string path);

    void runTimeline(uint64_t attemptId, std::vector<TimelineSegment> segments);

    void finishAttempt(
            uint64_t attemptId,
            PlaybackKind kind,
            std::optional<PlaybackErrorCode> error,
            int segmentIndex);

    bool markPlaying(uint64_t attemptId);

    bool isCancelled(uint64_t attemptId) const;

    void joinFinishedWorker();

    PlaybackTerminalCallback terminalCallback_;
    PreviewRenderer renderer_;
    AppearanceSnapshot appearance_;

    mutable std::mutex stateMutex_;
    std::mutex rendererMutex_;
    std::condition_variable waitCv_;
    std::thread worker_;

    std::atomic<bool> cancelRequested_{false};
    PlaybackState state_ = PlaybackState::Idle;
    PlaybackKind currentKind_ = PlaybackKind::Single;
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
