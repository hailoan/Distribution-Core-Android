#ifndef VIDEOLIB_VIDEO_EXPORT_H
#define VIDEOLIB_VIDEO_EXPORT_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "appearance.h"

// Terminal error taxonomy for one export attempt. The integer values are the
// private JNI contract consumed by VideoExporter.kt (mapped to ExportError):
// they are DISTINCT from PlaybackErrorCode (video_playback.h) so the two
// pipelines never share a wire enum. A missing error means success.
enum class ExportErrorCode : int {
    InputOpen = 1,        // -> ExportError.INPUT_OPEN
    UnsupportedVideo = 2, // -> ExportError.UNSUPPORTED_VIDEO
    Decode = 3,           // -> ExportError.DECODE
    Render = 4,           // -> ExportError.RENDER (offscreen EGL/GLES effect)
    Encode = 5,           // -> ExportError.ENCODE (MediaCodec H.264)
    Mux = 6,              // -> ExportError.MUX (FFmpeg MP4 container)
    Output = 7,           // -> ExportError.OUTPUT (output file I/O)
};

// One clip to export. A single-clip export is a one-element timeline whose
// window is the whole file (startMs 0, endMs -1). startMs is the inclusive
// start (A); endMs is the exclusive end (B), or a negative value for the
// natural end of the file. speed and appearance apply for the whole clip.
struct ExportSegment {
    std::string path;
    int64_t startMs = 0;
    int64_t endMs = -1;
    double speed = 1.0;
    AppearanceSnapshot appearance;
};

// A complete export request: an ordered list of segments concatenated into one
// output file, plus the output destination and whether source audio is kept.
struct ExportRequest {
    std::vector<ExportSegment> segments;
    std::string outputPath;
    bool includeAudio = true;
};

enum class ExportState {
    Idle,
    Preparing,
    Exporting,
    Completed,
    Failed,
    Cancelled,
    Released,
};

// A missing error denotes success. segmentIndex identifies the failing segment
// (0-based) for a timeline; it is -1 for a single-clip attempt and for success.
using ExportTerminalCallback =
        std::function<void(uint64_t, std::optional<ExportErrorCode>, int)>;

// Per-VideoExporter native owner. Coordinates exactly one export attempt on a
// dedicated worker thread: decode (FrameSource) -> offscreen effect
// (OffscreenRenderer) -> H.264 encode (H264Encoder) -> MP4 mux (Mp4Muxer),
// with optional audio transcode (AudioTranscoder). Keeps all FFmpeg/GL/codec
// work off the JNI caller thread. Reports exactly one terminal event.
class VideoExport {
public:
    explicit VideoExport(ExportTerminalCallback terminalCallback);

    ~VideoExport();

    VideoExport(const VideoExport &) = delete;

    VideoExport &operator=(const VideoExport &) = delete;

    // Returns a positive attempt ID when accepted, otherwise zero.
    uint64_t start(ExportRequest request);

    // Cancels an active attempt. When this returns, the cancelled attempt can no
    // longer report success. Idempotent.
    void cancel();

    void release();

private:
    void runExport(uint64_t attemptId, ExportRequest request);

    // Exports one segment into the already-open encoder/muxer. Returns an error
    // on failure, or nullopt when the segment's frames were all written. The
    // running video PTS baseline (for continuous timeline PTS) is threaded via
    // basePtsUs, updated to the next free PTS on success.
    std::optional<ExportErrorCode> exportSegment(
            uint64_t attemptId,
            const ExportSegment &segment,
            class OffscreenRenderer &renderer,
            class H264Encoder &encoder,
            class Mp4Muxer &muxer,
            class AudioTranscoder *audio,
            int64_t *basePtsUs);

    void finish(uint64_t attemptId, std::optional<ExportErrorCode> error, int segmentIndex);

    bool isCancelled(uint64_t attemptId) const;

    void joinFinishedWorker();

    ExportTerminalCallback terminalCallback_;

    mutable std::mutex stateMutex_;
    std::thread worker_;
    std::atomic<bool> cancelRequested_{false};

    ExportState state_ = ExportState::Idle;
    uint64_t currentAttemptId_ = 0;
    uint64_t nextAttemptId_ = 1;
    bool terminalClaimed_ = false;
};

#endif // VIDEOLIB_VIDEO_EXPORT_H
