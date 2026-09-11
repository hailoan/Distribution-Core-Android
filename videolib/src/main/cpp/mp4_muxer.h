#ifndef VIDEOLIB_MP4_MUXER_H
#define VIDEOLIB_MP4_MUXER_H

#include <cstdint>
#include <string>
#include <vector>

#include "h264_encoder.h" // EncodedSample

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

// MP4 container writer backed by FFmpeg's mov/mp4 muxer (SOLUTION-DESIGN D-5;
// the clarified "FFmpeg for MP4 muxing" path). The H.264 elementary stream comes
// from MediaCodec (H264Encoder) in Annex-B form; this class builds the avcC
// extradata from the codec-config SPS/PPS and converts each Annex-B access unit
// to length-prefixed AVCC before writing. AAC audio packets (from
// AudioTranscoder) are written on an optional second stream.
//
// Lifecycle: open() -> addVideoStream() [-> addAudioStream()] -> writeHeader()
// -> writeVideoSample()/writeAudioPacket()... -> finish(). On any failure the
// caller (VideoExport) deletes the partial file.
class Mp4Muxer {
public:
    Mp4Muxer() = default;

    ~Mp4Muxer();

    Mp4Muxer(const Mp4Muxer &) = delete;

    Mp4Muxer &operator=(const Mp4Muxer &) = delete;

    // Allocate the output context and open the file for writing.
    bool open(const std::string &path);

    // Add the H.264 video stream. `configAnnexB` is the MediaCodec codec-config
    // (Annex-B SPS/PPS); avcC extradata is derived from it. Must be called
    // before writeHeader(). fps is used only to set a nominal frame rate hint.
    bool addVideoStream(const uint8_t *configAnnexB, size_t configSize,
                        int width, int height, int fps);

    // Add an AAC audio stream whose codec parameters (incl. extradata) are
    // copied from the audio encoder context. Optional; call before writeHeader().
    bool addAudioStream(const AVCodecContext *audioEncoder);

    bool hasAudioStream() const { return audioStreamIndex_ >= 0; }

    // Write the container header. Call once after all streams are added.
    bool writeHeader();

    // Write one encoded H.264 access unit (Annex-B). ptsUs is in microseconds.
    bool writeVideoSample(const H264Encoder::EncodedSample &sample);

    // Write one AAC packet. `pkt` PTS/DTS are in `srcTimeBase`; ownership stays
    // with the caller (the packet is unref'd by the interleaved writer copy).
    bool writeAudioPacket(AVPacket *pkt, AVRational srcTimeBase);

    // Finalize: write the trailer and close the file. Returns false on failure.
    bool finish();

private:
    void closeAll();

    AVFormatContext *format_ = nullptr;
    int videoStreamIndex_ = -1;
    int audioStreamIndex_ = -1;
    AVRational videoTimeBase_{1, 1000000}; // our PTS unit before header rewrite
    bool headerWritten_ = false;
    bool trailerWritten_ = false;
    std::vector<uint8_t> avccBuf_; // AVCC-converted scratch for the current sample
};

#endif // VIDEOLIB_MP4_MUXER_H
