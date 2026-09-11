#include "mp4_muxer.h"

#include <android/log.h>

#include <cstring>
#include <vector>

extern "C" {
#include <libavutil/mem.h>
}

#define LOG_TAG "videolib.export.mux"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {
    // Split an Annex-B buffer into NAL units (payloads without the start code).
    // Handles both 3-byte (00 00 01) and 4-byte (00 00 00 01) start codes.
    std::vector<std::pair<const uint8_t *, size_t>>
    splitAnnexB(const uint8_t *data, size_t size) {
        std::vector<std::pair<const uint8_t *, size_t>> nals;
        size_t i = 0;
        auto isStart = [&](size_t p, int *scLen) -> bool {
            if (p + 3 <= size && data[p] == 0 && data[p + 1] == 0 && data[p + 2] == 1) {
                *scLen = 3;
                return true;
            }
            if (p + 4 <= size && data[p] == 0 && data[p + 1] == 0 && data[p + 2] == 0 &&
                data[p + 3] == 1) {
                *scLen = 4;
                return true;
            }
            return false;
        };
        // Find first start code.
        int sc = 0;
        while (i < size && !isStart(i, &sc)) ++i;
        while (i < size) {
            i += sc;
            const size_t nalStart = i;
            int nextSc = 0;
            while (i < size && !isStart(i, &nextSc)) ++i;
            const size_t nalEnd = i;
            if (nalEnd > nalStart) {
                nals.emplace_back(data + nalStart, nalEnd - nalStart);
            }
            sc = nextSc;
        }
        return nals;
    }

    // Build avcC (AVCDecoderConfigurationRecord) extradata from Annex-B SPS/PPS.
    // Returns false if a usable SPS/PPS pair is not present.
    bool buildAvcC(const uint8_t *configAnnexB, size_t configSize,
                   std::vector<uint8_t> *out) {
        const uint8_t *sps = nullptr, *pps = nullptr;
        size_t spsLen = 0, ppsLen = 0;
        for (const auto &nal : splitAnnexB(configAnnexB, configSize)) {
            const uint8_t type = nal.first[0] & 0x1F;
            if (type == 7 && sps == nullptr) {
                sps = nal.first;
                spsLen = nal.second;
            } else if (type == 8 && pps == nullptr) {
                pps = nal.first;
                ppsLen = nal.second;
            }
        }
        if (sps == nullptr || pps == nullptr || spsLen < 4) {
            LOGE("codec-config missing SPS/PPS");
            return false;
        }
        out->clear();
        out->push_back(1);          // configurationVersion
        out->push_back(sps[1]);     // AVCProfileIndication
        out->push_back(sps[2]);     // profile_compatibility
        out->push_back(sps[3]);     // AVCLevelIndication
        out->push_back(0xFF);       // 6 bits reserved + lengthSizeMinusOne (3 => 4 bytes)
        out->push_back(0xE1);       // 3 bits reserved + numOfSPS (1)
        out->push_back(static_cast<uint8_t>((spsLen >> 8) & 0xFF));
        out->push_back(static_cast<uint8_t>(spsLen & 0xFF));
        out->insert(out->end(), sps, sps + spsLen);
        out->push_back(1);          // numOfPPS
        out->push_back(static_cast<uint8_t>((ppsLen >> 8) & 0xFF));
        out->push_back(static_cast<uint8_t>(ppsLen & 0xFF));
        out->insert(out->end(), pps, pps + ppsLen);
        return true;
    }

    // Convert an Annex-B access unit to length-prefixed AVCC (4-byte lengths).
    void annexBToAvcc(const uint8_t *data, size_t size, std::vector<uint8_t> *out) {
        out->clear();
        for (const auto &nal : splitAnnexB(data, size)) {
            const size_t len = nal.second;
            out->push_back(static_cast<uint8_t>((len >> 24) & 0xFF));
            out->push_back(static_cast<uint8_t>((len >> 16) & 0xFF));
            out->push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
            out->push_back(static_cast<uint8_t>(len & 0xFF));
            out->insert(out->end(), nal.first, nal.first + len);
        }
    }
}

Mp4Muxer::~Mp4Muxer() {
    closeAll();
}

bool Mp4Muxer::open(const std::string &path) {
    if (avformat_alloc_output_context2(&format_, nullptr, "mp4", path.c_str()) < 0 ||
        format_ == nullptr) {
        LOGE("alloc_output_context2 failed");
        return false;
    }
    if (!(format_->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&format_->pb, path.c_str(), AVIO_FLAG_WRITE) < 0) {
            LOGE("avio_open(%s) failed", path.c_str());
            avformat_free_context(format_);
            format_ = nullptr;
            return false;
        }
    }
    return true;
}

bool Mp4Muxer::addVideoStream(const uint8_t *configAnnexB, size_t configSize,
                              int width, int height, int fps) {
    if (format_ == nullptr) return false;
    std::vector<uint8_t> avcc;
    if (!buildAvcC(configAnnexB, configSize, &avcc)) {
        return false;
    }
    AVStream *stream = avformat_new_stream(format_, nullptr);
    if (stream == nullptr) return false;
    AVCodecParameters *par = stream->codecpar;
    par->codec_type = AVMEDIA_TYPE_VIDEO;
    par->codec_id = AV_CODEC_ID_H264;
    par->width = width & ~1;
    par->height = height & ~1;
    par->format = AV_PIX_FMT_YUV420P;
    par->extradata = static_cast<uint8_t *>(av_mallocz(avcc.size() + AV_INPUT_BUFFER_PADDING_SIZE));
    if (par->extradata == nullptr) return false;
    std::memcpy(par->extradata, avcc.data(), avcc.size());
    par->extradata_size = static_cast<int>(avcc.size());
    stream->time_base = videoTimeBase_; // microseconds; muxer rescales on write
    if (fps > 0) {
        stream->avg_frame_rate = AVRational{fps, 1};
    }
    videoStreamIndex_ = stream->index;
    return true;
}

bool Mp4Muxer::addAudioStream(const AVCodecContext *audioEncoder) {
    if (format_ == nullptr || audioEncoder == nullptr) return false;
    AVStream *stream = avformat_new_stream(format_, nullptr);
    if (stream == nullptr) return false;
    if (avcodec_parameters_from_context(stream->codecpar, audioEncoder) < 0) {
        return false;
    }
    stream->time_base = audioEncoder->time_base;
    audioStreamIndex_ = stream->index;
    return true;
}

bool Mp4Muxer::writeHeader() {
    if (format_ == nullptr || videoStreamIndex_ < 0) return false;
    if (avformat_write_header(format_, nullptr) < 0) {
        LOGE("avformat_write_header failed");
        return false;
    }
    headerWritten_ = true;
    return true;
}

bool Mp4Muxer::writeVideoSample(const H264Encoder::EncodedSample &sample) {
    if (!headerWritten_ || videoStreamIndex_ < 0) return false;
    annexBToAvcc(sample.data, sample.size, &avccBuf_);
    if (avccBuf_.empty()) return true; // nothing to write (e.g. config-only)

    AVPacket *pkt = av_packet_alloc();
    if (pkt == nullptr) return false;
    if (av_new_packet(pkt, static_cast<int>(avccBuf_.size())) < 0) {
        av_packet_free(&pkt);
        return false;
    }
    std::memcpy(pkt->data, avccBuf_.data(), avccBuf_.size());
    pkt->stream_index = videoStreamIndex_;
    AVStream *stream = format_->streams[videoStreamIndex_];
    pkt->pts = av_rescale_q(sample.ptsUs, videoTimeBase_, stream->time_base);
    pkt->dts = pkt->pts; // MediaCodec output is in decode order; no B-frames requested
    pkt->duration = 0;
    if (sample.keyframe) {
        pkt->flags |= AV_PKT_FLAG_KEY;
    }
    const int r = av_interleaved_write_frame(format_, pkt);
    av_packet_free(&pkt);
    if (r < 0) {
        LOGE("write video frame failed: %d", r);
        return false;
    }
    return true;
}

bool Mp4Muxer::writeAudioPacket(AVPacket *pkt, AVRational srcTimeBase) {
    if (!headerWritten_ || audioStreamIndex_ < 0 || pkt == nullptr) return false;
    AVStream *stream = format_->streams[audioStreamIndex_];
    AVPacket *copy = av_packet_alloc();
    if (copy == nullptr) return false;
    if (av_packet_ref(copy, pkt) < 0) {
        av_packet_free(&copy);
        return false;
    }
    copy->stream_index = audioStreamIndex_;
    av_packet_rescale_ts(copy, srcTimeBase, stream->time_base);
    const int r = av_interleaved_write_frame(format_, copy);
    av_packet_free(&copy);
    if (r < 0) {
        LOGE("write audio frame failed: %d", r);
        return false;
    }
    return true;
}

bool Mp4Muxer::finish() {
    if (format_ == nullptr || !headerWritten_ || trailerWritten_) {
        return false;
    }
    const bool ok = av_write_trailer(format_) == 0;
    trailerWritten_ = true;
    if (!ok) {
        LOGE("av_write_trailer failed");
    }
    return ok;
}

void Mp4Muxer::closeAll() {
    if (format_ != nullptr) {
        if (!(format_->oformat->flags & AVFMT_NOFILE) && format_->pb != nullptr) {
            avio_closep(&format_->pb);
        }
        avformat_free_context(format_);
        format_ = nullptr;
    }
}
