#include "videoplaybackcontroller.h"

#include "playback/qtmediaplaybackbackend.h"

#ifdef USE_FFMPEG
#include "ffmpeg/ffmpegplaybackbackend.h"
#endif

#include <QMediaPlayer>
#include <QMetaType>
#include <QFileInfo>
#include <QUrl>
#include <QVideoWidget>

VideoPlaybackController::VideoPlaybackController(QObject* parent, BackendType backendType)
    : QObject(parent)
{
    qRegisterMetaType<IPlaybackBackend::PlaybackState>("IPlaybackBackend::PlaybackState");
    qRegisterMetaType<IPlaybackBackend::MediaStatus>("IPlaybackBackend::MediaStatus");
    qRegisterMetaType<IPlaybackBackend::PlaybackError>("IPlaybackBackend::PlaybackError");

    m_backend = createBackend(backendType);
    attachBackendSignals();
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
        emit backendTypeChanged(m_backendType);
    }
}

VideoPlaybackController::BackendType VideoPlaybackController::backendType() const
{
    return m_backendType;
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
    const QString trimmedPath = filePath.trimmed();
    if (trimmedPath.isEmpty()) {
        return false;
    }

    const QFileInfo fileInfo(trimmedPath);
    if (fileInfo.exists() || fileInfo.isAbsolute()) {
        return true;
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
    setBackendType(shouldUseFFmpegForLocalFile(filePath)
                       ? BackendType::FFmpeg
                       : BackendType::QtMedia);

    const QUrl url(filePath);
    m_backend->openLocalFile(url.isLocalFile() ? url.toLocalFile() : filePath);
}

void VideoPlaybackController::openUrl(const QUrl& url)
{
    setBackendType(BackendType::QtMedia);
    m_backend->openUrl(url);
}

void VideoPlaybackController::play()
{
    m_backend->play();
}

void VideoPlaybackController::pause()
{
    m_backend->pause();
}

void VideoPlaybackController::stop()
{
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

void VideoPlaybackController::attachBackendSignals()
{
    if (m_backend == nullptr) {
        return;
    }

    connect(m_backend, &IPlaybackBackend::positionChanged,
            this, &VideoPlaybackController::positionChanged);
    connect(m_backend, &IPlaybackBackend::durationChanged,
            this, &VideoPlaybackController::durationChanged);
    connect(m_backend, &IPlaybackBackend::playbackStateChanged,
            this, &VideoPlaybackController::playbackStateChanged);
    connect(m_backend, &IPlaybackBackend::mediaStatusChanged,
            this, &VideoPlaybackController::mediaStatusChanged);
    connect(m_backend, &IPlaybackBackend::errorOccurred,
            this, &VideoPlaybackController::playbackError);
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
