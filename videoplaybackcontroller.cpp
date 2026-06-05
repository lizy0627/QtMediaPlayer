#include "videoplaybackcontroller.h"

#include "playback/qtmediaplaybackbackend.h"

#ifdef USE_FFMPEG
#include "ffmpeg/ffmpegplaybackbackend.h"
#endif

#include <QMediaPlayer>
#include <QMetaType>
#include <QDebug>
#include <QUrl>
#include <QVideoWidget>

namespace {

const char* backendTypeName(VideoPlaybackController::BackendType backendType)
{
    switch (backendType) {
    case VideoPlaybackController::BackendType::QtMedia:
        return "QtMedia";
    case VideoPlaybackController::BackendType::FFmpeg:
        return "FFmpeg";
    }

    return "Unknown";
}

} // namespace

VideoPlaybackController::VideoPlaybackController(QObject* parent, BackendType backendType)
    : QObject(parent)
{
    qRegisterMetaType<IPlaybackBackend::PlaybackState>("IPlaybackBackend::PlaybackState");
    qRegisterMetaType<IPlaybackBackend::MediaStatus>("IPlaybackBackend::MediaStatus");
    qRegisterMetaType<IPlaybackBackend::PlaybackError>("IPlaybackBackend::PlaybackError");

    m_backend = createBackend(backendType);
    attachBackendSignals();
    qDebug() << "[VideoPlaybackController] backend initialized"
             << "backend" << backendTypeName(m_backendType);
}

QMediaPlayer* VideoPlaybackController::player() const
{
    // TODO: Remove this Qt-specific escape hatch once upper layers stop depending on QMediaPlayer.
    QtMediaPlaybackBackend* qtBackend = qobject_cast<QtMediaPlaybackBackend*>(m_backend);
    return qtBackend ? qtBackend->player() : nullptr;
}

QAudioOutput* VideoPlaybackController::audioOutput() const
{
    QtMediaPlaybackBackend* qtBackend = qobject_cast<QtMediaPlaybackBackend*>(m_backend);
    return qtBackend ? qtBackend->audioOutput() : nullptr;
}

void VideoPlaybackController::setBackendType(BackendType backendType)
{
    const BackendType targetBackendType = resolvedBackendType(backendType);
    if (backendType != targetBackendType) {
        qDebug() << "[VideoPlaybackController] backend request resolved"
                 << "requested" << backendTypeName(backendType)
                 << "resolved" << backendTypeName(targetBackendType);
    }

    if (m_backendType == targetBackendType && m_backend != nullptr) {
        return;
    }

    const BackendType previousBackendType = m_backendType;
    const int previousVolume = m_backend ? volume() : 50;
    const double previousSpeed = m_backend ? speed() : 1.0;

    releaseBackend();

    m_backend = createBackend(targetBackendType);
    attachBackendSignals();
    setVideoOutput(m_videoOutput);
    setFrameOutput(m_frameOutput);
    setVolume(previousVolume);
    setSpeed(previousSpeed);

    if (m_backendType != previousBackendType) {
        qDebug() << "[VideoPlaybackController] backend switched"
                 << "from" << backendTypeName(previousBackendType)
                 << "to" << backendTypeName(m_backendType)
                 << "file" << m_currentLocalFilePath;
        emit backendTypeChanged(m_backendType);
    }
}

VideoPlaybackController::BackendType VideoPlaybackController::backendType() const
{
    return m_backendType;
}

void VideoPlaybackController::setLocalFileBackendPolicy(LocalFileBackendPolicy policy)
{
#ifndef USE_FFMPEG
    if (policy == LocalFileBackendPolicy::PreferFFmpeg) {
        policy = LocalFileBackendPolicy::PreferQtMedia;
    }
#endif

    m_localFileBackendPolicy = policy;
}

VideoPlaybackController::LocalFileBackendPolicy VideoPlaybackController::localFileBackendPolicy() const
{
    return m_localFileBackendPolicy;
}

bool VideoPlaybackController::isFFmpegBackendAvailable() const
{
#ifdef USE_FFMPEG
    return true;
#else
    return false;
#endif
}

bool VideoPlaybackController::shouldUseFFmpegForLocalFile(const QString& filePath) const
{
#ifdef USE_FFMPEG
    if (m_localFileBackendPolicy != LocalFileBackendPolicy::PreferFFmpeg) {
        return false;
    }

    const QString trimmedPath = filePath.trimmed();
    if (trimmedPath.isEmpty()) {
        return false;
    }

    const QUrl url(trimmedPath);
    if (url.isValid() && !url.scheme().isEmpty() && !url.isLocalFile()) {
        return false;
    }

    return true;
#else
    Q_UNUSED(filePath)
    return false;
#endif
}

void VideoPlaybackController::setVideoOutput(QVideoWidget* videoOutput)
{
    m_videoOutput = videoOutput;
    m_backend->setVideoOutput(videoOutput);
}

void VideoPlaybackController::setFrameOutput(FFmpegVideoWidget* frameOutput)
{
    m_frameOutput = frameOutput;
    m_backend->setFrameOutput(frameOutput);
}

void VideoPlaybackController::openLocalFile(const QString& filePath)
{
    const QUrl url(filePath);
    m_currentLocalFilePath = url.isLocalFile() ? url.toLocalFile() : filePath;
    m_currentSourceIsLocalFile = true;
    m_ffmpegFallbackAttempted = false;
    m_playbackRequested = false;
    m_fallbackPlaybackRequested = false;
    m_lastKnownPosition = 0;
    m_fallbackPosition = -1;

    setBackendType(shouldUseFFmpegForLocalFile(filePath)
                       ? BackendType::FFmpeg
                       : BackendType::QtMedia);

    qDebug() << "[VideoPlaybackController] openLocalFile"
             << "backend" << backendTypeName(m_backendType)
             << "file" << m_currentLocalFilePath;
    m_backend->openLocalFile(m_currentLocalFilePath);
}

void VideoPlaybackController::openUrl(const QUrl& url)
{
    m_currentSourceIsLocalFile = false;
    m_currentLocalFilePath.clear();
    m_ffmpegFallbackAttempted = false;
    m_playbackRequested = false;
    m_fallbackPlaybackRequested = false;
    m_lastKnownPosition = 0;
    m_fallbackPosition = -1;

    setBackendType(BackendType::QtMedia);
    m_backend->openUrl(url);
}

void VideoPlaybackController::play()
{
    m_playbackRequested = true;
    qDebug() << "[VideoPlaybackController] play"
             << "backend" << backendTypeName(m_backendType)
             << "file" << m_currentLocalFilePath
             << "positionMs" << position();
    m_backend->play();
}

void VideoPlaybackController::pause()
{
    m_playbackRequested = false;
    qDebug() << "[VideoPlaybackController] pause"
             << "backend" << backendTypeName(m_backendType)
             << "file" << m_currentLocalFilePath
             << "positionMs" << position();
    m_backend->pause();
}

void VideoPlaybackController::stop()
{
    m_playbackRequested = false;
    qDebug() << "[VideoPlaybackController] stop"
             << "backend" << backendTypeName(m_backendType)
             << "file" << m_currentLocalFilePath
             << "positionMs" << position();
    m_backend->stop();
}

void VideoPlaybackController::toggle()
{
    if (isPlaying()) {
        pause();
    } else {
        play();
    }
}

void VideoPlaybackController::jump(bool forward, int ms)
{
    const qint64 position = m_backend->position();
    const qint64 targetPosition = forward ? position + ms : position - ms;
    m_backend->seek(targetPosition);
}

void VideoPlaybackController::seek(qint64 position)
{
    qDebug() << "[VideoPlaybackController] seek"
             << "backend" << backendTypeName(m_backendType)
             << "file" << m_currentLocalFilePath
             << "fromMs" << this->position()
             << "toMs" << position;
    m_backend->seek(position);
}

void VideoPlaybackController::setVolume(int volume)
{
    m_backend->setVolume(volume);
}

int VideoPlaybackController::volume() const
{
    return m_backend->volume();
}

void VideoPlaybackController::setSpeed(double speed)
{
    m_backend->setSpeed(speed);
}

double VideoPlaybackController::speed() const
{
    return m_backend->speed();
}

qint64 VideoPlaybackController::position() const
{
    return m_backend->position();
}

qint64 VideoPlaybackController::duration() const
{
    return m_backend->duration();
}

IPlaybackBackend::PlaybackState VideoPlaybackController::playbackState() const
{
    return m_backend->playbackState();
}

IPlaybackBackend::MediaStatus VideoPlaybackController::mediaStatus() const
{
    return m_backend->mediaStatus();
}

bool VideoPlaybackController::isPlaying() const
{
    return m_backend->isPlaying();
}

IPlaybackBackend* VideoPlaybackController::createBackend(BackendType backendType)
{
    backendType = resolvedBackendType(backendType);

#ifdef USE_FFMPEG
    if (backendType == BackendType::FFmpeg) {
        m_backendType = BackendType::FFmpeg;
        return new FFmpegPlaybackBackend(this);
    }
#else
    Q_UNUSED(backendType)
#endif

    m_backendType = BackendType::QtMedia;
    return new QtMediaPlaybackBackend(this);
}

VideoPlaybackController::BackendType VideoPlaybackController::resolvedBackendType(BackendType backendType) const
{
#ifdef USE_FFMPEG
    return backendType;
#else
    Q_UNUSED(backendType)
    return BackendType::QtMedia;
#endif
}

bool VideoPlaybackController::isFFmpegFallbackError(IPlaybackBackend::PlaybackError error) const
{
    return error == IPlaybackBackend::PlaybackError::FormatError
        || error == IPlaybackBackend::PlaybackError::ResourceError;
}

bool VideoPlaybackController::tryFallbackToFFmpeg(IPlaybackBackend::PlaybackError error, const QString& message)
{
    Q_UNUSED(message)

#ifndef USE_FFMPEG
    Q_UNUSED(error)
    return false;
#else
    if (!m_currentSourceIsLocalFile
        || m_currentLocalFilePath.trimmed().isEmpty()
        || m_backendType != BackendType::QtMedia
        || m_ffmpegFallbackAttempted
        || !isFFmpegFallbackError(error)) {
        return false;
    }

    m_ffmpegFallbackAttempted = true;

    const int previousVolume = m_backend ? volume() : 50;
    const double previousSpeed = m_backend ? speed() : 1.0;
    const qint64 fallbackPosition = m_fallbackPosition >= 0
        ? m_fallbackPosition
        : qMax<qint64>(0, m_lastKnownPosition);
    const bool shouldResumePlayback = m_fallbackPlaybackRequested
        || m_playbackRequested
        || (m_backend && m_backend->playbackState() == IPlaybackBackend::PlaybackState::Playing);
    const BackendType previousBackendType = m_backendType;

    releaseBackend();

    m_backend = createBackend(BackendType::FFmpeg);
    attachBackendSignals();
    setVideoOutput(m_videoOutput);
    setFrameOutput(m_frameOutput);
    setVolume(previousVolume);
    setSpeed(previousSpeed);

    if (m_backendType != previousBackendType) {
        qDebug() << "[VideoPlaybackController] backend switched"
                 << "from" << backendTypeName(previousBackendType)
                 << "to" << backendTypeName(m_backendType)
                 << "reason" << "QtMedia fallback error"
                 << "message" << message
                 << "file" << m_currentLocalFilePath
                 << "resumePositionMs" << fallbackPosition
                 << "resumePlayback" << shouldResumePlayback;
        emit backendTypeChanged(m_backendType);
    }

    m_fallbackPlaybackRequested = false;
    m_fallbackPosition = -1;
    m_playbackRequested = shouldResumePlayback;
    m_backend->openLocalFile(m_currentLocalFilePath);
    if (fallbackPosition > 0) {
        m_backend->seek(fallbackPosition);
    }
    if (shouldResumePlayback) {
        m_backend->play();
    }

    return true;
#endif
}

void VideoPlaybackController::attachBackendSignals()
{
    if (m_backend == nullptr) {
        return;
    }

    connect(m_backend, &IPlaybackBackend::positionChanged,
            this, [this](qint64 position) {
                m_lastKnownPosition = position;
                emit positionChanged(position);
            });
    connect(m_backend, &IPlaybackBackend::durationChanged,
            this, &VideoPlaybackController::durationChanged);
    connect(m_backend, &IPlaybackBackend::playbackStateChanged,
            this, &VideoPlaybackController::playbackStateChanged);
    connect(m_backend, &IPlaybackBackend::mediaStatusChanged,
            this, [this](IPlaybackBackend::MediaStatus status) {
                if (status == IPlaybackBackend::MediaStatus::InvalidMedia
                    && m_currentSourceIsLocalFile
                    && m_backendType == BackendType::QtMedia
                    && !m_ffmpegFallbackAttempted) {
                    m_fallbackPosition = qMax(m_lastKnownPosition,
                                              m_backend ? m_backend->position() : qint64(0));
                    m_fallbackPlaybackRequested =
                        m_playbackRequested
                        || (m_backend
                            && m_backend->playbackState() == IPlaybackBackend::PlaybackState::Playing);
                }
                emit mediaStatusChanged(status);
            });
    connect(m_backend, &IPlaybackBackend::errorOccurred,
            this, [this](IPlaybackBackend::PlaybackError error, const QString& message) {
                if (tryFallbackToFFmpeg(error, message)) {
                    return;
                }
                emit playbackError(error, message);
            });
}

void VideoPlaybackController::releaseBackend()
{
    if (m_backend == nullptr) {
        return;
    }

    IPlaybackBackend* oldBackend = m_backend;
    m_backend = nullptr;

    disconnect(oldBackend, nullptr, nullptr, nullptr);
    oldBackend->stop();
    oldBackend->deleteLater();
}
