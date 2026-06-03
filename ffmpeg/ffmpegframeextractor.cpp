#include "ffmpegframeextractor.h"

#ifdef USE_FFMPEG

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <QByteArray>
#include <QFile>
#include <QFileInfo>

#include <memory>

namespace {

void closeInput(AVFormatContext* context)
{
    avformat_close_input(&context);
}

void freeCodecContext(AVCodecContext* context)
{
    avcodec_free_context(&context);
}

void freePacket(AVPacket* packet)
{
    av_packet_free(&packet);
}

void freeFrame(AVFrame* frame)
{
    av_frame_free(&frame);
}

void freeSwsContext(SwsContext* context)
{
    sws_freeContext(context);
}

QString ffmpegErrorString(int errorCode)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    if (av_strerror(errorCode, buffer, sizeof(buffer)) < 0) {
        return QStringLiteral("FFmpeg error %1").arg(errorCode);
    }
    return QString::fromUtf8(buffer);
}

bool fail(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    return false;
}

bool validateLocalVideoFile(const QString& filePath, QString* errorMessage)
{
    const QString normalizedPath = filePath.trimmed();
    if (normalizedPath.isEmpty()) {
        return fail(errorMessage, QStringLiteral("文件路径为空，无法提取视频帧"));
    }

    const QFileInfo fileInfo(normalizedPath);
    if (!fileInfo.exists()) {
        return fail(errorMessage, QStringLiteral("文件不存在：%1").arg(fileInfo.absoluteFilePath()));
    }
    if (!fileInfo.isFile()) {
        return fail(errorMessage, QStringLiteral("不是有效的本地视频文件：%1").arg(fileInfo.absoluteFilePath()));
    }

    QFile file(fileInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(errorMessage, QStringLiteral("文件不可读，可能没有访问权限：%1").arg(fileInfo.absoluteFilePath()));
    }
    file.close();

    if (fileInfo.size() <= 0) {
        return fail(errorMessage, QStringLiteral("文件为空，无法提取视频帧：%1").arg(fileInfo.absoluteFilePath()));
    }

    return true;
}

bool isUsableVideoStream(const AVStream* stream)
{
    return stream != nullptr
        && stream->codecpar != nullptr
        && stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO
        && (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) == 0;
}

int findVideoStreamIndex(const AVFormatContext* formatContext)
{
    for (unsigned int i = 0; i < formatContext->nb_streams; ++i) {
        if (isUsableVideoStream(formatContext->streams[i])) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool convertFrameToImage(const AVFrame* frame, QImage& outImage, QString* errorMessage)
{
    if (frame->width <= 0 || frame->height <= 0) {
        return fail(errorMessage, QStringLiteral("Invalid decoded video frame size"));
    }

    const AVPixelFormat sourceFormat = static_cast<AVPixelFormat>(frame->format);
    if (sourceFormat == AV_PIX_FMT_NONE) {
        return fail(errorMessage, QStringLiteral("Invalid decoded video pixel format"));
    }

    std::unique_ptr<SwsContext, decltype(&freeSwsContext)> swsContext(
        sws_getContext(frame->width,
                       frame->height,
                       sourceFormat,
                       frame->width,
                       frame->height,
                       AV_PIX_FMT_RGB24,
                       SWS_BILINEAR,
                       nullptr,
                       nullptr,
                       nullptr),
        freeSwsContext);
    if (!swsContext) {
        return fail(errorMessage, QStringLiteral("Failed to create FFmpeg scaling context"));
    }

    QImage image(frame->width, frame->height, QImage::Format_RGB888);
    if (image.isNull()) {
        return fail(errorMessage, QStringLiteral("Failed to allocate output image"));
    }

    uint8_t* destinationData[] = { image.bits(), nullptr, nullptr, nullptr };
    int destinationLineSize[] = { static_cast<int>(image.bytesPerLine()), 0, 0, 0 };
    const int scaledHeight = sws_scale(swsContext.get(),
                                       frame->data,
                                       frame->linesize,
                                       0,
                                       frame->height,
                                       destinationData,
                                       destinationLineSize);
    if (scaledHeight <= 0) {
        return fail(errorMessage, QStringLiteral("Failed to convert video frame to RGB image"));
    }

    outImage = image;
    return true;
}

bool receiveFrame(AVCodecContext* codecContext,
                  qint64 targetTimestamp,
                  AVFrame* frame,
                  QImage& outImage,
                  QString* errorMessage)
{
    while (true) {
        const int result = avcodec_receive_frame(codecContext, frame);
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
            return false;
        }
        if (result < 0) {
            fail(errorMessage, QStringLiteral("FFmpeg 解码视频帧失败：%1").arg(ffmpegErrorString(result)));
            return true;
        }

        const qint64 frameTimestamp = frame->best_effort_timestamp;
        const bool timestampUnavailable = frameTimestamp == AV_NOPTS_VALUE;
        const bool reachedTarget = timestampUnavailable
            || targetTimestamp <= 0
            || frameTimestamp >= targetTimestamp;
        if (reachedTarget) {
            convertFrameToImage(frame, outImage, errorMessage);
            av_frame_unref(frame);
            return true;
        }
        av_frame_unref(frame);
    }
}

} // namespace

bool FFmpegFrameExtractor::extractFrame(const QString& filePath,
                                        qint64 positionMs,
                                        QImage& outImage,
                                        QString* errorMessage)
{
    outImage = QImage();
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }

    if (!validateLocalVideoFile(filePath, errorMessage)) {
        return false;
    }

    AVFormatContext* rawFormatContext = nullptr;
    const QByteArray encodedPath = QFileInfo(filePath.trimmed()).absoluteFilePath().toUtf8();

    int result = avformat_open_input(&rawFormatContext, encodedPath.constData(), nullptr, nullptr);
    if (result < 0) {
        avformat_close_input(&rawFormatContext);
        return fail(errorMessage, QStringLiteral("FFmpeg 打开媒体文件失败：%1").arg(ffmpegErrorString(result)));
    }

    std::unique_ptr<AVFormatContext, decltype(&closeInput)> formatContext(rawFormatContext, closeInput);
    result = avformat_find_stream_info(formatContext.get(), nullptr);
    if (result < 0) {
        return fail(errorMessage, QStringLiteral("FFmpeg 读取流信息失败：%1").arg(ffmpegErrorString(result)));
    }

    const int videoStreamIndex = findVideoStreamIndex(formatContext.get());
    if (videoStreamIndex < 0) {
        return fail(errorMessage, QStringLiteral("FFmpeg 未发现可用的视频流"));
    }

    AVStream* videoStream = formatContext->streams[videoStreamIndex];
    const AVCodecParameters* codecParameters = videoStream->codecpar;
    const AVCodec* decoder = avcodec_find_decoder(codecParameters->codec_id);
    if (decoder == nullptr) {
        return fail(errorMessage, QStringLiteral("FFmpeg 未找到对应的视频解码器"));
    }

    std::unique_ptr<AVCodecContext, decltype(&freeCodecContext)> codecContext(
        avcodec_alloc_context3(decoder),
        freeCodecContext);
    if (!codecContext) {
        return fail(errorMessage, QStringLiteral("FFmpeg 解码器上下文创建失败"));
    }

    result = avcodec_parameters_to_context(codecContext.get(), codecParameters);
    if (result < 0) {
        return fail(errorMessage, QStringLiteral("FFmpeg 配置解码器失败：%1").arg(ffmpegErrorString(result)));
    }

    result = avcodec_open2(codecContext.get(), decoder, nullptr);
    if (result < 0) {
        return fail(errorMessage, QStringLiteral("FFmpeg 打开视频解码器失败：%1").arg(ffmpegErrorString(result)));
    }

    const qint64 clampedPositionMs = qMax<qint64>(0, positionMs);
    const qint64 targetTimestamp = av_rescale_q(clampedPositionMs,
                                                AVRational { 1, 1000 },
                                                videoStream->time_base);
    result = av_seek_frame(formatContext.get(), videoStreamIndex, targetTimestamp, AVSEEK_FLAG_BACKWARD);
    if (result < 0) {
        return fail(errorMessage, QStringLiteral("FFmpeg 定位视频时间点失败：%1").arg(ffmpegErrorString(result)));
    }
    avcodec_flush_buffers(codecContext.get());

    std::unique_ptr<AVPacket, decltype(&freePacket)> packet(av_packet_alloc(), freePacket);
    std::unique_ptr<AVFrame, decltype(&freeFrame)> frame(av_frame_alloc(), freeFrame);
    if (!packet || !frame) {
        return fail(errorMessage, QStringLiteral("FFmpeg 数据包或帧缓存创建失败"));
    }

    while ((result = av_read_frame(formatContext.get(), packet.get())) >= 0) {
        if (packet->stream_index != videoStreamIndex) {
            av_packet_unref(packet.get());
            continue;
        }

        result = avcodec_send_packet(codecContext.get(), packet.get());
        av_packet_unref(packet.get());
        if (result < 0) {
            return fail(errorMessage, QStringLiteral("FFmpeg 发送视频包到解码器失败：%1").arg(ffmpegErrorString(result)));
        }

        if (receiveFrame(codecContext.get(), targetTimestamp, frame.get(), outImage, errorMessage)) {
            return !outImage.isNull();
        }
    }

    if (result != AVERROR_EOF) {
        return fail(errorMessage, QStringLiteral("FFmpeg 读取视频包失败：%1").arg(ffmpegErrorString(result)));
    }

    result = avcodec_send_packet(codecContext.get(), nullptr);
    if (result < 0) {
        return fail(errorMessage, QStringLiteral("FFmpeg 刷新视频解码器失败：%1").arg(ffmpegErrorString(result)));
    }

    if (receiveFrame(codecContext.get(), targetTimestamp, frame.get(), outImage, errorMessage)) {
        return !outImage.isNull();
    }

    return fail(errorMessage, QStringLiteral("FFmpeg 未能在指定时间点附近解码出视频帧"));
}

QImage FFmpegFrameExtractor::extractFrame(const QString& filePath, qint64 positionMs)
{
    QImage image;
    m_lastError.clear();
    if (!extractFrame(filePath, positionMs, image, &m_lastError)) {
        return QImage();
    }
    return image;
}

QString FFmpegFrameExtractor::lastError() const
{
    return m_lastError;
}

#endif // USE_FFMPEG

#ifndef USE_FFMPEG
bool FFmpegFrameExtractor::extractFrame(const QString& filePath,
                                        qint64 positionMs,
                                        QImage& outImage,
                                        QString* errorMessage)
{
    Q_UNUSED(filePath)
    Q_UNUSED(positionMs)
    outImage = QImage();
    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("当前未启用 FFmpeg，无法从视频文件提取帧");
    }
    return false;
}

QImage FFmpegFrameExtractor::extractFrame(const QString& filePath, qint64 positionMs)
{
    QImage image;
    extractFrame(filePath, positionMs, image, &m_lastError);
    return image;
}

QString FFmpegFrameExtractor::lastError() const
{
    return m_lastError;
}
#endif // USE_FFMPEG
