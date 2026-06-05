#include "qtmediaplaybackbackend.h"

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QUrl>
#include <QVideoWidget>
#include <QtGlobal>

QtMediaPlaybackBackend::QtMediaPlaybackBackend(QObject* parent)
    : IPlaybackBackend(parent)
{
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);

    m_player->setAudioOutput(m_audioOutput);
    m_audioOutput->setVolume(0.5);

    connect(m_player, &QMediaPlayer::positionChanged,
            this, &QtMediaPlaybackBackend::positionChanged);
    connect(m_player, &QMediaPlayer::durationChanged,
            this, &QtMediaPlaybackBackend::durationChanged);
    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this, [this](QMediaPlayer::PlaybackState state) {
                setPlaybackState(toBackendPlaybackState(state));
            });
    connect(m_player, &QMediaPlayer::mediaStatusChanged,
            this, [this](QMediaPlayer::MediaStatus status) {
                setMediaStatus(toBackendMediaStatus(status));
            });
    connect(m_player, &QMediaPlayer::errorOccurred,
            this, [this](QMediaPlayer::Error error, const QString& errorString) {
                if (error != QMediaPlayer::NoError) {
                    setMediaStatus(MediaStatus::InvalidMedia);
                    setPlaybackState(PlaybackState::Stopped);
                    emitErrorOnce(toBackendPlaybackError(error), errorString);
                }
            });
}

QMediaPlayer* QtMediaPlaybackBackend::player() const
{
    return m_player;
}

QAudioOutput* QtMediaPlaybackBackend::audioOutput() const
{
    return m_audioOutput;
}

void QtMediaPlaybackBackend::setVideoOutput(QVideoWidget* videoOutput)
{
    m_player->setVideoOutput(videoOutput);
}

void QtMediaPlaybackBackend::openLocalFile(const QString& filePath)
{
    m_errorEmittedForCurrentMedia = false;
    setMediaStatus(MediaStatus::Loading, true);
    setPlaybackState(PlaybackState::Stopped);
    m_player->setSource(QUrl::fromLocalFile(filePath));
}

void QtMediaPlaybackBackend::openUrl(const QUrl& url)
{
    m_errorEmittedForCurrentMedia = false;
    setMediaStatus(MediaStatus::Loading, true);
    setPlaybackState(PlaybackState::Stopped);
    m_player->setSource(url);
}

void QtMediaPlaybackBackend::play()
{
    if (m_player->source().isEmpty()) {
        setMediaStatus(MediaStatus::InvalidMedia);
        setPlaybackState(PlaybackState::Stopped);
        emitErrorOnce(PlaybackError::ResourceError,
                      QStringLiteral("No media source is loaded."));
        return;
    }

    m_player->play();
    setPlaybackState(PlaybackState::Playing);
}

void QtMediaPlaybackBackend::pause()
{
    m_player->pause();
    setPlaybackState(PlaybackState::Paused);
}

void QtMediaPlaybackBackend::stop()
{
    m_player->stop();
    setPlaybackState(PlaybackState::Stopped);
}

void QtMediaPlaybackBackend::seek(qint64 position)
{
    m_player->setPosition(boundedPosition(position));
}

void QtMediaPlaybackBackend::setVolume(int volume)
{
    m_audioOutput->setVolume(qBound(0, volume, 100) / 100.0);
}

int QtMediaPlaybackBackend::volume() const
{
    return static_cast<int>(m_audioOutput->volume() * 100);
}

void QtMediaPlaybackBackend::setSpeed(double speed)
{
    m_playbackRate = qBound(0.25, speed, 4.0);
    m_player->setPlaybackRate(m_playbackRate);
}

double QtMediaPlaybackBackend::speed() const
{
    return m_playbackRate;
}

qint64 QtMediaPlaybackBackend::position() const
{
    return m_player->position();
}

qint64 QtMediaPlaybackBackend::duration() const
{
    return m_player->duration();
}

IPlaybackBackend::MediaStatus QtMediaPlaybackBackend::mediaStatus() const
{
    return m_mediaStatus;
}

IPlaybackBackend::PlaybackState QtMediaPlaybackBackend::playbackState() const
{
    return m_playbackState;
}

bool QtMediaPlaybackBackend::isPlaying() const
{
    return playbackState() == PlaybackState::Playing;
}

IPlaybackBackend::PlaybackState QtMediaPlaybackBackend::toBackendPlaybackState(int state)
{
    switch (static_cast<QMediaPlayer::PlaybackState>(state)) {
    case QMediaPlayer::StoppedState:
        return PlaybackState::Stopped;
    case QMediaPlayer::PlayingState:
        return PlaybackState::Playing;
    case QMediaPlayer::PausedState:
        return PlaybackState::Paused;
    }

    return PlaybackState::Stopped;
}

IPlaybackBackend::MediaStatus QtMediaPlaybackBackend::toBackendMediaStatus(int status)
{
    switch (static_cast<QMediaPlayer::MediaStatus>(status)) {
    case QMediaPlayer::NoMedia:
        return MediaStatus::NoMedia;
    case QMediaPlayer::LoadingMedia:
        return MediaStatus::Loading;
    case QMediaPlayer::LoadedMedia:
    case QMediaPlayer::BufferingMedia:
    case QMediaPlayer::BufferedMedia:
        return MediaStatus::Loaded;
    case QMediaPlayer::EndOfMedia:
        return MediaStatus::EndOfMedia;
    case QMediaPlayer::InvalidMedia:
        return MediaStatus::InvalidMedia;
    case QMediaPlayer::StalledMedia:
        return MediaStatus::Loading;
    }

    return MediaStatus::InvalidMedia;
}

IPlaybackBackend::PlaybackError QtMediaPlaybackBackend::toBackendPlaybackError(int error)
{
    switch (static_cast<QMediaPlayer::Error>(error)) {
    case QMediaPlayer::NoError:
        return PlaybackError::NoError;
    case QMediaPlayer::ResourceError:
        return PlaybackError::ResourceError;
    case QMediaPlayer::FormatError:
        return PlaybackError::FormatError;
    case QMediaPlayer::NetworkError:
        return PlaybackError::NetworkError;
    case QMediaPlayer::AccessDeniedError:
        return PlaybackError::AccessDeniedError;
    }

    return PlaybackError::UnknownError;
}

void QtMediaPlaybackBackend::setPlaybackState(PlaybackState state, bool force)
{
    if (!force && m_playbackState == state) {
        return;
    }

    m_playbackState = state;
    emit playbackStateChanged(state);
}

void QtMediaPlaybackBackend::setMediaStatus(MediaStatus status, bool force)
{
    if (!force && m_mediaStatus == status) {
        return;
    }

    if (status != MediaStatus::InvalidMedia) {
        m_errorEmittedForCurrentMedia = false;
    }

    m_mediaStatus = status;
    emit mediaStatusChanged(status);
}

void QtMediaPlaybackBackend::emitErrorOnce(PlaybackError error, const QString& message)
{
    if (m_errorEmittedForCurrentMedia) {
        return;
    }

    m_errorEmittedForCurrentMedia = true;
    emit errorOccurred(error, message);
}

qint64 QtMediaPlaybackBackend::boundedPosition(qint64 requestedPosition) const
{
    const qint64 minPosition = 0;
    const qint64 targetPosition = qMax(minPosition, requestedPosition);
    const qint64 mediaDuration = duration();

    if (mediaDuration <= 0) {
        return targetPosition;
    }

    return qBound(minPosition, targetPosition, mediaDuration);
}
