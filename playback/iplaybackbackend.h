#ifndef IPLAYBACKBACKEND_H
#define IPLAYBACKBACKEND_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QtGlobal>

class QVideoWidget;
class FFmpegVideoWidget;

class IPlaybackBackend : public QObject
{
    Q_OBJECT

public:
    enum class PlaybackState {
        Stopped,
        Playing,
        Paused
    };
    Q_ENUM(PlaybackState)

    enum class MediaStatus {
        NoMedia,
        Loading,
        Loaded,
        Buffered,
        EndOfMedia,
        InvalidMedia
    };
    Q_ENUM(MediaStatus)

    enum class PlaybackError {
        NoError,
        ResourceError,
        FormatError,
        NetworkError,
        AccessDeniedError,
        UnknownError
    };
    Q_ENUM(PlaybackError)

    explicit IPlaybackBackend(QObject* parent = nullptr)
        : QObject(parent)
    {
    }
    ~IPlaybackBackend() override = default;

    virtual void setVideoOutput(QVideoWidget* videoOutput) = 0;
    virtual void setFrameOutput(FFmpegVideoWidget* frameOutput)
    {
        Q_UNUSED(frameOutput)
    }
    virtual void openLocalFile(const QString& filePath) = 0;
    virtual void openUrl(const QUrl& url) = 0;
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual void seek(qint64 position) = 0;
    virtual void setVolume(int volume) = 0;
    virtual int volume() const = 0;
    virtual void setSpeed(double speed) = 0;
    virtual double speed() const = 0;
    virtual qint64 position() const = 0;
    virtual qint64 duration() const = 0;
    virtual PlaybackState playbackState() const = 0;
    virtual MediaStatus mediaStatus() const = 0;
    virtual bool isPlaying() const = 0;

signals:
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void playbackStateChanged(IPlaybackBackend::PlaybackState state);
    void mediaStatusChanged(IPlaybackBackend::MediaStatus status);
    void errorOccurred(IPlaybackBackend::PlaybackError error, QString message);
};

#endif // IPLAYBACKBACKEND_H
