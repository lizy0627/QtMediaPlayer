#include "ffmpegprobe.h"

#ifdef USE_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
#include <libavutil/version.h>
}

#include <QByteArray>
#include <QFile>
#include <QFileInfo>

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

void setError(FFmpegMediaInfo& info, const QString& message)
{
    info.valid = false;
    info.errorMessage = message;
}

bool validateLocalFile(FFmpegMediaInfo& info, const QString& filePath)
{
    const QString normalizedPath = filePath.trimmed();
    if (normalizedPath.isEmpty()) {
        setError(info, QStringLiteral("文件路径为空，无法进行 FFmpeg 探测"));
        return false;
    }

    const QFileInfo fileInfo(normalizedPath);
    info.filePath = fileInfo.absoluteFilePath();

    if (!fileInfo.exists()) {
        setError(info, QStringLiteral("文件不存在：%1").arg(info.filePath));
        return false;
    }
    if (!fileInfo.isFile()) {
        setError(info, QStringLiteral("不是有效的本地媒体文件：%1").arg(info.filePath));
        return false;
    }

    QFile file(fileInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        setError(info, QStringLiteral("文件不可读，可能没有访问权限：%1").arg(info.filePath));
        return false;
    }
    file.close();

    if (fileInfo.size() <= 0) {
        setError(info, QStringLiteral("文件为空，无法读取媒体信息：%1").arg(info.filePath));
        return false;
    }

    return true;
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

qint64 streamDurationMs(const AVStream* stream)
{
    if (stream == nullptr || stream->duration == AV_NOPTS_VALUE || stream->duration <= 0) {
        return 0;
    }
    return av_rescale_q(stream->duration, stream->time_base, AVRational { 1, 1000 });
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
#if LIBAVUTIL_VERSION_MAJOR >= 57
    info.channels = codecParameters->ch_layout.nb_channels;
#else
    info.channels = codecParameters->channels;
#endif
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

qint64 estimatedBitRateFromFileSize(qint64 fileSize, qint64 durationMs)
{
    if (fileSize <= 0 || durationMs <= 0) {
        return 0;
    }
    return fileSize * 8 * 1000 / durationMs;
}

QString formatDescription(const AVInputFormat* inputFormat)
{
    if (inputFormat == nullptr) {
        return QString();
    }

    const QString name = inputFormat->name != nullptr
        ? QString::fromUtf8(inputFormat->name)
        : QString();
    const QString longName = inputFormat->long_name != nullptr
        ? QString::fromUtf8(inputFormat->long_name)
        : QString();

    if (!longName.isEmpty() && !name.isEmpty() && longName != name) {
        return QStringLiteral("%1 (%2)").arg(longName, name);
    }
    return longName.isEmpty() ? name : longName;
}

void fillStreamInfo(FFmpegMediaInfo& info, const AVFormatContext* formatContext)
{
    qint64 maxStreamDurationMs = 0;

    for (unsigned int i = 0; i < formatContext->nb_streams; ++i) {
        const AVStream* stream = formatContext->streams[i];
        if (stream == nullptr || stream->codecpar == nullptr) {
            continue;
        }

        maxStreamDurationMs = qMax(maxStreamDurationMs, streamDurationMs(stream));

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

    if (info.durationMs <= 0) {
        info.durationMs = maxStreamDurationMs;
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
    if (!validateLocalFile(info, filePath)) {
        return info;
    }

    AVFormatContext* rawContext = nullptr;
    const QByteArray encodedPath = info.filePath.toUtf8();

    int result = avformat_open_input(&rawContext, encodedPath.constData(), nullptr, nullptr);
    if (result < 0) {
        setError(info, QStringLiteral("FFmpeg open input failed: %1").arg(ffmpegErrorString(result)));
        avformat_close_input(&rawContext);
        return info;
    }
    std::unique_ptr<AVFormatContext, decltype(&closeInput)> formatContext(rawContext, closeInput);

    result = avformat_find_stream_info(formatContext.get(), nullptr);
    if (result < 0) {
        setError(info, QStringLiteral("FFmpeg stream info failed: %1").arg(ffmpegErrorString(result)));
        return info;
    }

    info.formatName = formatDescription(formatContext->iformat);

    if (formatContext->duration != AV_NOPTS_VALUE && formatContext->duration > 0) {
        info.durationMs = av_rescale(formatContext->duration, 1000, AV_TIME_BASE);
    }

    fillStreamInfo(info, formatContext.get());
    info.bitRate = formatContext->bit_rate > 0
        ? formatContext->bit_rate
        : streamBitRateSum(formatContext.get());
    if (info.bitRate <= 0) {
        info.bitRate = estimatedBitRateFromFileSize(QFileInfo(info.filePath).size(), info.durationMs);
    }

    if (!info.hasVideo && !info.hasAudio) {
        setError(info, QStringLiteral("NO_PLAYABLE_AUDIO_VIDEO_STREAM"));
        return info;
    }

    info.valid = true;
    info.errorMessage.clear();
    return info;
#endif
}
