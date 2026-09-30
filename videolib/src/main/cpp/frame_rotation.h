// Display-orientation support shared by preview (video_playback.cpp) and export
// (frame_source.cpp).
//
// Phone recordings are usually stored in the sensor's landscape layout and carry
// a display matrix (MP4 `tkhd` rotation, e.g. 90 for a portrait clip) telling
// the player how to turn them upright. The decoder ignores that matrix, so both
// pipelines rotate each decoded RGBA frame here, before it reaches the GL effect
// pass. Everything downstream — aspect-fit, filters/effects that read uv, the
// export encoder dimensions — then sees the frame as the user sees it in the
// gallery, and the exported file needs no rotation tag of its own.
#ifndef VIDEOLIB_FRAME_ROTATION_H
#define VIDEOLIB_FRAME_ROTATION_H

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

extern "C" {
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/display.h>
}

namespace frame_rotation {

    // Clockwise rotation, in degrees (0, 90, 180 or 270), that turns the stream's
    // stored frames upright. Mirrors ffmpeg's own autorotate: the display matrix
    // encodes a counter-clockwise angle, so the frame is turned by its negation.
    // Flips in the matrix are ignored (not produced by phone cameras).
    inline int clockwiseDegrees(const AVStream *stream) {
        if (stream == nullptr || stream->codecpar == nullptr) return 0;
        const AVPacketSideData *sd = av_packet_side_data_get(
                stream->codecpar->coded_side_data, stream->codecpar->nb_coded_side_data,
                AV_PKT_DATA_DISPLAYMATRIX);
        if (sd == nullptr || sd->size < static_cast<size_t>(9 * sizeof(int32_t))) return 0;
        const double angle = av_display_rotation_get(
                reinterpret_cast<const int32_t *>(sd->data));
        if (std::isnan(angle)) return 0;
        int degrees = static_cast<int>(std::lround(-angle / 90.0)) * 90;
        degrees %= 360;
        if (degrees < 0) degrees += 360;
        return degrees;
    }

    inline bool swapsAxes(int degrees) { return degrees == 90 || degrees == 270; }

    // Rotates a tightly packed RGBA8888 frame clockwise by `degrees` into `dst`
    // (resized). The output is height x width when the axes swap. Callers skip
    // this entirely for 0 degrees.
    inline void rotateRgba(const uint8_t *src, int width, int height, int degrees,
                           std::vector<uint8_t> *dst) {
        dst->resize(static_cast<size_t>(width) * height * 4);
        const auto *in = reinterpret_cast<const uint32_t *>(src);
        auto *out = reinterpret_cast<uint32_t *>(dst->data());
        switch (degrees) {
            case 90: // dst is height x width; dst(x', y') = src(y', height-1-x')
                for (int y = 0; y < width; ++y) {
                    uint32_t *row = out + static_cast<size_t>(y) * height;
                    for (int x = 0; x < height; ++x) {
                        row[x] = in[static_cast<size_t>(height - 1 - x) * width + y];
                    }
                }
                break;
            case 180:
                for (int y = 0; y < height; ++y) {
                    const uint32_t *srcRow = in + static_cast<size_t>(height - 1 - y) * width;
                    uint32_t *row = out + static_cast<size_t>(y) * width;
                    for (int x = 0; x < width; ++x) {
                        row[x] = srcRow[width - 1 - x];
                    }
                }
                break;
            case 270: // dst is height x width; dst(x', y') = src(width-1-y', x')
                for (int y = 0; y < width; ++y) {
                    uint32_t *row = out + static_cast<size_t>(y) * height;
                    const int srcX = width - 1 - y;
                    for (int x = 0; x < height; ++x) {
                        row[x] = in[static_cast<size_t>(x) * width + srcX];
                    }
                }
                break;
            default:
                std::memcpy(dst->data(), src, dst->size());
                break;
        }
    }
} // namespace frame_rotation

#endif // VIDEOLIB_FRAME_ROTATION_H
