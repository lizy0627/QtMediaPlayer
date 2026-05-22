#include "audioplaybackcontroller.h"

#include <QAudioOutput>
#include <QMediaPlayer>
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

AudioPlaybackController::AudioPlaybackController(QObject* parent)
    : QObject(parent)
{
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);
    m_audioOutput->setVolume(0.8);
}

QMediaPlayer* AudioPlaybackController::player() const
{
    return m_player;
}

QAudioOutput* AudioPlaybackController::audioOutput() const
{
    return m_audioOutput;
}

void AudioPlaybackController::open(const QUrl& url)
{
    m_player->setSource(url);
}

void AudioPlaybackController::play()
{
    ensureAudioOutput();
    m_player->play();
}

void AudioPlaybackController::pause()
{
    m_player->pause();
}

void AudioPlaybackController::stop()
{
    m_player->stop();
}

void AudioPlaybackController::toggle()
{
    if (isPlaying()) {
        pause();
    } else {
        play();
    }
}

bool AudioPlaybackController::isPlaying() const
{
    return m_player->playbackState() == QMediaPlayer::PlayingState;
}

void AudioPlaybackController::setPosition(qint64 position)
{
    m_player->setPosition(boundedMediaPosition(m_player, position));
}

void AudioPlaybackController::setVolume(int volume)
{
    m_audioOutput->setVolume(qBound(0, volume, 100) / 100.0);
}

int AudioPlaybackController::volume() const
{
    return static_cast<int>(m_audioOutput->volume() * 100);
}

void AudioPlaybackController::ensureAudioOutput()
{
    if (!m_player->audioOutput()) {
        m_player->setAudioOutput(m_audioOutput);
    }
}
