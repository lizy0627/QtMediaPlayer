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
#include <QDebug>
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
constexpr qint64 kSeekPrerollToleranceMs = 15;

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

const char* failureCategory(const QString& message)
{
    if (message.contains(QStringLiteral("open input"), Qt::CaseInsensitive)
        || message.contains(QStringLiteral("stream info"), Qt::CaseInsensitive)
        || message.contains(QStringLiteral("playable video stream"), Qt::CaseInsensitive)
        || message.contains(QStringLiteral("File "), Qt::CaseSensitive)
        || message.contains(QStringLiteral("Path "), Qt::CaseSensitive)) {
        return "open";
    }
    if (message.contains(QStringLiteral("audio output"), Qt::CaseInsensitive)
        || message.contains(QStringLiteral("audio sink"), Qt::CaseInsensitive)
        || message.contains(QStringLiteral("audio output write"), Qt::CaseInsensitive)) {
        return "audio-output";
    }
    if (message.contains(QStringLiteral("decoder"), Qt::CaseInsensitive)
        || message.contains(QStringLiteral("decoded"), Qt::CaseInsensitive)
        || message.contains(QStringLiteral("packet"), Qt::CaseInsensitive)
        || message.contains(QStringLiteral("resample"), Qt::CaseInsensitive)
        || message.contains(QStringLiteral("frame"), Qt::CaseInsensitive)) {
        return "decode";
    }
    if (message.contains(QStringLiteral("seek"), Qt::CaseInsensitive)) {
        return "seek";
    }

    return "playback";
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

void freeCodecParameters(AVCodecParameters* parameters)
{
    avcodec_parameters_free(&parameters);
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

void deleteAudioSink(QAudioSink* sink)
{
    if (sink != nullptr) {
        sink->stop();
        delete sink;
    }
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

int streamFrameDelayMs(AVRational avgFrameRate, AVRational rFrameRate)
{
    double fps = rationalToDouble(avgFrameRate);
    if (fps <= 0.0) {
        fps = rationalToDouble(rFrameRate);
    }
    if (fps <= 0.0) {
        return 33;
    }

    return qMax(1, static_cast<int>(std::lround(1000.0 / fps)));
}

qint64 frameTimestampMs(const AVFrame* frame, AVRational timeBase, qint64 fallbackMs)
{
    if (frame == nullptr
        || timeBase.num <= 0
        || timeBase.den <= 0
        || frame->best_effort_timestamp == AV_NOPTS_VALUE) {
        return fallbackMs;
    }

    return av_rescale_q(frame->best_effort_timestamp, timeBase, AVRational { 1, 1000 });
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
                                    Qt::DirectConnection);
    }
}

void FFmpegPlaybackBackend::openLocalFile(const QString& filePath)
{
    stopDecodeThread();
    closeInput();
    m_filePath = filePath.trimmed();
    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;
    m_durationMs = 0;
    m_positionMs.store(0);
    m_audioClockMs.store(0);
    m_seekGeneration.store(0);
    m_seekTargetMs.store(-1);

    m_errorEmittedForCurrentMedia = false;
    setMediaStatus(MediaStatus::Loading, true);
    setPlaybackState(PlaybackState::Stopped);

    qDebug() << "[FFmpegPlaybackBackend] openLocalFile"
             << "file" << m_filePath;

    QString absoluteFilePath;
    if (!validateLocalFile(filePath, &absoluteFilePath)) {
        return;
    }
    m_filePath = absoluteFilePath;
    qDebug() << "[FFmpegPlaybackBackend] opening input"
             << "file" << m_filePath;

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

    m_durationMs = formatDurationMs(m_formatContext);

    emit positionChanged(m_positionMs.load());
    emit durationChanged(m_durationMs);
    setMediaStatus(MediaStatus::Loaded);
    qDebug() << "[FFmpegPlaybackBackend] loaded"
             << "file" << m_filePath
             << "durationMs" << m_durationMs
             << "videoStream" << m_videoStreamIndex
             << "audioStream" << m_audioStreamIndex;
}

void FFmpegPlaybackBackend::openUrl(const QUrl& url)
{
    Q_UNUSED(url)
    fail(QStringLiteral("FFmpegPlaybackBackend currently supports local files only."));
}

void FFmpegPlaybackBackend::play()
{
    bool hasInput = false;
    {
        std::lock_guard<std::mutex> lock(m_decodeIoMutex);
        hasInput = m_formatContext != nullptr && m_videoStreamIndex >= 0;
    }

    if (!hasInput) {
        fail(QStringLiteral("No local video file is loaded."));
        return;
    }

    qDebug() << "[FFmpegPlaybackBackend] play"
             << "file" << m_filePath
             << "positionMs" << m_positionMs.load();
    startDecodeThread();
}

void FFmpegPlaybackBackend::pause()
{
    qDebug() << "[FFmpegPlaybackBackend] pause"
             << "file" << m_filePath
             << "positionMs" << m_positionMs.load();
    if (m_decodeThread.joinable() && !m_decodeFinished.load()) {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_pauseRequested = true;
        }
        m_isPlaying.store(false);
        m_stateChanged.notify_all();
    }
    setPlaybackState(PlaybackState::Paused);
}

void FFmpegPlaybackBackend::stop()
{
    qDebug() << "[FFmpegPlaybackBackend] stop"
             << "file" << m_filePath
             << "positionMs" << m_positionMs.load();
    stopDecodeThread();
    m_seekTargetMs.store(-1);
    m_positionMs.store(0);
    m_audioClockMs.store(0);
    emit positionChanged(m_positionMs.load());
    setPlaybackState(PlaybackState::Stopped);
}

void FFmpegPlaybackBackend::seek(qint64 position)
{
    const qint64 targetPosition = boundedPosition(position);
    int result = 0;
    bool hasInput = false;

    qDebug() << "[FFmpegPlaybackBackend] seek"
             << "file" << m_filePath
             << "fromMs" << m_positionMs.load()
             << "toMs" << targetPosition
             << "requestedMs" << position;

    m_seekTargetMs.store(targetPosition);
    m_seekGeneration.fetch_add(1);
    m_stateChanged.notify_all();

    {
        std::lock_guard<std::mutex> lock(m_decodeIoMutex);
        hasInput = m_formatContext != nullptr && m_videoStreamIndex >= 0;
        if (hasInput) {
            const int targetStreamIndex = seekStreamIndex();
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
    }

    if (!hasInput) {
        m_positionMs.store(targetPosition);
        m_audioClockMs.store(targetPosition);
        m_seekTargetMs.store(-1);
        emit positionChanged(targetPosition);
        return;
    }

    if (result < 0) {
        m_seekTargetMs.store(-1);
        fail(QStringLiteral("FFmpeg seek failed: %1").arg(ffmpegErrorString(result)));
        return;
    }

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
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_mediaStatus;
}

IPlaybackBackend::PlaybackState FFmpegPlaybackBackend::playbackState() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_playbackState;
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
        setPlaybackState(PlaybackState::Playing);
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
    setPlaybackState(PlaybackState::Playing);
}

void FFmpegPlaybackBackend::stopDecodeThread()
{
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_stopRequested = true;
        m_pauseRequested = false;
    }
    m_stateChanged.notify_all();

    const bool calledFromDecodeThread = m_decodeThread.joinable()
        && std::this_thread::get_id() == m_decodeThread.get_id();
    if (m_decodeThread.joinable() && !calledFromDecodeThread) {
        m_decodeThread.join();
    }

    if (calledFromDecodeThread) {
        return;
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
    qDebug() << "[FFmpegPlaybackBackend] decode started"
             << "file" << m_filePath
             << "positionMs" << m_positionMs.load();

    bool reachedEnd = false;
    std::unique_ptr<AVCodecParameters, decltype(&freeCodecParameters)> videoCodecParameters(
        avcodec_parameters_alloc(),
        freeCodecParameters);
    if (!videoCodecParameters) {
        fail(QStringLiteral("FFmpeg failed to allocate video stream parameters."));
        m_isPlaying.store(false);
        m_decodeFinished.store(true);
        setPlaybackState(PlaybackState::Stopped);
        return;
    }

    std::unique_ptr<AVCodecParameters, decltype(&freeCodecParameters)> audioCodecParameters(nullptr,
                                                                                            freeCodecParameters);
    int localVideoStreamIndex = -1;
    int localAudioStreamIndex = -1;
    bool hasAudioStream = false;
    AVRational videoTimeBase = { 0, 1 };
    AVRational videoAvgFrameRate = { 0, 1 };
    AVRational videoRFrameRate = { 0, 1 };
    AVRational audioTimeBase = { 0, 1 };

    int result = 0;
    {
        std::lock_guard<std::mutex> lock(m_decodeIoMutex);
        localVideoStreamIndex = m_videoStreamIndex;
        localAudioStreamIndex = m_audioStreamIndex;
        if (m_formatContext == nullptr
            || localVideoStreamIndex < 0
            || static_cast<unsigned int>(localVideoStreamIndex) >= m_formatContext->nb_streams
            || m_formatContext->streams[localVideoStreamIndex] == nullptr
            || m_formatContext->streams[localVideoStreamIndex]->codecpar == nullptr) {
            result = AVERROR_INVALIDDATA;
        } else {
            const AVStream* videoStream = m_formatContext->streams[localVideoStreamIndex];
            result = avcodec_parameters_copy(videoCodecParameters.get(), videoStream->codecpar);
            videoTimeBase = videoStream->time_base;
            videoAvgFrameRate = videoStream->avg_frame_rate;
            videoRFrameRate = videoStream->r_frame_rate;
        }

        if (result >= 0
            && localAudioStreamIndex >= 0
            && static_cast<unsigned int>(localAudioStreamIndex) < m_formatContext->nb_streams
            && m_formatContext->streams[localAudioStreamIndex] != nullptr
            && m_formatContext->streams[localAudioStreamIndex]->codecpar != nullptr) {
            audioCodecParameters.reset(avcodec_parameters_alloc());
            if (!audioCodecParameters) {
                result = AVERROR(ENOMEM);
            } else {
                const AVStream* audioStream = m_formatContext->streams[localAudioStreamIndex];
                result = avcodec_parameters_copy(audioCodecParameters.get(), audioStream->codecpar);
                audioTimeBase = audioStream->time_base;
                hasAudioStream = result >= 0;
            }
        }
    }

    if (result < 0) {
        fail(QStringLiteral("FFmpeg failed to read stream parameters: %1").arg(ffmpegErrorString(result)));
        m_isPlaying.store(false);
        m_decodeFinished.store(true);
        setPlaybackState(PlaybackState::Stopped);
        return;
    }

    const AVCodec* videoDecoder = avcodec_find_decoder(videoCodecParameters->codec_id);
    if (videoDecoder == nullptr) {
        fail(QStringLiteral("FFmpeg did not find a decoder for the video stream."));
        m_isPlaying.store(false);
        m_decodeFinished.store(true);
        setPlaybackState(PlaybackState::Stopped);
        return;
    }

    std::unique_ptr<AVCodecContext, decltype(&freeCodecContext)> videoCodecContext(
        avcodec_alloc_context3(videoDecoder),
        freeCodecContext);
    if (!videoCodecContext) {
        fail(QStringLiteral("FFmpeg failed to allocate video decoder context."));
        m_isPlaying.store(false);
        m_decodeFinished.store(true);
        setPlaybackState(PlaybackState::Stopped);
        return;
    }

    result = avcodec_parameters_to_context(videoCodecContext.get(), videoCodecParameters.get());
    if (result < 0) {
        fail(QStringLiteral("FFmpeg failed to configure video decoder: %1").arg(ffmpegErrorString(result)));
        m_isPlaying.store(false);
        m_decodeFinished.store(true);
        setPlaybackState(PlaybackState::Stopped);
        return;
    }

    result = avcodec_open2(videoCodecContext.get(), videoDecoder, nullptr);
    if (result < 0) {
        fail(QStringLiteral("FFmpeg failed to open video decoder: %1").arg(ffmpegErrorString(result)));
        m_isPlaying.store(false);
        m_decodeFinished.store(true);
        setPlaybackState(PlaybackState::Stopped);
        return;
    }

    std::unique_ptr<AVCodecContext, decltype(&freeCodecContext)> audioCodecContext(nullptr, freeCodecContext);
    std::unique_ptr<SwrContext, decltype(&freeSwrContext)> swrContext(nullptr, freeSwrContext);
    std::unique_ptr<QAudioSink, decltype(&deleteAudioSink)> audioSink(nullptr, deleteAudioSink);
    QIODevice* audioDevice = nullptr;
    QAudioFormat audioFormat;
    bool audioClockStarted = false;
    qint64 audioClockBaseUs = boundedPosition(m_positionMs.load()) * 1000;
    qint64 audioSubmittedUntilUs = audioClockBaseUs;

    if (hasAudioStream && audioCodecParameters) {
        const AVCodec* audioDecoder = avcodec_find_decoder(audioCodecParameters->codec_id);
        if (audioDecoder == nullptr) {
            fail(QStringLiteral("FFmpeg did not find a decoder for the audio stream."));
            m_isPlaying.store(false);
            m_decodeFinished.store(true);
            setPlaybackState(PlaybackState::Stopped);
            return;
        }

        audioCodecContext.reset(avcodec_alloc_context3(audioDecoder));
        if (!audioCodecContext) {
            fail(QStringLiteral("FFmpeg failed to allocate audio decoder context."));
            m_isPlaying.store(false);
            m_decodeFinished.store(true);
            setPlaybackState(PlaybackState::Stopped);
            return;
        }

        result = avcodec_parameters_to_context(audioCodecContext.get(), audioCodecParameters.get());
        if (result < 0) {
            fail(QStringLiteral("FFmpeg failed to configure audio decoder: %1").arg(ffmpegErrorString(result)));
            m_isPlaying.store(false);
            m_decodeFinished.store(true);
            setPlaybackState(PlaybackState::Stopped);
            return;
        }

        result = avcodec_open2(audioCodecContext.get(), audioDecoder, nullptr);
        if (result < 0) {
            fail(QStringLiteral("FFmpeg failed to open audio decoder: %1").arg(ffmpegErrorString(result)));
            m_isPlaying.store(false);
            m_decodeFinished.store(true);
            setPlaybackState(PlaybackState::Stopped);
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
            setPlaybackState(PlaybackState::Stopped);
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
            setPlaybackState(PlaybackState::Stopped);
            return;
        }
        swrContext.reset(rawSwrContext);

        result = swr_init(swrContext.get());
        if (result < 0) {
            fail(QStringLiteral("FFmpeg failed to initialize audio resampler: %1").arg(ffmpegErrorString(result)));
            m_isPlaying.store(false);
            m_decodeFinished.store(true);
            setPlaybackState(PlaybackState::Stopped);
            return;
        }

        audioSink.reset(new QAudioSink(outputDevice, audioFormat));
        audioSink->setBufferSize(qMax<qsizetype>(audioFormat.bytesForDuration(120000),
                                                 audioFormat.bytesPerFrame() * 2048));
        audioSink->setVolume(qBound(0, m_volume.load(), 100) / 100.0);
        audioDevice = audioSink->start();
        if (audioDevice == nullptr) {
            fail(QStringLiteral("Qt failed to start audio output."));
            m_isPlaying.store(false);
            m_decodeFinished.store(true);
            setPlaybackState(PlaybackState::Stopped);
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
        const int targetStreamIndex = localVideoStreamIndex >= 0 ? localVideoStreamIndex : localAudioStreamIndex;
        result = av_seek_frame(m_formatContext,
                               targetStreamIndex,
                               positionToStreamTimestamp(startPosition, targetStreamIndex),
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
        setPlaybackState(PlaybackState::Stopped);
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
        setPlaybackState(PlaybackState::Stopped);
        return;
    }

    const int frameDelayMs = streamFrameDelayMs(videoAvgFrameRate, videoRFrameRate);
    quint64 localSeekGeneration = m_seekGeneration.load();
    qint64 localAudioSeekTargetMs = m_seekTargetMs.load();
    qint64 localVideoSeekTargetMs = localAudioSeekTargetMs;
    int appliedVolume = -1;

    auto updateAudioClock = [&]() {
        if (audioSink && audioClockStarted) {
            // audio clock = 已提交给 QAudioSink 的音频终点 - 仍在声卡缓冲区中的音频时长。
            const qsizetype pendingBytes = qMax<qsizetype>(0, audioSink->bufferSize() - audioSink->bytesFree());
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
            swr_close(swrContext.get());
            swr_init(swrContext.get());
        }
        audioClockStarted = false;
        audioClockBaseUs = boundedPosition(m_positionMs.load()) * 1000;
        audioSubmittedUntilUs = audioClockBaseUs;
        m_audioClockMs.store(audioClockBaseUs / 1000);
        // Reset local sync state before decoded preroll frames are allowed through.
        localSeekGeneration = currentGeneration;
        localAudioSeekTargetMs = m_seekTargetMs.load();
        localVideoSeekTargetMs = localAudioSeekTargetMs;
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
            // 只有真正写入 QAudioSink 的字节才推进已提交音频终点，避免把未播放数据算进 audio clock。
            audioSubmittedUntilUs += audioFormat.durationForBytes(static_cast<qint32>(writtenBytes));
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

            const qint64 framePosition = frameTimestampMs(audioFrame.get(), audioTimeBase, m_positionMs.load());
            if (localAudioSeekTargetMs >= 0
                && framePosition + kSeekPrerollToleranceMs < localAudioSeekTargetMs) {
                av_frame_unref(audioFrame.get());
                continue;
            }
            if (localAudioSeekTargetMs >= 0) {
                localAudioSeekTargetMs = -1;
            }
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

            // 音频作为主时钟；视频只比较自己的 PTS 和 audio clock，不反向拉动音频。
            const qint64 audioClock = updateAudioClock();
            const qint64 diffMs = framePosition - audioClock;

            // 视频落后音频太多时继续显示会造成明显拖影，直接丢帧追上音频。
            if (diffMs < -kVideoDropThresholdMs) {
                return VideoSyncDecision::Drop;
            }

            // 视频快于 audio clock 时等待音频追上；进入容忍窗口后再显示，减少抖动。
            if (diffMs <= kVideoEarlyToleranceMs) {
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

            const qint64 framePosition = frameTimestampMs(videoFrame.get(),
                                                          videoTimeBase,
                                                          m_positionMs.load() + frameDelayMs);
            if (localVideoSeekTargetMs >= 0
                && framePosition + kSeekPrerollToleranceMs < localVideoSeekTargetMs) {
                av_frame_unref(videoFrame.get());
                continue;
            }
            if (localVideoSeekTargetMs >= 0) {
                localVideoSeekTargetMs = -1;
            }

            QImage image;
            QString errorMessage;
            SwsContext* rawSwsContext = swsContext.release();
            const bool converted = convertFrameToImage(videoFrame.get(), &rawSwsContext, &image, &errorMessage);
            swsContext.reset(rawSwsContext);
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
            readVideoPacket = result >= 0 && packet->stream_index == localVideoStreamIndex;
            readAudioPacket = result >= 0 && audioCodecContext && packet->stream_index == localAudioStreamIndex;
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
            const QString streamName = readAudioPacket ? QStringLiteral("audio") : QStringLiteral("video");
            fail(attemptedSendPacket
                     ? QStringLiteral("FFmpeg failed to send %1 packet to decoder: %2")
                           .arg(streamName, ffmpegErrorString(result))
                     : QStringLiteral("FFmpeg failed to read video packet: %1").arg(ffmpegErrorString(result)));
            break;
        }

        if (packet->stream_index != localVideoStreamIndex && packet->stream_index != localAudioStreamIndex) {
            av_packet_unref(packet.get());
            continue;
        }

        const int packetStreamIndex = packet->stream_index;
        av_packet_unref(packet.get());

        if (packetStreamIndex == localAudioStreamIndex) {
            if (!receiveAudioFrames()) {
                break;
            }
            continue;
        }

        if (!receiveVideoFrames()) {
            break;
        }
    }

    audioSink.reset();

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
    setPlaybackState(PlaybackState::Stopped);
    if (reachedEnd) {
        setMediaStatus(MediaStatus::EndOfMedia);
    }
    qDebug() << "[FFmpegPlaybackBackend] decode stopped"
             << "file" << m_filePath
             << "positionMs" << m_positionMs.load()
             << "reachedEnd" << reachedEnd;
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
    m_seekTargetMs.store(-1);
    setMediaStatus(MediaStatus::InvalidMedia);
    setPlaybackState(PlaybackState::Stopped);
    emitErrorOnce(PlaybackError::FormatError, message);
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
    return m_videoStreamIndex >= 0 ? m_videoStreamIndex : m_audioStreamIndex;
}

void FFmpegPlaybackBackend::setPlaybackState(PlaybackState state, bool force)
{
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (!force && m_playbackState == state) {
            return;
        }
        m_playbackState = state;
    }

    emit playbackStateChanged(state);
}

void FFmpegPlaybackBackend::setMediaStatus(MediaStatus status, bool force)
{
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (!force && m_mediaStatus == status) {
            return;
        }
        if (status != MediaStatus::InvalidMedia) {
            m_errorEmittedForCurrentMedia = false;
        }
        m_mediaStatus = status;
    }

    emit mediaStatusChanged(status);
}

void FFmpegPlaybackBackend::emitErrorOnce(PlaybackError error, const QString& message)
{
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (m_errorEmittedForCurrentMedia) {
            return;
        }
        m_errorEmittedForCurrentMedia = true;
    }

    qDebug() << "[FFmpegPlaybackBackend] failure"
             << "category" << failureCategory(message)
             << "file" << m_filePath
             << "positionMs" << m_positionMs.load()
             << "message" << message;
    emit errorOccurred(error, message);
}

#endif // USE_FFMPEG
