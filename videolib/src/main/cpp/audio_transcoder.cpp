#include "audio_transcoder.h"

#include <android/log.h>

#include <algorithm>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
}

#define LOG_TAG "videolib.export.audio"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {
    constexpr AVRational kMicrosecondTimeBase{1, 1000000};
    constexpr int kAacBitrate = 128000;

    int64_t clampMsToUs(int64_t ms) {
        if (ms <= 0) return 0;
        return ms > std::numeric_limits<int64_t>::max() / 1000
               ? std::numeric_limits<int64_t>::max()
               : ms * 1000;
    }

    // Factor a tempo into a chain of atempo values each in [0.5, 2.0], since a
    // single atempo instance only accepts that range. Our playback speed floor
    // is 0.1, so up to three 0.5 factors may be needed.
    std::string buildAtempoChain(double tempo) {
        if (tempo <= 0.0) tempo = 1.0;
        std::vector<double> factors;
        while (tempo < 0.5) {
            factors.push_back(0.5);
            tempo /= 0.5;
        }
        while (tempo > 2.0) {
            factors.push_back(2.0);
            tempo /= 2.0;
        }
        factors.push_back(tempo);
        std::ostringstream chain;
        for (size_t i = 0; i < factors.size(); ++i) {
            if (i > 0) chain << ",";
            chain << "atempo=" << factors[i];
        }
        return chain.str();
    }
}

AudioTranscoder::AudioTranscoder(const std::atomic<bool> *cancel) : cancel_(cancel) {}

AudioTranscoder::~AudioTranscoder() {
    av_frame_free(&decodeFrame_);
    av_frame_free(&filterFrame_);
    av_packet_free(&packet_);
    avfilter_graph_free(&filterGraph_);
    avcodec_free_context(&decoder_);
    avcodec_free_context(&encoder_);
    if (format_ != nullptr) {
        avformat_close_input(&format_);
    }
}

bool AudioTranscoder::cancelled() const {
    return cancel_ != nullptr && cancel_->load(std::memory_order_acquire);
}

std::optional<ExportErrorCode> AudioTranscoder::open(const std::string &path, double speed) {
    if (avformat_open_input(&format_, path.c_str(), nullptr, nullptr) < 0) {
        return ExportErrorCode::InputOpen;
    }
    if (avformat_find_stream_info(format_, nullptr) < 0) {
        return ExportErrorCode::Decode;
    }
    audioStreamIndex_ = av_find_best_stream(format_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audioStreamIndex_ < 0) {
        // No audio stream: video-only export (A-2). Not an error.
        hasAudio_ = false;
        return std::nullopt;
    }

    AVStream *stream = format_->streams[audioStreamIndex_];
    const AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (dec == nullptr) {
        // Unsupported audio: fall back to video-only rather than failing export.
        LOGE("no decoder for source audio; exporting video-only");
        hasAudio_ = false;
        audioStreamIndex_ = -1;
        return std::nullopt;
    }
    decoder_ = avcodec_alloc_context3(dec);
    if (decoder_ == nullptr ||
        avcodec_parameters_to_context(decoder_, stream->codecpar) < 0 ||
        avcodec_open2(decoder_, dec, nullptr) < 0) {
        return ExportErrorCode::Decode;
    }
    if (decoder_->ch_layout.nb_channels <= 0 || decoder_->sample_rate <= 0) {
        LOGE("invalid source audio params; exporting video-only");
        hasAudio_ = false;
        audioStreamIndex_ = -1;
        return std::nullopt;
    }

    // AAC encoder configured to match the source rate/channels (FLTP).
    const AVCodec *enc = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (enc == nullptr) {
        return ExportErrorCode::Encode;
    }
    encoder_ = avcodec_alloc_context3(enc);
    if (encoder_ == nullptr) {
        return ExportErrorCode::Encode;
    }
    encoder_->sample_fmt = AV_SAMPLE_FMT_FLTP;
    encoder_->sample_rate = decoder_->sample_rate;
    encoder_->bit_rate = kAacBitrate;
    if (av_channel_layout_copy(&encoder_->ch_layout, &decoder_->ch_layout) < 0) {
        return ExportErrorCode::Encode;
    }
    encoder_->time_base = AVRational{1, encoder_->sample_rate};
    if (avcodec_open2(encoder_, enc, nullptr) < 0) {
        return ExportErrorCode::Encode;
    }

    packet_ = av_packet_alloc();
    decodeFrame_ = av_frame_alloc();
    filterFrame_ = av_frame_alloc();
    if (packet_ == nullptr || decodeFrame_ == nullptr || filterFrame_ == nullptr) {
        return ExportErrorCode::Encode;
    }
    if (auto err = initFilter(speed)) {
        return err;
    }
    hasAudio_ = true;
    return std::nullopt;
}

std::optional<ExportErrorCode> AudioTranscoder::initFilter(double speed) {
    filterGraph_ = avfilter_graph_alloc();
    if (filterGraph_ == nullptr) {
        return ExportErrorCode::Encode;
    }
    char chLayout[256];
    av_channel_layout_describe(&decoder_->ch_layout, chLayout, sizeof(chLayout));

    std::ostringstream args;
    args << "time_base=1/" << decoder_->sample_rate
         << ":sample_rate=" << decoder_->sample_rate
         << ":sample_fmt=" << av_get_sample_fmt_name(decoder_->sample_fmt)
         << ":channel_layout=" << chLayout;

    const AVFilter *abuffer = avfilter_get_by_name("abuffer");
    const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
    if (abuffer == nullptr || abuffersink == nullptr) {
        return ExportErrorCode::Encode;
    }
    if (avfilter_graph_create_filter(&bufferSrc_, abuffer, "in",
                                     args.str().c_str(), nullptr, filterGraph_) < 0) {
        return ExportErrorCode::Encode;
    }
    if (avfilter_graph_create_filter(&bufferSink_, abuffersink, "out",
                                     nullptr, nullptr, filterGraph_) < 0) {
        return ExportErrorCode::Encode;
    }
    // Constrain the sink to the encoder's format so aformat is inserted.
    const enum AVSampleFormat outFmts[] = {encoder_->sample_fmt, AV_SAMPLE_FMT_NONE};
    if (av_opt_set_int_list(bufferSink_, "sample_fmts", outFmts, AV_SAMPLE_FMT_NONE,
                            AV_OPT_SEARCH_CHILDREN) < 0) {
        return ExportErrorCode::Encode;
    }
    const int outRates[] = {encoder_->sample_rate, -1};
    if (av_opt_set_int_list(bufferSink_, "sample_rates", outRates, -1,
                            AV_OPT_SEARCH_CHILDREN) < 0) {
        return ExportErrorCode::Encode;
    }

    // Graph: in -> atempo chain -> aformat(encoder) -> out
    char encLayout[256];
    av_channel_layout_describe(&encoder_->ch_layout, encLayout, sizeof(encLayout));
    std::ostringstream desc;
    desc << buildAtempoChain(speed)
         << ",aformat=sample_fmts=" << av_get_sample_fmt_name(encoder_->sample_fmt)
         << ":sample_rates=" << encoder_->sample_rate
         << ":channel_layouts=" << encLayout;

    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();
    if (outputs == nullptr || inputs == nullptr) {
        avfilter_inout_free(&outputs);
        avfilter_inout_free(&inputs);
        return ExportErrorCode::Encode;
    }
    outputs->name = av_strdup("in");
    outputs->filter_ctx = bufferSrc_;
    outputs->pad_idx = 0;
    outputs->next = nullptr;
    inputs->name = av_strdup("out");
    inputs->filter_ctx = bufferSink_;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    int rc = avfilter_graph_parse_ptr(filterGraph_, desc.str().c_str(), &inputs, &outputs, nullptr);
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    if (rc < 0 || avfilter_graph_config(filterGraph_, nullptr) < 0) {
        LOGE("audio filter graph config failed");
        return ExportErrorCode::Encode;
    }
    // Deliver encoder-sized frames from the sink when the encoder needs it.
    if (encoder_->frame_size > 0) {
        av_buffersink_set_frame_size(bufferSink_, encoder_->frame_size);
    }
    return std::nullopt;
}

std::optional<ExportErrorCode> AudioTranscoder::encodeAndSink(
        AVFrame *frame, int64_t ptsBaseUs, const PacketSink &sink, bool *aborted) {
    // frame may be null (flush). Assign a monotonic PTS in encoder time base.
    if (frame != nullptr) {
        frame->pts = nextPts_;
        nextPts_ += frame->nb_samples;
    }
    if (avcodec_send_frame(encoder_, frame) < 0) {
        return ExportErrorCode::Encode;
    }
    while (true) {
        const int r = avcodec_receive_packet(encoder_, packet_);
        if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) {
            return std::nullopt;
        }
        if (r < 0) {
            return ExportErrorCode::Encode;
        }
        // Offset by the timeline base (convert base us -> encoder samples).
        const int64_t baseSamples = av_rescale_q(ptsBaseUs, kMicrosecondTimeBase,
                                                 encoder_->time_base);
        packet_->pts += baseSamples;
        packet_->dts = packet_->pts;
        if (!sink(packet_, encoder_->time_base)) {
            *aborted = true;
            av_packet_unref(packet_);
            return ExportErrorCode::Mux;
        }
        av_packet_unref(packet_);
    }
}

std::optional<ExportErrorCode> AudioTranscoder::transcode(
        int64_t startMs, int64_t endMs, int64_t ptsBaseUs, const PacketSink &sink) {
    if (!hasAudio_) {
        return std::nullopt; // video-only: nothing to do
    }
    AVStream *stream = format_->streams[audioStreamIndex_];
    const int64_t startUs = clampMsToUs(startMs);
    const int64_t endUs = endMs >= 0 ? clampMsToUs(endMs) : -1;
    nextPts_ = 0;

    if (startUs > 0) {
        int64_t target = av_rescale_q(startUs, kMicrosecondTimeBase, stream->time_base);
        if (stream->start_time != AV_NOPTS_VALUE) {
            target += stream->start_time;
        }
        av_seek_frame(format_, audioStreamIndex_, target, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(decoder_);
    }

    bool aborted = false;
    auto pumpFilter = [&]() -> std::optional<ExportErrorCode> {
        while (true) {
            const int r = av_buffersink_get_frame(bufferSink_, filterFrame_);
            if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) {
                return std::nullopt;
            }
            if (r < 0) {
                return ExportErrorCode::Encode;
            }
            auto err = encodeAndSink(filterFrame_, ptsBaseUs, sink, &aborted);
            av_frame_unref(filterFrame_);
            if (err.has_value()) {
                return err;
            }
        }
    };

    const int64_t timelineStartUs = stream->start_time != AV_NOPTS_VALUE
                                    ? av_rescale_q(stream->start_time, stream->time_base,
                                                   kMicrosecondTimeBase)
                                    : 0;

    while (!cancelled() && !aborted) {
        const int readResult = av_read_frame(format_, packet_);
        if (readResult < 0) {
            break; // EOF or error -> flush below
        }
        if (packet_->stream_index != audioStreamIndex_) {
            av_packet_unref(packet_);
            continue;
        }
        // Stop feeding once past B (exclusive).
        if (endUs >= 0 && packet_->pts != AV_NOPTS_VALUE) {
            const int64_t ptsUs = av_rescale_q(packet_->pts, stream->time_base,
                                               kMicrosecondTimeBase) - timelineStartUs;
            if (ptsUs >= endUs) {
                av_packet_unref(packet_);
                break;
            }
        }
        const int sendResult = avcodec_send_packet(decoder_, packet_);
        av_packet_unref(packet_);
        if (sendResult < 0) {
            return ExportErrorCode::Decode;
        }
        while (true) {
            const int r = avcodec_receive_frame(decoder_, decodeFrame_);
            if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) break;
            if (r < 0) return ExportErrorCode::Decode;
            if (av_buffersrc_add_frame_flags(bufferSrc_, decodeFrame_,
                                             AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
                av_frame_unref(decodeFrame_);
                return ExportErrorCode::Encode;
            }
            av_frame_unref(decodeFrame_);
            if (auto err = pumpFilter()) return err;
        }
    }
    if (cancelled()) {
        return ExportErrorCode::Decode;
    }
    // Flush decoder -> filter -> encoder.
    avcodec_send_packet(decoder_, nullptr);
    while (true) {
        const int r = avcodec_receive_frame(decoder_, decodeFrame_);
        if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) break;
        if (r < 0) break;
        (void) av_buffersrc_add_frame_flags(bufferSrc_, decodeFrame_,
                                            AV_BUFFERSRC_FLAG_KEEP_REF);
        av_frame_unref(decodeFrame_);
        if (auto err = pumpFilter()) return err;
    }
    (void) av_buffersrc_add_frame(bufferSrc_, nullptr); // signal EOF to the filter graph
    if (auto err = pumpFilter()) return err;
    // Flush encoder.
    if (auto err = encodeAndSink(nullptr, ptsBaseUs, sink, &aborted)) {
        return err;
    }
    return std::nullopt;
}
