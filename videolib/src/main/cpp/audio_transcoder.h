#ifndef VIDEOLIB_AUDIO_TRANSCODER_H
#define VIDEOLIB_AUDIO_TRANSCODER_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "video_export.h" // ExportErrorCode

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
}

// Audio side of export: decode the source audio stream, re-time it with the
// `atempo` filter so it matches the video speed transform (A/V sync), and
// encode to AAC (the LGPL FFmpeg build has ff_aac_encoder). Encoded packets are
// handed to a sink (the muxer). When the source has no audio stream, hasAudio()
// is false and the whole stage is inert (video-only export, SOLUTION-DESIGN A-2).
//
// One AudioTranscoder handles one source file window at a time; for a timeline,
// VideoExport constructs a transcoder per segment. Not thread-safe.
class AudioTranscoder {
public:
    // Receives one encoded AAC packet (in the encoder time base). Return false
    // to abort. The packet is owned by the transcoder and unref'd after return.
    using PacketSink = std::function<bool(AVPacket *pkt, AVRational timeBase)>;

    explicit AudioTranscoder(const std::atomic<bool> *cancel);

    ~AudioTranscoder();

    AudioTranscoder(const AudioTranscoder &) = delete;

    AudioTranscoder &operator=(const AudioTranscoder &) = delete;

    // Open the file and, if it has an audio stream, set up decode + atempo +
    // AAC encode for the given playback `speed`. Returns nullopt on success
    // (including the no-audio case; check hasAudio()); an error otherwise.
    std::optional<ExportErrorCode> open(const std::string &path, double speed);

    bool hasAudio() const { return hasAudio_; }

    // The AAC encoder context (for Mp4Muxer::addAudioStream). Null if no audio.
    const AVCodecContext *encoderContext() const { return encoder_; }

    // Transcode the half-open window [startMs, endMs) delivering AAC packets to
    // `sink` whose PTS start at `ptsBaseUs` (microseconds) for timeline
    // continuity. No-op returning success when hasAudio() is false.
    std::optional<ExportErrorCode> transcode(int64_t startMs, int64_t endMs,
                                             int64_t ptsBaseUs, const PacketSink &sink);

private:
    std::optional<ExportErrorCode> initFilter(double speed);

    std::optional<ExportErrorCode> encodeAndSink(AVFrame *frame, int64_t ptsBaseUs,
                                                 const PacketSink &sink, bool *aborted);

    bool cancelled() const;

    const std::atomic<bool> *cancel_;

    AVFormatContext *format_ = nullptr;
    AVCodecContext *decoder_ = nullptr;
    AVCodecContext *encoder_ = nullptr;
    AVFilterGraph *filterGraph_ = nullptr;
    AVFilterContext *bufferSrc_ = nullptr;
    AVFilterContext *bufferSink_ = nullptr;
    AVPacket *packet_ = nullptr;
    AVFrame *decodeFrame_ = nullptr;
    AVFrame *filterFrame_ = nullptr;

    int audioStreamIndex_ = -1;
    bool hasAudio_ = false;
    int64_t nextPts_ = 0;         // running encoder PTS (encoder time base)
    int64_t encoderSampleCount_ = 0;
};

#endif // VIDEOLIB_AUDIO_TRANSCODER_H
