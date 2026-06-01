#include "ffmpegprobe.h"

#ifdef USE_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
}

#include <QByteArray>

#include <cmath>
#include <memory>
#endif

namespace {
#ifdef USE_FFMPEG
void closeInput(AVFormatContext* context)
{
    avformat_close_input(&context);
}

QString ffmpegErrorString(int errorCode)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    if (av_strerror(errorCode, buffer, sizeof(buffer)) < 0) {
        return QStringLiteral("FFmpeg error %1").arg(errorCode);
    }
    return QString::fromUtf8(buffer);
}

QString codecName(AVCodecID codecId)
{
    const AVCodecDescriptor* descriptor = avcodec_descriptor_get(codecId);
    if (descriptor != nullptr) {
        if (descriptor->long_name != nullptr) {
            return QString::fromUtf8(descriptor->long_name);
        }
        if (descriptor->name != nullptr) {
            return QString::fromUtf8(descriptor->name);
        }
    }
    return QString::fromUtf8(avcodec_get_name(codecId));
}

double frameRateToDouble(AVRational rational)
{
    if (rational.num <= 0 || rational.den <= 0) {
        return 0.0;
    }

    const double value = av_q2d(rational);
    if (!std::isfinite(value) || value <= 0.0) {
        return 0.0;
    }
    return value;
}

double streamFps(const AVStream* stream)
{
    double fps = frameRateToDouble(stream->avg_frame_rate);
    if (fps <= 0.0) {
        fps = frameRateToDouble(stream->r_frame_rate);
    }
    return fps;
}

bool isAttachedPictureStream(const AVStream* stream)
{
    return (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) != 0;
}

void fillVideoInfo(FFmpegMediaInfo& info, const AVStream* stream, const AVCodecParameters* codecParameters)
{
    info.hasVideo = true;
    info.videoCodec = codecName(codecParameters->codec_id);
    info.width = codecParameters->width;
    info.height = codecParameters->height;
    info.fps = streamFps(stream);
}

void fillAudioInfo(FFmpegMediaInfo& info, const AVCodecParameters* codecParameters)
{
    info.hasAudio = true;
    info.audioCodec = codecName(codecParameters->codec_id);
    info.sampleRate = codecParameters->sample_rate;
    info.channels = codecParameters->ch_layout.nb_channels;
}

qint64 streamBitRateSum(const AVFormatContext* formatContext)
{
    qint64 bitRate = 0;
    for (unsigned int i = 0; i < formatContext->nb_streams; ++i) {
        const AVStream* stream = formatContext->streams[i];
        if (stream == nullptr || stream->codecpar == nullptr) {
            continue;
        }
        if (stream->codecpar->bit_rate > 0) {
            bitRate += stream->codecpar->bit_rate;
        }
    }
    return bitRate;
}

void fillStreamInfo(FFmpegMediaInfo& info, const AVFormatContext* formatContext)
{
    for (unsigned int i = 0; i < formatContext->nb_streams; ++i) {
        const AVStream* stream = formatContext->streams[i];
        if (stream == nullptr || stream->codecpar == nullptr) {
            continue;
        }

        const AVCodecParameters* codecParameters = stream->codecpar;
        switch (codecParameters->codec_type) {
        case AVMEDIA_TYPE_VIDEO:
            if (!info.hasVideo && !isAttachedPictureStream(stream)) {
                fillVideoInfo(info, stream, codecParameters);
            }
            break;
        case AVMEDIA_TYPE_AUDIO:
            if (!info.hasAudio) {
                fillAudioInfo(info, codecParameters);
            }
            break;
        default:
            break;
        }
    }
}
#endif
}

FFmpegMediaInfo FFmpegProbe::probeFile(const QString& filePath)
{
    FFmpegMediaInfo info;
    info.filePath = filePath;

#ifndef USE_FFMPEG
    info.errorMessage = QStringLiteral("当前未启用 FFmpeg 支持");
    return info;
#else
    AVFormatContext* rawContext = nullptr;
    const QByteArray encodedPath = filePath.toUtf8();

    int result = avformat_open_input(&rawContext, encodedPath.constData(), nullptr, nullptr);
    if (result < 0) {
        info.errorMessage = ffmpegErrorString(result);
        avformat_close_input(&rawContext);
        return info;
    }
    std::unique_ptr<AVFormatContext, decltype(&closeInput)> formatContext(rawContext, closeInput);

    result = avformat_find_stream_info(formatContext.get(), nullptr);
    if (result < 0) {
        info.errorMessage = ffmpegErrorString(result);
        return info;
    }

    if (formatContext->iformat != nullptr) {
        const char* formatName = formatContext->iformat->long_name != nullptr
            ? formatContext->iformat->long_name
            : formatContext->iformat->name;
        if (formatName != nullptr) {
            info.formatName = QString::fromUtf8(formatName);
        }
    }

    if (formatContext->duration != AV_NOPTS_VALUE && formatContext->duration > 0) {
        info.durationMs = av_rescale(formatContext->duration, 1000, AV_TIME_BASE);
    }
    info.bitRate = formatContext->bit_rate > 0
        ? formatContext->bit_rate
        : streamBitRateSum(formatContext.get());

    fillStreamInfo(info, formatContext.get());
    if (!info.hasVideo && !info.hasAudio) {
        info.errorMessage = QStringLiteral("FFmpeg did not find an audio or video stream");
        return info;
    }

    info.valid = true;
    return info;
#endif
}
