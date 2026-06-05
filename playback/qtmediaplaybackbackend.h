#ifndef QTMEDIAPLAYBACKBACKEND_H
#define QTMEDIAPLAYBACKBACKEND_H

#include "iplaybackbackend.h"

class QAudioOutput;
class QMediaPlayer;
class QVideoWidget;

class QtMediaPlaybackBackend : public IPlaybackBackend
{
    Q_OBJECT

public:
    explicit QtMediaPlaybackBackend(QObject* parent = nullptr);

    QMediaPlayer* player() const;
    QAudioOutput* audioOutput() const;

    void setVideoOutput(QVideoWidget* videoOutput) override;
    void openLocalFile(const QString& filePath) override;
    void openUrl(const QUrl& url) override;
    void play() override;
    void pause() override;
    void stop() override;
    void seek(qint64 position) override;
    void setVolume(int volume) override;
    int volume() const override;
    void setSpeed(double speed) override;
    double speed() const override;
    qint64 position() const override;
    qint64 duration() const override;
    PlaybackState playbackState() const override;
    MediaStatus mediaStatus() const override;
    bool isPlaying() const override;

private:
    static PlaybackState toBackendPlaybackState(int state);
    static MediaStatus toBackendMediaStatus(int status);
    static PlaybackError toBackendPlaybackError(int error);
    void setPlaybackState(PlaybackState state, bool force = false);
    void setMediaStatus(MediaStatus status, bool force = false);
    void emitErrorOnce(PlaybackError error, const QString& message);
    qint64 boundedPosition(qint64 requestedPosition) const;

    QMediaPlayer* m_player = nullptr;
    QAudioOutput* m_audioOutput = nullptr;
    double m_playbackRate = 1.0;
    PlaybackState m_playbackState = PlaybackState::Stopped;
    MediaStatus m_mediaStatus = MediaStatus::NoMedia;
    bool m_errorEmittedForCurrentMedia = false;
};

#endif // QTMEDIAPLAYBACKBACKEND_H
