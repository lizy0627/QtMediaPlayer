#include "ffmpegplaybackbackend.h"

#ifdef USE_FFMPEG

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QMediaDevices>
#include <QUrl>
#include <QVideoWidget>
#include <QtGlobal>

#include "ffmpegvideowidget.h"

#include <chrono>
#include <cmath>
#include <memory>

namespace {

constexpr qint64 kVideoEarlyToleranceMs = 10;
constexpr qint64 kVideoDropThresholdMs = 120;
constexpr qint64 kVideoSyncWaitSliceMs = 10;

enum class VideoSyncDecision {
    Display,
    Drop,
    Stop
};

QString ffmpegErrorString(int errorCode)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    if (av_strerror(errorCode, buffer, sizeof(buffer)) < 0) {
        return QStringLiteral("FFmpeg error %1").arg(errorCode);
    }
    return QString::fromUtf8(buffer);
}

bool isAttachedPictureStream(const AVStream* stream)
{
    return stream != nullptr
        && (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) != 0;
}

qint64 formatDurationMs(const AVFormatContext* formatContext)
{
    if (formatContext == nullptr
        || formatContext->duration == AV_NOPTS_VALUE
        || formatContext->duration <= 0) {
        return 0;
    }
    return av_rescale(formatContext->duration, 1000, AV_TIME_BASE);
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

void freeSwrContext(SwrContext* context)
{
    swr_free(&context);
}

double rationalToDouble(AVRational rational)
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

int streamFrameDelayMs(const AVStream* stream)
{
    double fps = rationalToDouble(stream->avg_frame_rate);
    if (fps <= 0.0) {
        fps = rationalToDouble(stream->r_frame_rate);
    }
    if (fps <= 0.0) {
        return 33;
    }

    return qMax(1, static_cast<int>(std::lround(1000.0 / fps)));
}

qint64 frameTimestampMs(const AVFrame* frame, const AVStream* stream, qint64 fallbackMs)
{
    if (frame == nullptr || stream == nullptr || frame->best_effort_timestamp == AV_NOPTS_VALUE) {
        return fallbackMs;
    }

    return av_rescale_q(frame->best_effort_timestamp, stream->time_base, AVRational { 1, 1000 });
}

bool preparePcmS16Format(const QAudioDevice& device,
                         int sourceSampleRate,
                         QAudioFormat* format,
                         QString* errorMessage)
{
    if (format == nullptr) {
        return false;
    }

    QAudioFormat desiredFormat;
    desiredFormat.setSampleRate(sourceSampleRate > 0 ? sourceSampleRate : 48000);
    desiredFormat.setChannelCount(2);
    desiredFormat.setSampleFormat(QAudioFormat::Int16);

    if (device.isFormatSupported(desiredFormat)) {
        *format = desiredFormat;
        return true;
    }

    QAudioFormat preferredFormat = device.preferredFormat();
    if (preferredFormat.sampleFormat() == QAudioFormat::Int16
        && preferredFormat.sampleRate() > 0
        && preferredFormat.channelCount() > 0
        && device.isFormatSupported(preferredFormat)) {
        *format = preferredFormat;
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Default audio output does not support PCM S16 playback.");
    }
    return false;
}

bool convertFrameToImage(const AVFrame* frame, SwsContext** swsContext, QImage* image, QString* errorMessage)
{
    if (frame == nullptr || swsContext == nullptr || image == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Invalid FFmpeg frame conversion state.");
        }
        return false;
    }

    if (frame->width <= 0 || frame->height <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Invalid decoded video frame size.");
        }
        return false;
    }

    const AVPixelFormat sourceFormat = static_cast<AVPixelFormat>(frame->format);
    if (sourceFormat == AV_PIX_FMT_NONE) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Invalid decoded video pixel format.");
        }
        return false;
    }

    *swsContext = sws_getCachedContext(*swsContext,
                                       frame->width,
                                       frame->height,
                                       sourceFormat,
                                       frame->width,
                                       frame->height,
                                       AV_PIX_FMT_RGB24,
                                       SWS_BILINEAR,
                                       nullptr,
                                       nullptr,
                                       nullptr);
    if (*swsContext == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to create FFmpeg scaling context.");
        }
        return false;
    }

    QImage convertedImage(frame->width, frame->height, QImage::Format_RGB888);
    if (convertedImage.isNull()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to allocate decoded video image.");
        }
        return false;
    }

    uint8_t* destinationData[] = { convertedImage.bits(), nullptr, nullptr, nullptr };
    int destinationLineSize[] = { static_cast<int>(convertedImage.bytesPerLine()), 0, 0, 0 };
    const int scaledHeight = sws_scale(*swsContext,
                                       frame->data,
                                       frame->linesize,
                                       0,
                                       frame->height,
                                       destinationData,
                                       destinationLineSize);
    if (scaledHeight <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to convert video frame to RGB image.");
        }
        return false;
    }

    *image = convertedImage;
    return true;
}

} // namespace

FFmpegPlaybackBackend::FFmpegPlaybackBackend(QObject* parent)
    : IPlaybackBackend(parent)
{
}

FFmpegPlaybackBackend::~FFmpegPlaybackBackend()
{
    stopDecodeThread();
    closeInput();
}

void FFmpegPlaybackBackend::setVideoOutput(QVideoWidget* videoOutput)
{
    Q_UNUSED(videoOutput)
}

void FFmpegPlaybackBackend::setFrameOutput(FFmpegVideoWidget* frameOutput)
{
    if (m_frameConnection) {
        disconnect(m_frameConnection);
        m_frameConnection = QMetaObject::Connection();
    }

    m_frameOutput = frameOutput;
    if (m_frameOutput != nullptr) {
        m_frameConnection = connect(this,
                                    &FFmpegPlaybackBackend::videoFrameReady,
                                    m_frameOutput,
                                    &FFmpegVideoWidget::setFrame,
                                    Qt::QueuedConnection);
    }
}

void FFmpegPlaybackBackend::openLocalFile(const QString& filePath)
{
    stopDecodeThread();
    closeInput();
    m_filePath.clear();
    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;
    m_durationMs = 0;
    m_positionMs.store(0);
    m_audioClockMs.store(0);
    m_seekGeneration.store(0);

    setMediaStatus(MediaStatus::Loading);
    emit playbackStateChanged(PlaybackState::Stopped);

    QString absoluteFilePath;
    if (!validateLocalFile(filePath, &absoluteFilePath)) {
        return;
    }

    AVFormatContext* rawFormatContext = nullptr;
    const QByteArray encodedPath = absoluteFilePath.toUtf8();

    int result = avformat_open_input(&rawFormatContext, encodedPath.constData(), nullptr, nullptr);
    if (result < 0) {
        avformat_close_input(&rawFormatContext);
        fail(QStringLiteral("FFmpeg open input failed: %1").arg(ffmpegErrorString(result)));
        return;
    }

    m_formatContext = rawFormatContext;
    result = avformat_find_stream_info(m_formatContext, nullptr);
    if (result < 0) {
        fail(QStringLiteral("FFmpeg stream info failed: %1").arg(ffmpegErrorString(result)));
        closeInput();
        return;
    }

    for (unsigned int i = 0; i < m_formatContext->nb_streams; ++i) {
        const AVStream* stream = m_formatContext->streams[i];
        if (stream == nullptr || stream->codecpar == nullptr) {
            continue;
        }

        switch (stream->codecpar->codec_type) {
        case AVMEDIA_TYPE_VIDEO:
            if (m_videoStreamIndex < 0 && !isAttachedPictureStream(stream)) {
                m_videoStreamIndex = static_cast<int>(i);
            }
            break;
        case AVMEDIA_TYPE_AUDIO:
            if (m_audioStreamIndex < 0) {
                m_audioStreamIndex = static_cast<int>(i);
            }
            break;
        default:
            break;
        }
    }

    if (m_videoStreamIndex < 0) {
        fail(QStringLiteral("FFmpeg did not find a playable video stream."));
        closeInput();
        return;
    }

    m_filePath = absoluteFilePath;
    m_durationMs = formatDurationMs(m_formatContext);

    emit positionChanged(m_positionMs.load());
    emit durationChanged(m_durationMs);
    setMediaStatus(MediaStatus::Loaded);
}

void FFmpegPlaybackBackend::openUrl(const QUrl& url)
{
    Q_UNUSED(url)
    fail(QStringLiteral("FFmpegPlaybackBackend currently supports local files only."));
}

void FFmpegPlaybackBackend::play()
{
    if (m_formatContext == nullptr || m_videoStreamIndex < 0) {
        fail(QStringLiteral("No local video file is loaded."));
        return;
    }

    startDecodeThread();
}

void FFmpegPlaybackBackend::pause()
{
    if (m_decodeThread.joinable() && !m_decodeFinished.load()) {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_pauseRequested = true;
        }
        m_isPlaying.store(false);
        m_stateChanged.notify_all();
    }
    emit playbackStateChanged(PlaybackState::Paused);
}

void FFmpegPlaybackBackend::stop()
{
    stopDecodeThread();
    m_positionMs.store(0);
    m_audioClockMs.store(0);
    emit positionChanged(m_positionMs.load());
    emit playbackStateChanged(PlaybackState::Stopped);
}

void FFmpegPlaybackBackend::seek(qint64 position)
{
    if (m_formatContext == nullptr || m_videoStreamIndex < 0) {
        m_positionMs.store(boundedPosition(position));
        m_audioClockMs.store(m_positionMs.load());
        emit positionChanged(m_positionMs.load());
        return;
    }

    const qint64 targetPosition = boundedPosition(position);
    const int targetStreamIndex = seekStreamIndex();

    int result = 0;
    {
        std::lock_guard<std::mutex> lock(m_decodeIoMutex);
        const qint64 targetTimestamp = positionToStreamTimestamp(targetPosition, targetStreamIndex);
        result = av_seek_frame(m_formatContext,
                               targetStreamIndex,
                               targetTimestamp,
                               AVSEEK_FLAG_BACKWARD);
        if (result >= 0 && m_videoCodecContext != nullptr) {
            avcodec_flush_buffers(m_videoCodecContext);
        }
        if (result >= 0 && m_audioCodecContext != nullptr) {
            avcodec_flush_buffers(m_audioCodecContext);
        }
    }

    if (result < 0) {
        fail(QStringLiteral("FFmpeg seek failed: %1").arg(ffmpegErrorString(result)));
        return;
    }

    m_seekGeneration.fetch_add(1);
    m_positionMs.store(targetPosition);
    m_audioClockMs.store(targetPosition);
    emit positionChanged(targetPosition);
    setMediaStatus(MediaStatus::Loaded);
    m_stateChanged.notify_all();
}

void FFmpegPlaybackBackend::setVolume(int volume)
{
    m_volume.store(qBound(0, volume, 100));
}

int FFmpegPlaybackBackend::volume() const
{
    return m_volume.load();
}

void FFmpegPlaybackBackend::setSpeed(double speed)
{
    m_speed.store(qBound(0.25, speed, 4.0));
}

double FFmpegPlaybackBackend::speed() const
{
    return m_speed.load();
}

qint64 FFmpegPlaybackBackend::position() const
{
    return m_positionMs.load();
}

qint64 FFmpegPlaybackBackend::duration() const
{
    return m_durationMs;
}

IPlaybackBackend::MediaStatus FFmpegPlaybackBackend::mediaStatus() const
{
    return m_mediaStatus;
}

IPlaybackBackend::PlaybackState FFmpegPlaybackBackend::playbackState() const
{
    if (m_isPlaying.load()) {
        return PlaybackState::Playing;
    }

    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_pauseRequested ? PlaybackState::Paused : PlaybackState::Stopped;
}

bool FFmpegPlaybackBackend::isPlaying() const
{
    return m_isPlaying.load();
}

void FFmpegPlaybackBackend::startDecodeThread()
{
    cleanupFinishedThread();

    if (m_decodeThread.joinable()) {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_pauseRequested = false;
        }
        m_isPlaying.store(true);
        m_stateChanged.notify_all();
        emit playbackStateChanged(PlaybackState::Playing);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_stopRequested = false;
        m_pauseRequested = false;
    }

    m_decodeFinished.store(false);
    m_isPlaying.store(true);
    m_decodeThread = std::thread(&FFmpegPlaybackBackend::decodeLoop, this);
    emit playbackStateChanged(PlaybackState::Playing);
}

void FFmpegPlaybackBackend::stopDecodeThread()
{
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_stopRequested = true;
        m_pauseRequested = false;
    }
    m_stateChanged.notify_all();

    if (m_decodeThread.joinable()) {
        m_decodeThread.join();
    }

    m_decodeFinished.store(false);
    m_isPlaying.store(false);
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_stopRequested = false;
        m_pauseRequested = false;
    }
}

void FFmpegPlaybackBackend::cleanupFinishedThread()
{
    if (m_decodeThread.joinable() && m_decodeFinished.load()) {
        m_decodeThread.join();
    }
}

bool FFmpegPlaybackBackend::waitUntilRunnable()
{
    std::unique_lock<std::mutex> lock(m_stateMutex);
    m_stateChanged.wait(lock, [this]() {
        return m_stopRequested || !m_pauseRequested;
    });
    return !m_stopRequested;
}

bool FFmpegPlaybackBackend::waitFrameInterval(int delayMs)
{
    const double playbackSpeed = qBound(0.25, m_speed.load(), 4.0);
    const int adjustedDelayMs = qMax(1, static_cast<int>(std::lround(delayMs / playbackSpeed)));
    const quint64 generation = m_seekGeneration.load();

    std::unique_lock<std::mutex> lock(m_stateMutex);
    if (m_stopRequested) {
        return false;
    }

    m_stateChanged.wait_for(lock,
                            std::chrono::milliseconds(adjustedDelayMs),
                            [this, generation]() {
                                return m_stopRequested
                                    || m_pauseRequested
                                    || m_seekGeneration.load() != generation;
                            });
    return !m_stopRequested;
}

void FFmpegPlaybackBackend::decodeLoop()
{
    bool reachedEnd = false;
    const AVStream* videoStream = m_formatContext->streams[m_videoStreamIndex];
    const AVCodecParameters* videoCodecParameters = videoStream->codecpar;
    const AVCodec* videoDecoder = avcodec_find_decoder(videoCodecParameters->codec_id);
    if (videoDecoder == nullptr) {
        fail(QStringLiteral("FFmpeg did not find a decoder for the video stream."));
        m_isPlaying.store(false);
        m_decodeFinished.store(true);
        emit playbackStateChanged(PlaybackState::Stopped);
        return;
    }

    std::unique_ptr<AVCodecContext, decltype(&freeCodecContext)> videoCodecContext(
        avcodec_alloc_context3(videoDecoder),
        freeCodecContext);
    if (!videoCodecContext) {
        fail(QStringLiteral("FFmpeg failed to allocate video decoder context."));
        m_isPlaying.store(false);
        m_decodeFinished.store(true);
        emit playbackStateChanged(PlaybackState::Stopped);
        return;
    }

    int result = avcodec_parameters_to_context(videoCodecContext.get(), videoCodecParameters);
    if (result < 0) {
        fail(QStringLiteral("FFmpeg failed to configure video decoder: %1").arg(ffmpegErrorString(result)));
        m_isPlaying.store(false);
        m_decodeFinished.store(true);
        emit playbackStateChanged(PlaybackState::Stopped);
        return;
    }

    result = avcodec_open2(videoCodecContext.get(), videoDecoder, nullptr);
    if (result < 0) {
        fail(QStringLiteral("FFmpeg failed to open video decoder: %1").arg(ffmpegErrorString(result)));
        m_isPlaying.store(false);
        m_decodeFinished.store(true);
        emit playbackStateChanged(PlaybackState::Stopped);
        return;
    }

    const bool hasAudioStream = m_audioStreamIndex >= 0;
    const AVStream* audioStream = hasAudioStream ? m_formatContext->streams[m_audioStreamIndex] : nullptr;
    std::unique_ptr<AVCodecContext, decltype(&freeCodecContext)> audioCodecContext(nullptr, freeCodecContext);
    std::unique_ptr<SwrContext, decltype(&freeSwrContext)> swrContext(nullptr, freeSwrContext);
    std::unique_ptr<QAudioSink> audioSink;
    QIODevice* audioDevice = nullptr;
    QAudioFormat audioFormat;
    bool audioClockStarted = false;
    qint64 audioClockBaseUs = boundedPosition(m_positionMs.load()) * 1000;
    qint64 audioSubmittedUntilUs = audioClockBaseUs;

    if (hasAudioStream && audioStream != nullptr && audioStream->codecpar != nullptr) {
        const AVCodec* audioDecoder = avcodec_find_decoder(audioStream->codecpar->codec_id);
        if (audioDecoder == nullptr) {
            fail(QStringLiteral("FFmpeg did not find a decoder for the audio stream."));
            m_isPlaying.store(false);
            m_decodeFinished.store(true);
            emit playbackStateChanged(PlaybackState::Stopped);
            return;
        }

        audioCodecContext.reset(avcodec_alloc_context3(audioDecoder));
        if (!audioCodecContext) {
            fail(QStringLiteral("FFmpeg failed to allocate audio decoder context."));
            m_isPlaying.store(false);
            m_decodeFinished.store(true);
            emit playbackStateChanged(PlaybackState::Stopped);
            return;
        }

        result = avcodec_parameters_to_context(audioCodecContext.get(), audioStream->codecpar);
        if (result < 0) {
            fail(QStringLiteral("FFmpeg failed to configure audio decoder: %1").arg(ffmpegErrorString(result)));
            m_isPlaying.store(false);
            m_decodeFinished.store(true);
            emit playbackStateChanged(PlaybackState::Stopped);
            return;
        }

        result = avcodec_open2(audioCodecContext.get(), audioDecoder, nullptr);
        if (result < 0) {
            fail(QStringLiteral("FFmpeg failed to open audio decoder: %1").arg(ffmpegErrorString(result)));
            m_isPlaying.store(false);
            m_decodeFinished.store(true);
            emit playbackStateChanged(PlaybackState::Stopped);
            return;
        }

        const QAudioDevice outputDevice = QMediaDevices::defaultAudioOutput();
        QString audioFormatError;
        if (!preparePcmS16Format(outputDevice,
                                 audioCodecContext->sample_rate,
                                 &audioFormat,
                                 &audioFormatError)) {
            fail(audioFormatError);
            m_isPlaying.store(false);
            m_decodeFinished.store(true);
            emit playbackStateChanged(PlaybackState::Stopped);
            return;
        }

        AVChannelLayout inputLayout = {};
        if (audioCodecContext->ch_layout.nb_channels > 0) {
            av_channel_layout_copy(&inputLayout, &audioCodecContext->ch_layout);
        } else {
            av_channel_layout_default(&inputLayout, audioFormat.channelCount());
        }

        AVChannelLayout outputLayout = {};
        av_channel_layout_default(&outputLayout, audioFormat.channelCount());

        SwrContext* rawSwrContext = nullptr;
        result = swr_alloc_set_opts2(&rawSwrContext,
                                     &outputLayout,
                                     AV_SAMPLE_FMT_S16,
                                     audioFormat.sampleRate(),
                                     &inputLayout,
                                     audioCodecContext->sample_fmt,
                                     audioCodecContext->sample_rate,
                                     0,
                                     nullptr);
        av_channel_layout_uninit(&inputLayout);
        av_channel_layout_uninit(&outputLayout);

        if (result < 0 || rawSwrContext == nullptr) {
            fail(QStringLiteral("FFmpeg failed to configure audio resampler: %1").arg(ffmpegErrorString(result)));
            m_isPlaying.store(false);
            m_decodeFinished.store(true);
            emit playbackStateChanged(PlaybackState::Stopped);
            return;
        }
        swrContext.reset(rawSwrContext);

        result = swr_init(swrContext.get());
        if (result < 0) {
            fail(QStringLiteral("FFmpeg failed to initialize audio resampler: %1").arg(ffmpegErrorString(result)));
            m_isPlaying.store(false);
            m_decodeFinished.store(true);
            emit playbackStateChanged(PlaybackState::Stopped);
            return;
        }

        audioSink = std::make_unique<QAudioSink>(outputDevice, audioFormat);
        audioSink->setBufferSize(qMax<qsizetype>(audioFormat.bytesForDuration(120000),
                                                 audioFormat.bytesPerFrame() * 2048));
        audioSink->setVolume(qBound(0, m_volume.load(), 100) / 100.0);
        audioDevice = audioSink->start();
        if (audioDevice == nullptr) {
            fail(QStringLiteral("Qt failed to start audio output."));
            m_isPlaying.store(false);
            m_decodeFinished.store(true);
            emit playbackStateChanged(PlaybackState::Stopped);
            return;
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_decodeIoMutex);
        m_videoCodecContext = videoCodecContext.get();
        m_audioCodecContext = audioCodecContext.get();
    }

    const qint64 startPosition = boundedPosition(m_positionMs.load());
    result = 0;
    if (startPosition > 0) {
        std::lock_guard<std::mutex> lock(m_decodeIoMutex);
        result = av_seek_frame(m_formatContext,
                               seekStreamIndex(),
                               positionToStreamTimestamp(startPosition, seekStreamIndex()),
                               AVSEEK_FLAG_BACKWARD);
        if (result >= 0) {
            avcodec_flush_buffers(videoCodecContext.get());
            if (audioCodecContext) {
                avcodec_flush_buffers(audioCodecContext.get());
            }
        }
    }
    if (result < 0) {
        fail(QStringLiteral("FFmpeg seek failed: %1").arg(ffmpegErrorString(result)));
        {
            std::lock_guard<std::mutex> lock(m_decodeIoMutex);
            if (m_videoCodecContext == videoCodecContext.get()) {
                m_videoCodecContext = nullptr;
            }
            if (m_audioCodecContext == audioCodecContext.get()) {
                m_audioCodecContext = nullptr;
            }
        }
        m_isPlaying.store(false);
        m_decodeFinished.store(true);
        emit playbackStateChanged(PlaybackState::Stopped);
        return;
    }

    std::unique_ptr<AVPacket, decltype(&freePacket)> packet(av_packet_alloc(), freePacket);
    std::unique_ptr<AVFrame, decltype(&freeFrame)> videoFrame(av_frame_alloc(), freeFrame);
    std::unique_ptr<AVFrame, decltype(&freeFrame)> audioFrame(av_frame_alloc(), freeFrame);
    std::unique_ptr<SwsContext, decltype(&freeSwsContext)> swsContext(nullptr, freeSwsContext);
    if (!packet || !videoFrame || !audioFrame) {
        fail(QStringLiteral("FFmpeg failed to allocate packet or frame buffers."));
        {
            std::lock_guard<std::mutex> lock(m_decodeIoMutex);
            if (m_videoCodecContext == videoCodecContext.get()) {
                m_videoCodecContext = nullptr;
            }
            if (m_audioCodecContext == audioCodecContext.get()) {
                m_audioCodecContext = nullptr;
            }
        }
        m_isPlaying.store(false);
        m_decodeFinished.store(true);
        emit playbackStateChanged(PlaybackState::Stopped);
        return;
    }

    const int frameDelayMs = streamFrameDelayMs(videoStream);
    quint64 localSeekGeneration = m_seekGeneration.load();
    int appliedVolume = -1;

    auto updateAudioClock = [&]() {
        if (audioSink && audioClockStarted) {
            // audio clock = 已提交给 QAudioSink 的音频终�?- 仍在声卡缓冲区中的音频时长�?            // 这个 clock 跟真实播放进度绑定；pause 时缓冲区不再消耗，所�?resume 后不会凭空跳变�?            const qsizetype pendingBytes = qMax<qsizetype>(0, audioSink->bufferSize() - audioSink->bytesFree());
            const qint64 pendingUs = audioFormat.durationForBytes(static_cast<qint32>(pendingBytes));
            const qint64 clockUs = qBound(audioClockBaseUs,
                                          audioSubmittedUntilUs - pendingUs,
                                          audioSubmittedUntilUs);
            const qint64 clockMs = boundedPosition(clockUs / 1000);
            m_audioClockMs.store(clockMs);
            m_positionMs.store(clockMs);
            emit positionChanged(clockMs);
            return clockMs;
        }

        return m_positionMs.load();
    };

    auto syncSeekGeneration = [&]() -> bool {
        const quint64 currentGeneration = m_seekGeneration.load();
        if (currentGeneration == localSeekGeneration) {
            return true;
        }

        if (audioSink) {
            audioSink->reset();
            audioDevice = audioSink->start();
        }
        if (swrContext) {
            swr_init(swrContext.get());
        }
        audioClockStarted = false;
        audioClockBaseUs = boundedPosition(m_positionMs.load()) * 1000;
        audioSubmittedUntilUs = audioClockBaseUs;
        m_audioClockMs.store(audioClockBaseUs / 1000);
        // seek 后所有旧 packet/frame 都可能来自旧时间线，重置本地同步基准后才允许继续显示�?        localSeekGeneration = currentGeneration;
        return audioDevice != nullptr || !audioSink;
    };

    auto waitForPlayback = [&]() -> bool {
        bool wasSuspended = false;
        while (true) {
            {
                std::unique_lock<std::mutex> lock(m_stateMutex);
                if (m_stopRequested) {
                    return false;
                }
                if (!m_pauseRequested) {
                    break;
                }
            }

            if (audioSink && audioSink->state() != QAudio::SuspendedState) {
                updateAudioClock();
                audioSink->suspend();
                wasSuspended = true;
            }

            std::unique_lock<std::mutex> lock(m_stateMutex);
            m_stateChanged.wait(lock, [&]() {
                return m_stopRequested || !m_pauseRequested || m_seekGeneration.load() != localSeekGeneration;
            });
            if (m_stopRequested) {
                return false;
            }
            if (!syncSeekGeneration()) {
                return false;
            }
        }

        if (audioSink && wasSuspended && audioSink->state() == QAudio::SuspendedState) {
            audioSink->resume();
        }
        return syncSeekGeneration();
    };

    auto waitBriefly = [&]() -> bool {
        std::unique_lock<std::mutex> lock(m_stateMutex);
        m_stateChanged.wait_for(lock, std::chrono::milliseconds(5), [&]() {
            return m_stopRequested || m_pauseRequested || m_seekGeneration.load() != localSeekGeneration;
        });
        return !m_stopRequested;
    };

    auto writeAudioData = [&](const QByteArray& pcmData, qint64 pcmStartMs) -> bool {
        if (!audioSink || audioDevice == nullptr || pcmData.isEmpty()) {
            return true;
        }

        if (!audioClockStarted) {
            audioClockBaseUs = boundedPosition(pcmStartMs) * 1000;
            audioSubmittedUntilUs = audioClockBaseUs;
            audioClockStarted = true;
            m_audioClockMs.store(audioClockBaseUs / 1000);
        }

        qsizetype offset = 0;
        while (offset < pcmData.size()) {
            if (!waitForPlayback()) {
                return false;
            }

            const int requestedVolume = qBound(0, m_volume.load(), 100);
            if (requestedVolume != appliedVolume) {
                audioSink->setVolume(requestedVolume / 100.0);
                appliedVolume = requestedVolume;
            }

            const qsizetype writableBytes = audioSink->bytesFree();
            if (writableBytes <= 0) {
                updateAudioClock();
                if (!waitBriefly()) {
                    return false;
                }
                continue;
            }

            const qsizetype bytesToWrite = qMin<qsizetype>(writableBytes, pcmData.size() - offset);
            const qint64 writtenBytes = audioDevice->write(pcmData.constData() + offset, bytesToWrite);
            if (writtenBytes < 0) {
                fail(QStringLiteral("Qt audio output write failed."));
                return false;
            }
            if (writtenBytes == 0) {
                if (!waitBriefly()) {
                    return false;
                }
                continue;
            }

            offset += writtenBytes;
            // 只有真正写入 QAudioSink 的字节才推进提交终点，避免把尚未播放的解码数据算进主时钟�?            audioSubmittedUntilUs += audioFormat.durationForBytes(static_cast<qint32>(writtenBytes));
            updateAudioClock();
        }

        return true;
    };

    auto receiveAudioFrames = [&]() -> bool {
        if (!audioCodecContext || !swrContext) {
            return true;
        }

        while (waitForPlayback()) {
            const quint64 receiveGeneration = m_seekGeneration.load();
            int receiveResult = 0;
            {
                std::lock_guard<std::mutex> lock(m_decodeIoMutex);
                receiveResult = avcodec_receive_frame(audioCodecContext.get(), audioFrame.get());
            }
            if (receiveResult == AVERROR(EAGAIN)) {
                return true;
            }
            if (receiveResult == AVERROR_EOF) {
                return true;
            }
            if (receiveResult < 0) {
                fail(QStringLiteral("FFmpeg failed to receive decoded audio frame: %1").arg(ffmpegErrorString(receiveResult)));
                return false;
            }

            if (receiveGeneration != m_seekGeneration.load()) {
                av_frame_unref(audioFrame.get());
                continue;
            }

            const qint64 framePosition = frameTimestampMs(audioFrame.get(), audioStream, m_positionMs.load());
            if (!audioClockStarted) {
                audioClockBaseUs = boundedPosition(framePosition) * 1000;
                audioSubmittedUntilUs = audioClockBaseUs;
                audioClockStarted = true;
                m_audioClockMs.store(audioClockBaseUs / 1000);
            }

            const int64_t delayedSamples = swr_get_delay(swrContext.get(), audioCodecContext->sample_rate);
            const int outputSamples = static_cast<int>(av_rescale_rnd(delayedSamples + audioFrame->nb_samples,
                                                                      audioFormat.sampleRate(),
                                                                      audioCodecContext->sample_rate,
                                                                      AV_ROUND_UP));
            const int outputBufferSize = audioFormat.bytesForFrames(outputSamples);
            QByteArray pcmData(outputBufferSize, Qt::Uninitialized);
            uint8_t* outputData[] = { reinterpret_cast<uint8_t*>(pcmData.data()) };
            const int convertedSamples = swr_convert(swrContext.get(),
                                                     outputData,
                                                     outputSamples,
                                                     const_cast<const uint8_t**>(audioFrame->extended_data),
                                                     audioFrame->nb_samples);
            av_frame_unref(audioFrame.get());

            if (convertedSamples < 0) {
                fail(QStringLiteral("FFmpeg failed to resample audio: %1").arg(ffmpegErrorString(convertedSamples)));
                return false;
            }

            pcmData.resize(audioFormat.bytesForFrames(convertedSamples));
            if (receiveGeneration != m_seekGeneration.load()) {
                continue;
            }
            if (!writeAudioData(pcmData, framePosition)) {
                return false;
            }
        }

        return false;
    };

    auto syncVideoFrame = [&](qint64 framePosition, quint64 frameGeneration) -> VideoSyncDecision {
        if (!audioSink || !audioClockStarted) {
            return waitFrameInterval(frameDelayMs) ? VideoSyncDecision::Display : VideoSyncDecision::Stop;
        }

        while (waitForPlayback()) {
            if (frameGeneration != m_seekGeneration.load()) {
                return VideoSyncDecision::Drop;
            }

            // 音频是主时钟：音频一旦写�?QAudioSink，就按声卡缓冲区消耗量推进时间�?            // 视频只比较自己的 pts �?audio clock；不反向拉动音频，避免声音卡顿或跳变�?            const qint64 audioClock = updateAudioClock();
            const qint64 diffMs = framePosition - audioClock;

            // 视频慢太多时继续显示只会造成“追不上”的拖影，直接丢帧追音频�?            if (diffMs < -kVideoDropThresholdMs) {
                return VideoSyncDecision::Drop;
            }

            // 视频快了就等音频 clock 追上；小于容忍窗口时直接显示，减少抖动�?            if (diffMs <= kVideoEarlyToleranceMs) {
                return VideoSyncDecision::Display;
            }

            std::unique_lock<std::mutex> lock(m_stateMutex);
            m_stateChanged.wait_for(lock,
                                    std::chrono::milliseconds(qMin<qint64>(diffMs - kVideoEarlyToleranceMs,
                                                                          kVideoSyncWaitSliceMs)),
                                    [this, frameGeneration]() {
                                        return m_stopRequested
                                            || m_pauseRequested
                                            || m_seekGeneration.load() != frameGeneration;
                                    });
            if (m_stopRequested) {
                return VideoSyncDecision::Stop;
            }
        }

        return VideoSyncDecision::Stop;
    };

    auto receiveVideoFrames = [&]() -> bool {
        while (waitForPlayback()) {
            const quint64 receiveGeneration = m_seekGeneration.load();
            int receiveResult = 0;
            {
                std::lock_guard<std::mutex> lock(m_decodeIoMutex);
                receiveResult = avcodec_receive_frame(videoCodecContext.get(), videoFrame.get());
            }
            if (receiveResult == AVERROR(EAGAIN)) {
                return true;
            }
            if (receiveResult == AVERROR_EOF) {
                reachedEnd = true;
                return false;
            }
            if (receiveResult < 0) {
                fail(QStringLiteral("FFmpeg failed to receive decoded video frame: %1").arg(ffmpegErrorString(receiveResult)));
                return false;
            }

            if (receiveGeneration != m_seekGeneration.load()) {
                av_frame_unref(videoFrame.get());
                continue;
            }

            QImage image;
            QString errorMessage;
            SwsContext* rawSwsContext = swsContext.release();
            const bool converted = convertFrameToImage(videoFrame.get(), &rawSwsContext, &image, &errorMessage);
            swsContext.reset(rawSwsContext);
            const qint64 framePosition = frameTimestampMs(videoFrame.get(),
                                                          videoStream,
                                                          m_positionMs.load() + frameDelayMs);
            av_frame_unref(videoFrame.get());

            if (!converted) {
                fail(errorMessage);
                return false;
            }

            if (receiveGeneration != m_seekGeneration.load()) {
                continue;
            }

            const VideoSyncDecision syncDecision = syncVideoFrame(framePosition, receiveGeneration);
            if (syncDecision == VideoSyncDecision::Stop) {
                return false;
            }
            if (syncDecision == VideoSyncDecision::Drop) {
                continue;
            }
            if (receiveGeneration != m_seekGeneration.load()) {
                continue;
            }

            if (!audioSink || !audioClockStarted) {
                m_positionMs.store(boundedPosition(framePosition));
                emit positionChanged(m_positionMs.load());
            }
            emit videoFrameReady(image);
        }

        return false;
    };

    while (waitForPlayback()) {
        const quint64 readGeneration = m_seekGeneration.load();
        bool readVideoPacket = false;
        bool readAudioPacket = false;
        bool attemptedSendPacket = false;
        {
            std::lock_guard<std::mutex> lock(m_decodeIoMutex);
            result = av_read_frame(m_formatContext, packet.get());
            readVideoPacket = result >= 0 && packet->stream_index == m_videoStreamIndex;
            readAudioPacket = result >= 0 && audioCodecContext && packet->stream_index == m_audioStreamIndex;
            if (readVideoPacket && readGeneration == m_seekGeneration.load()) {
                attemptedSendPacket = true;
                result = avcodec_send_packet(videoCodecContext.get(), packet.get());
            } else if (readAudioPacket && readGeneration == m_seekGeneration.load()) {
                attemptedSendPacket = true;
                result = avcodec_send_packet(audioCodecContext.get(), packet.get());
            }
        }

        if (readGeneration != m_seekGeneration.load()) {
            av_packet_unref(packet.get());
            continue;
        }

        if (result == AVERROR_EOF) {
            {
                std::lock_guard<std::mutex> lock(m_decodeIoMutex);
                result = avcodec_send_packet(videoCodecContext.get(), nullptr);
                if (result >= 0 && audioCodecContext) {
                    result = avcodec_send_packet(audioCodecContext.get(), nullptr);
                }
            }
            if (result < 0 && result != AVERROR_EOF) {
                fail(QStringLiteral("FFmpeg failed to flush decoders: %1").arg(ffmpegErrorString(result)));
                break;
            }
            receiveAudioFrames();
            receiveVideoFrames();
            reachedEnd = true;
            break;
        }
        if (result < 0) {
            av_packet_unref(packet.get());
            fail(attemptedSendPacket
                     ? QStringLiteral("FFmpeg failed to send video packet to decoder: %1").arg(ffmpegErrorString(result))
                     : QStringLiteral("FFmpeg failed to read video packet: %1").arg(ffmpegErrorString(result)));
            break;
        }

        if (packet->stream_index != m_videoStreamIndex && packet->stream_index != m_audioStreamIndex) {
            av_packet_unref(packet.get());
            continue;
        }

        const int packetStreamIndex = packet->stream_index;
        av_packet_unref(packet.get());

        if (packetStreamIndex == m_audioStreamIndex) {
            if (!receiveAudioFrames()) {
                break;
            }
            continue;
        }

        if (!receiveVideoFrames()) {
            break;
        }
    }

    if (audioSink) {
        audioSink->stop();
    }

    {
        std::lock_guard<std::mutex> lock(m_decodeIoMutex);
        if (m_videoCodecContext == videoCodecContext.get()) {
            m_videoCodecContext = nullptr;
        }
        if (m_audioCodecContext == audioCodecContext.get()) {
            m_audioCodecContext = nullptr;
        }
    }

    m_isPlaying.store(false);
    m_decodeFinished.store(true);
    emit playbackStateChanged(PlaybackState::Stopped);
    if (reachedEnd) {
        setMediaStatus(MediaStatus::EndOfMedia);
    }
}

void FFmpegPlaybackBackend::closeInput()
{
    std::lock_guard<std::mutex> lock(m_decodeIoMutex);
    m_videoCodecContext = nullptr;
    m_audioCodecContext = nullptr;
    if (m_formatContext != nullptr) {
        avformat_close_input(&m_formatContext);
    }
}

bool FFmpegPlaybackBackend::validateLocalFile(const QString& filePath, QString* absoluteFilePath)
{
    const QString normalizedPath = filePath.trimmed();
    if (normalizedPath.isEmpty()) {
        return fail(QStringLiteral("File path is empty."));
    }

    const QFileInfo fileInfo(normalizedPath);
    const QString resolvedPath = fileInfo.absoluteFilePath();
    if (absoluteFilePath != nullptr) {
        *absoluteFilePath = resolvedPath;
    }

    if (!fileInfo.exists()) {
        return fail(QStringLiteral("File does not exist: %1").arg(resolvedPath));
    }
    if (!fileInfo.isFile()) {
        return fail(QStringLiteral("Path is not a local media file: %1").arg(resolvedPath));
    }

    QFile file(resolvedPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(QStringLiteral("File is not readable: %1").arg(resolvedPath));
    }
    file.close();

    if (fileInfo.size() <= 0) {
        return fail(QStringLiteral("File is empty: %1").arg(resolvedPath));
    }

    return true;
}

bool FFmpegPlaybackBackend::fail(const QString& message)
{
    setMediaStatus(MediaStatus::InvalidMedia);
    emit playbackStateChanged(PlaybackState::Stopped);
    emit errorOccurred(PlaybackError::FormatError, message);
    return false;
}

qint64 FFmpegPlaybackBackend::boundedPosition(qint64 requestedPosition) const
{
    const qint64 targetPosition = qMax<qint64>(0, requestedPosition);
    if (m_durationMs <= 0) {
        return targetPosition;
    }
    return qBound<qint64>(0, targetPosition, m_durationMs);
}

qint64 FFmpegPlaybackBackend::positionToStreamTimestamp(qint64 positionMs, int streamIndex) const
{
    if (m_formatContext == nullptr || streamIndex < 0) {
        return 0;
    }

    const AVStream* stream = m_formatContext->streams[streamIndex];
    if (stream == nullptr) {
        return 0;
    }

    return av_rescale_q(positionMs, AVRational { 1, 1000 }, stream->time_base);
}

int FFmpegPlaybackBackend::seekStreamIndex() const
{
    return m_audioStreamIndex >= 0 ? m_audioStreamIndex : m_videoStreamIndex;
}

void FFmpegPlaybackBackend::setMediaStatus(MediaStatus status)
{
    m_mediaStatus = status;
    emit mediaStatusChanged(status);
}

#endif // USE_FFMPEG
