#ifndef VIDEOPLAYERCONTROLLER_H
#define VIDEOPLAYERCONTROLLER_H

#include <QObject>
#include <QMediaPlayer>
#include <QString>
#include <QStringList>

#include "onlinevideotypes.h"

class CaptureService;
class DanmakuController;
class MediaHistoryService;
class OnlineVideoCoordinator;
class OnlineVideoService;
struct OnlinePlaybackRequest;
class QTimer;
class UserSession;
class VideoCaptureCoordinator;
class VideoDanmakuCoordinator;
class VideoHistoryCoordinator;
class VideoPlaybackController;
class QWidget;

class VideoPlayerController : public QObject
{
    Q_OBJECT

public:
    explicit VideoPlayerController(QWidget* viewParent,
                                   VideoPlaybackController* playbackController,
                                   MediaHistoryService* historyService,
                                   CaptureService* captureService,
                                   DanmakuController* danmakuController,
                                   OnlineVideoService* onlineVideoService,
                                   UserSession* userSession,
                                   QObject* parent = nullptr);

    bool open(const QString& filePath, bool localFile = true);
    bool openQueue(const QStringList& filePaths);
    void openAtPosition(const QString& filePath, qint64 position);
    void togglePlayback();
    void jump(bool forward, int ms = 5000);
    void seekToPosition(qint64 position);
    void setVolume(int volume);
    int volume() const;
    bool isPlaying() const;
    void setSpeed(double speed = 1.0);
    double speed() const;
    void pause();
    void play();

    qint64 position() const;
    qint64 duration() const;
    QString currentVideoPath() const;
    QStringList videoQueue() const;
    int currentVideoQueueIndex() const;

    void showHistoryDialog(QWidget* parent);
    void showMyDanmakuRecords(QWidget* parent);
    void showOnlineSearchDialog(QWidget* parent);
    void requestScreenshot();
    void toggleRecording();
    void toggleDanmaku();
    void sendDanmaku(const QString& content, const QString& color, int type);
    void playOnlineVideo(const VideoInfo& video);
    bool playQueuedVideo(int index);
    bool removeQueuedVideo(int index);

signals:
    void playbackStateChanged(bool playing);
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void volumeChanged(int volume);
    void speedChanged(double speed);
    void danmakuEnabledChanged(bool enabled);
    void playbackError(QString message);
    void warningRequested(QString title, QString message);
    void infoRequested(QString title, QString message);
    void videoQueueChanged(QStringList filePaths, int currentIndex);
    void previewVideoPathChanged(QString filePath);

private slots:
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onDurationChanged(qint64 duration);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onPlayerError(QMediaPlayer::Error error, const QString& errorString);
    void onOnlinePlaybackResolved(const OnlinePlaybackRequest& request);
    void onOnlinePlaybackResolveFailed(const QString& message);
    void saveCurrentProgress();

private:
    enum class PendingSeekMode {
        None,
        Resume,
        Exact
    };

    void checkAndRestoreProgress(const QString& filePath);
    void clearPendingSeek();
    void checkAndRestoreProgressSilently(const QString& filePath);
    qint64 resolvedPendingSeekPosition(qint64 requestedPosition, PendingSeekMode mode) const;
    void schedulePendingSeek(qint64 positionValue,
                             PendingSeekMode mode,
                             qint64 highlightPosition = -1);
    void tryApplyPendingSeek(QMediaPlayer::MediaStatus status);
    void clearLocalVideoStateForOnlinePlayback();
    void clearVideoQueue();
    void emitVideoQueueChanged();
    bool openQueuedVideo(int index, bool restoreProgressSilently = false);
    bool playNextQueuedVideo();
    bool retryCurrentOnlineVideoIfFailed();

    VideoPlaybackController* m_playbackController = nullptr;
    VideoHistoryCoordinator* m_historyCoordinator = nullptr;
    VideoCaptureCoordinator* m_captureCoordinator = nullptr;
    VideoDanmakuCoordinator* m_danmakuCoordinator = nullptr;
    OnlineVideoCoordinator* m_onlineCoordinator = nullptr;
    QTimer* m_saveTimer = nullptr;
    QString m_currentVideoPath;
    bool m_skipNextRestorePrompt = false;
    qint64 m_pendingSeekPosition = -1;
    qint64 m_pendingHighlightPosition = -1;
    PendingSeekMode m_pendingSeekMode = PendingSeekMode::None;
    QStringList m_videoQueue;
    int m_videoQueueIndex = -1;
    bool m_openingVideoQueueItem = false;
    bool m_silentRestoreQueueItem = false;
    bool m_currentOnlinePlaybackFailed = false;
};

#endif // VIDEOPLAYERCONTROLLER_H
