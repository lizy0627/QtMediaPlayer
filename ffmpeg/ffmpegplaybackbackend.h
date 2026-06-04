#ifndef FFMPEGPLAYBACKBACKEND_H
#define FFMPEGPLAYBACKBACKEND_H

#ifdef USE_FFMPEG

#include "playback/iplaybackbackend.h"

#include <QImage>
#include <QMetaObject>
#include <QtGlobal>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

struct AVFormatContext;
struct AVCodecContext;

class QVideoWidget;
class FFmpegVideoWidget;

class FFmpegPlaybackBackend : public IPlaybackBackend
{
    Q_OBJECT

public:
    explicit FFmpegPlaybackBackend(QObject* parent = nullptr);
    ~FFmpegPlaybackBackend() override;

    void setVideoOutput(QVideoWidget* videoOutput) override;
    void setFrameOutput(FFmpegVideoWidget* frameOutput) override;
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

signals:
    void videoFrameReady(const QImage& image);

private:
    void startDecodeThread();
    void stopDecodeThread();
    void cleanupFinishedThread();
    void decodeLoop();
    bool waitUntilRunnable();
    bool waitFrameInterval(int delayMs);
    void closeInput();
    bool validateLocalFile(const QString& filePath, QString* absoluteFilePath);
    bool fail(const QString& message);
    qint64 boundedPosition(qint64 requestedPosition) const;
    qint64 positionToStreamTimestamp(qint64 positionMs, int streamIndex) const;
    int seekStreamIndex() const;
    void setMediaStatus(MediaStatus status);

    AVFormatContext* m_formatContext = nullptr;
    AVCodecContext* m_videoCodecContext = nullptr;
    AVCodecContext* m_audioCodecContext = nullptr;
    FFmpegVideoWidget* m_frameOutput = nullptr;
    QMetaObject::Connection m_frameConnection;
    QString m_filePath;
    int m_videoStreamIndex = -1;
    int m_audioStreamIndex = -1;
    qint64 m_durationMs = 0;
    std::atomic<qint64> m_positionMs { 0 };
    std::atomic<qint64> m_audioClockMs { 0 };
    std::atomic<int> m_volume { 50 };
    std::atomic<double> m_speed { 1.0 };

    std::thread m_decodeThread;
    mutable std::mutex m_stateMutex;
    std::mutex m_decodeIoMutex;
    std::condition_variable m_stateChanged;
    bool m_stopRequested = false;
    bool m_pauseRequested = false;
    MediaStatus m_mediaStatus = MediaStatus::NoMedia;
    std::atomic_bool m_decodeFinished { false };
    std::atomic_bool m_isPlaying { false };
    std::atomic<quint64> m_seekGeneration { 0 };
};

#endif // USE_FFMPEG

#endif // FFMPEGPLAYBACKBACKEND_H
