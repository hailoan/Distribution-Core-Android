#ifndef VIDEOLIB_H264_ENCODER_H
#define VIDEOLIB_H264_ENCODER_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <media/NdkMediaCodec.h>

extern "C" {
#include <libswscale/swscale.h>
}

// H.264 encoder backed by Android MediaCodec (AMediaCodec) with BYTE-BUFFER
// input — deliberately not AMediaCodec_createInputSurface, which requires API 26
// (:videolib minSdk is 21). The LGPL FFmpeg build has no usable software H.264
// encoder, so MediaCodec is the encode path (SOLUTION-DESIGN D-3).
//
// Input frames are RGBA8888 as read back from the offscreen GL pass (bottom-left
// origin); the encoder converts RGBA -> NV12 with a vertical flip so the encoded
// image is upright. Encoded output is handed to caller sinks, NOT to a muxer:
// the MP4 container is written by FFmpeg (Mp4Muxer), so this class only produces
// the H.264 elementary stream plus its codec-config (SPS/PPS).
//
// Runs synchronously on the caller's export worker thread (offline export; no
// real-time cadence to preserve, so no internal thread and no frame dropping).
class H264Encoder {
public:
    struct EncodedSample {
        const uint8_t *data;
        size_t size;
        int64_t ptsUs;
        bool keyframe;
    };

    // Receives the codec-config bytes (Annex-B SPS/PPS) exactly once, before any
    // sample. Return false to abort. Used by the muxer to build H.264 extradata.
    using ConfigSink = std::function<bool(const uint8_t *data, size_t size)>;
    // Receives each encoded access unit in order. Return false to abort.
    using SampleSink = std::function<bool(const EncodedSample &)>;

    H264Encoder() = default;

    ~H264Encoder();

    H264Encoder(const H264Encoder &) = delete;

    H264Encoder &operator=(const H264Encoder &) = delete;

    // Configure + start the encoder. width/height are rounded down to even.
    // Returns false on any codec failure.
    bool start(int width, int height, int fps, int bitrate,
               ConfigSink configSink, SampleSink sampleSink);

    // Encode one RGBA8888 frame (bottom-left origin) at output PTS `ptsUs`.
    // Returns false on codec/conversion/sink failure. Blocks until an input
    // buffer is available (draining output meanwhile) — never drops frames.
    bool encode(const uint8_t *rgba, int width, int height, int64_t ptsUs);

    // Flush: signal end-of-stream and drain all remaining output. Returns false
    // on failure. Safe to call once after the last encode().
    bool finish();

    int width() const { return width_; }

    int height() const { return height_; }

private:
    bool drainOutput(bool endOfStream);

    void closeCodec();

    AMediaCodec *codec_ = nullptr;
    SwsContext *sws_ = nullptr;
    std::vector<uint8_t> nv12_;

    int width_ = 0;
    int height_ = 0;
    bool started_ = false;
    bool configDelivered_ = false;
    bool aborted_ = false;

    ConfigSink configSink_;
    SampleSink sampleSink_;
};

#endif // VIDEOLIB_H264_ENCODER_H
