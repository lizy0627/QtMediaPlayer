#include "videoplaybackcontroller.h"

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QUrl>
#include <QVideoWidget>
#include <QtGlobal>

namespace {
qint64 boundedMediaPosition(const QMediaPlayer* player, qint64 requestedPosition)
{
    const qint64 minPosition = 0;
    const qint64 targetPosition = qMax(minPosition, requestedPosition);
    const qint64 duration = player ? player->duration() : qint64(0);

    if (duration <= 0) {
        return targetPosition;
    }

    return qBound(minPosition, targetPosition, duration);
}
}

VideoPlaybackController::VideoPlaybackController(QObject* parent)
    : QObject(parent)
{
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);

    m_player->setAudioOutput(m_audioOutput);
    m_audioOutput->setVolume(0.5);
}

QMediaPlayer* VideoPlaybackController::player() const
{
    return m_player;
}

QAudioOutput* VideoPlaybackController::audioOutput() const
{
    return m_audioOutput;
}

void VideoPlaybackController::setVideoOutput(QVideoWidget* videoOutput)
{
    m_player->setVideoOutput(videoOutput);
}

void VideoPlaybackController::openLocalFile(const QString& filePath)
{
    m_player->setSource(QUrl::fromLocalFile(filePath));
}

void VideoPlaybackController::openUrl(const QUrl& url)
{
    m_player->setSource(url);
}

void VideoPlaybackController::play()
{
    m_player->play();
}

void VideoPlaybackController::pause()
{
    m_player->pause();
}

void VideoPlaybackController::stop()
{
    m_player->stop();
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
    const qint64 position = m_player->position();
    const qint64 targetPosition = forward ? position + ms : position - ms;
    m_player->setPosition(boundedMediaPosition(m_player, targetPosition));
}

void VideoPlaybackController::setVolume(int volume)
{
    m_audioOutput->setVolume(qBound(0, volume, 100) / 100.0);
}

int VideoPlaybackController::volume() const
{
    return static_cast<int>(m_audioOutput->volume() * 100);
}

void VideoPlaybackController::setSpeed(double speed)
{
    m_playbackRate = qBound(0.25, speed, 4.0);
    m_player->setPlaybackRate(m_playbackRate);
}

double VideoPlaybackController::speed() const
{
    return m_playbackRate;
}

bool VideoPlaybackController::isPlaying() const
{
    return m_player->playbackState() == QMediaPlayer::PlayingState;
}
