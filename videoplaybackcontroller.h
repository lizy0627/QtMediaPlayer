#ifndef VIDEOPLAYBACKCONTROLLER_H
#define VIDEOPLAYBACKCONTROLLER_H

#include <QObject>
#include <QString>

#include "playback/iplaybackbackend.h"

class QAudioOutput;
class QMediaPlayer;
class QUrl;
class QVideoWidget;
class FFmpegVideoWidget;

class VideoPlaybackController : public QObject
{
    Q_OBJECT

public:
    enum class BackendType {
        QtMedia,
        FFmpeg
    };
    Q_ENUM(BackendType)

    enum class LocalFileBackendPolicy {
        PreferQtMedia,
        PreferFFmpeg
    };
    Q_ENUM(LocalFileBackendPolicy)

    explicit VideoPlaybackController(QObject* parent = nullptr,
                                     BackendType backendType = BackendType::QtMedia);

    QMediaPlayer* player() const;
    QAudioOutput* audioOutput() const;

    void setBackendType(BackendType backendType);
    BackendType backendType() const;
    void setLocalFileBackendPolicy(LocalFileBackendPolicy policy);
    LocalFileBackendPolicy localFileBackendPolicy() const;
    bool isFFmpegBackendAvailable() const;
    bool shouldUseFFmpegForLocalFile(const QString& filePath) const;

    void setVideoOutput(QVideoWidget* videoOutput);
    void setFrameOutput(FFmpegVideoWidget* frameOutput);
    void openLocalFile(const QString& filePath);
    void openUrl(const QUrl& url);
    void play();
    void pause();
    void stop();
    void toggle();
    void jump(bool forward, int ms = 5000);
    void seek(qint64 position);

    void setVolume(int volume);
    int volume() const;

    void setSpeed(double speed = 1.0);
    double speed() const;
    qint64 position() const;
    qint64 duration() const;
    IPlaybackBackend::PlaybackState playbackState() const;
    IPlaybackBackend::MediaStatus mediaStatus() const;
    bool isPlaying() const;

signals:
    void backendTypeChanged(VideoPlaybackController::BackendType backendType);
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void playbackStateChanged(IPlaybackBackend::PlaybackState state);
    void mediaStatusChanged(IPlaybackBackend::MediaStatus status);
    void playbackError(IPlaybackBackend::PlaybackError error, QString message);

private:
    IPlaybackBackend* createBackend(BackendType backendType);
    BackendType resolvedBackendType(BackendType backendType) const;
    bool isFFmpegFallbackError(IPlaybackBackend::PlaybackError error) const;
    bool tryFallbackToFFmpeg(IPlaybackBackend::PlaybackError error, const QString& message);
    void attachBackendSignals();
    void releaseBackend();

    IPlaybackBackend* m_backend = nullptr;
    BackendType m_backendType = BackendType::QtMedia;
    LocalFileBackendPolicy m_localFileBackendPolicy = LocalFileBackendPolicy::PreferQtMedia;
    QVideoWidget* m_videoOutput = nullptr;
    FFmpegVideoWidget* m_frameOutput = nullptr;
    QString m_currentLocalFilePath;
    bool m_currentSourceIsLocalFile = false;
    bool m_ffmpegFallbackAttempted = false;
    bool m_playbackRequested = false;
    bool m_fallbackPlaybackRequested = false;
    qint64 m_lastKnownPosition = 0;
    qint64 m_fallbackPosition = -1;
};

#endif // VIDEOPLAYBACKCONTROLLER_H
