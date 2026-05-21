#include "videoplayercontroller.h"

#include <QTimer>
#include <QUrl>

#include "mediahistory.h"
#include "mediaprobeservice.h"
#include "onlinevideocoordinator.h"
#include "onlinevideoservice.h"
#include "videocapturecoordinator.h"
#include "videodanmakucoordinator.h"
#include "videohistorycoordinator.h"
#include "videoplaybackcontroller.h"

namespace {
constexpr qint64 kResumeNearEndThresholdMs = 5000;

bool isReadyForDeferredSeek(QMediaPlayer::MediaStatus status)
{
    return status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia;
}

QString mediaProbeWarningMessage(const ProbeResult& result)
{
    return QStringLiteral("\u4e0d\u652f\u6301\u64ad\u653e\u8be5\u6587\u4ef6\u3002\n\n"
                          "\u5177\u4f53\u539f\u56e0\uff1a\n%1\n\n"
                          "\u652f\u6301\u7684\u97f3\u9891\u683c\u5f0f\uff1a\n%2\n\n"
                          "\u652f\u6301\u7684\u89c6\u9891\u683c\u5f0f\uff1a\n%3")
        .arg(result.reason,
             result.supportedAudioFormats.join(QStringLiteral(", ")),
             result.supportedVideoFormats.join(QStringLiteral(", ")));
}
}

VideoPlayerController::VideoPlayerController(QWidget* viewParent,
                                             VideoPlaybackController* playbackController,
                                             MediaHistoryService* historyService,
                                             CaptureService* captureService,
                                             DanmakuController* danmakuController,
                                             OnlineVideoService* onlineVideoService,
                                             UserSession* userSession,
                                             QObject* parent)
    : QObject(parent)
    , m_playbackController(playbackController)
    , m_historyCoordinator(new VideoHistoryCoordinator(historyService, viewParent, this))
    , m_captureCoordinator(new VideoCaptureCoordinator(captureService, this))
    , m_danmakuCoordinator(new VideoDanmakuCoordinator(danmakuController, userSession, this))
    , m_onlineCoordinator(new OnlineVideoCoordinator(onlineVideoService, this))
    , m_saveTimer(new QTimer(this))
{
    m_saveTimer->setInterval(5000);
    connect(m_saveTimer, &QTimer::timeout, this, &VideoPlayerController::saveCurrentProgress);
    connect(m_historyCoordinator,
            &VideoHistoryCoordinator::playHistoryRequested,
            this,
            [this](const QString& filePath, qint64 savedPosition) {
                m_skipNextRestorePrompt = true;
                open(filePath, true);
                if (savedPosition > 0) {
                    schedulePendingSeek(savedPosition, PendingSeekMode::Resume);
                }
            });
    connect(m_captureCoordinator,
            &VideoCaptureCoordinator::warningRequested,
            this,
            &VideoPlayerController::warningRequested);
    connect(m_captureCoordinator,
            &VideoCaptureCoordinator::infoRequested,
            this,
            &VideoPlayerController::infoRequested);
    connect(m_captureCoordinator,
            &VideoCaptureCoordinator::recordingErrorRequested,
            this,
            &VideoPlayerController::warningRequested);
    connect(m_danmakuCoordinator,
            &VideoDanmakuCoordinator::danmakuEnabledChanged,
            this,
            &VideoPlayerController::danmakuEnabledChanged);
    connect(m_danmakuCoordinator,
            &VideoDanmakuCoordinator::warningRequested,
            this,
            &VideoPlayerController::warningRequested);
    connect(m_danmakuCoordinator,
            &VideoDanmakuCoordinator::infoRequested,
            this,
            &VideoPlayerController::infoRequested);
    connect(m_danmakuCoordinator,
            &VideoDanmakuCoordinator::locateRequested,
            this,
            [this](const QString& videoPath, qint64 timestamp) {
                if (videoPath.isEmpty()) {
                    return;
                }

                if (videoPath != m_currentVideoPath) {
                    m_skipNextRestorePrompt = true;
                    open(videoPath, true);
                    schedulePendingSeek(timestamp, PendingSeekMode::Exact, timestamp);
                    return;
                }

                seekToPosition(timestamp);
                if (m_danmakuCoordinator) {
                    m_danmakuCoordinator->highlight(timestamp);
                }
            });
    connect(m_onlineCoordinator,
            &OnlineVideoCoordinator::playbackResolved,
            this,
            &VideoPlayerController::onOnlinePlaybackResolved);
    connect(m_onlineCoordinator,
            &OnlineVideoCoordinator::playbackResolveFailed,
            this,
            &VideoPlayerController::onOnlinePlaybackResolveFailed);

    QMediaPlayer* player = m_playbackController ? m_playbackController->player() : nullptr;
    if (!player) {
        return;
    }

    connect(player, &QMediaPlayer::positionChanged, this, &VideoPlayerController::positionChanged);
    connect(player, &QMediaPlayer::positionChanged, this, [this](qint64 position) {
        if (m_danmakuCoordinator) {
            m_danmakuCoordinator->syncToPosition(position);
        }
    });
    connect(player, &QMediaPlayer::durationChanged, this, &VideoPlayerController::onDurationChanged);
    connect(player,
            &QMediaPlayer::playbackStateChanged,
            this,
            &VideoPlayerController::onPlaybackStateChanged);
    connect(player,
            &QMediaPlayer::mediaStatusChanged,
            this,
            &VideoPlayerController::onMediaStatusChanged);
    connect(player, &QMediaPlayer::errorOccurred, this, &VideoPlayerController::onPlayerError);

}

bool VideoPlayerController::open(const QString& filePath, bool localFile)
{
    if (!m_playbackController || filePath.isEmpty()) {
        return false;
    }

    if (localFile) {
        const ProbeResult probeResult = MediaProbeService::probeLocalFile(filePath);
        if (probeResult.status != ProbeStatus::Supported) {
            emit warningRequested(QStringLiteral("\u4e0d\u652f\u6301\u64ad\u653e\u8be5\u6587\u4ef6"),
                                  mediaProbeWarningMessage(probeResult));
            return false;
        }
    }

    clearPendingSeek();
    saveCurrentProgress();

    if (localFile) {
        m_currentVideoPath = filePath;
        if (m_historyCoordinator) {
            m_historyCoordinator->setCurrentVideo(filePath);
        }
        m_playbackController->openLocalFile(filePath);
        if (m_historyCoordinator) {
            m_historyCoordinator->maybeMarkPlaybackStarted(duration());
        }

        if (!m_skipNextRestorePrompt) {
            checkAndRestoreProgress(filePath);
        }
        m_skipNextRestorePrompt = false;

        if (m_danmakuCoordinator) {
            m_danmakuCoordinator->loadVideo(filePath);
        }
    } else {
        clearLocalVideoStateForOnlinePlayback();
        m_playbackController->openUrl(QUrl(filePath));
    }

    m_playbackController->play();
    return true;
}

void VideoPlayerController::openAtPosition(const QString& filePath, qint64 position)
{
    if (filePath.isEmpty()) {
        return;
    }

    m_skipNextRestorePrompt = true;
    if (!open(filePath, true)) {
        return;
    }
    if (position > 0) {
        schedulePendingSeek(position, PendingSeekMode::Resume);
    }
}

void VideoPlayerController::togglePlayback()
{
    if (m_playbackController) {
        m_playbackController->toggle();
    }
}

void VideoPlayerController::jump(bool forward, int ms)
{
    if (m_playbackController) {
        m_playbackController->jump(forward, ms);
    }
}

void VideoPlayerController::seekToPosition(qint64 positionValue)
{
    QMediaPlayer* player = m_playbackController ? m_playbackController->player() : nullptr;
    if (!player) {
        return;
    }

    player->setPosition(positionValue);
    emit positionChanged(positionValue);

    if (m_danmakuCoordinator) {
        m_danmakuCoordinator->syncAfterSeek(positionValue);
    }
}

void VideoPlayerController::setVolume(int volumeValue)
{
    if (!m_playbackController) {
        return;
    }

    m_playbackController->setVolume(volumeValue);
    emit volumeChanged(m_playbackController->volume());
}

int VideoPlayerController::volume() const
{
    return m_playbackController ? m_playbackController->volume() : 0;
}

bool VideoPlayerController::isPlaying() const
{
    return m_playbackController && m_playbackController->isPlaying();
}

void VideoPlayerController::setSpeed(double speedValue)
{
    if (!m_playbackController) {
        return;
    }

    m_playbackController->setSpeed(speedValue);
    emit speedChanged(m_playbackController->speed());
}

double VideoPlayerController::speed() const
{
    return m_playbackController ? m_playbackController->speed() : 1.0;
}

void VideoPlayerController::pause()
{
    if (m_playbackController) {
        m_playbackController->pause();
    }
}

void VideoPlayerController::play()
{
    if (m_playbackController) {
        m_playbackController->play();
    }
}

qint64 VideoPlayerController::position() const
{
    const QMediaPlayer* player = m_playbackController ? m_playbackController->player() : nullptr;
    return player ? player->position() : 0;
}

qint64 VideoPlayerController::duration() const
{
    const QMediaPlayer* player = m_playbackController ? m_playbackController->player() : nullptr;
    return player ? player->duration() : 0;
}

QString VideoPlayerController::currentVideoPath() const
{
    return m_currentVideoPath;
}

void VideoPlayerController::showHistoryDialog(QWidget* parent)
{
    if (m_historyCoordinator) {
        m_historyCoordinator->showHistoryDialog(parent);
    }
}

void VideoPlayerController::showMyDanmakuRecords(QWidget* parent)
{
    if (m_danmakuCoordinator) {
        m_danmakuCoordinator->showMyDanmakuRecords(parent, m_currentVideoPath);
    }
}

void VideoPlayerController::showOnlineSearchDialog(QWidget* parent)
{
    if (m_onlineCoordinator) {
        m_onlineCoordinator->showSearchDialog(parent);
    }
}

void VideoPlayerController::requestScreenshot()
{
    if (m_captureCoordinator) {
        m_captureCoordinator->requestScreenshot(isPlaying());
    }
}

void VideoPlayerController::toggleRecording()
{
    if (m_captureCoordinator) {
        m_captureCoordinator->toggleRecording(isPlaying());
    }
}

void VideoPlayerController::toggleDanmaku()
{
    if (m_danmakuCoordinator) {
        m_danmakuCoordinator->toggleDanmaku(position());
    }
}

void VideoPlayerController::sendDanmaku(const QString& content, const QString& color, int type)
{
    if (m_danmakuCoordinator) {
        m_danmakuCoordinator->sendDanmaku(content, color, type, position());
    }
}

void VideoPlayerController::playOnlineVideo(const VideoInfo& video)
{
    if (m_onlineCoordinator && m_playbackController) {
        m_onlineCoordinator->playOnlineVideo(video);
    }
}

void VideoPlayerController::onOnlinePlaybackResolved(const OnlinePlaybackRequest& request)
{
    if (!m_onlineCoordinator || !m_playbackController) {
        return;
    }

    if (request.resolution == PlaybackResolution::BrowserOnly) {
        emit warningRequested(QStringLiteral("只能浏览器打开"), request.errorMessage);
        return;
    }

    if (request.resolution != PlaybackResolution::DirectPlayable
        || !request.valid
        || !request.mediaUrl.isValid()) {
        emit warningRequested(QStringLiteral("错误"), request.errorMessage);
        return;
    }

    m_playbackController->stop();
    clearLocalVideoStateForOnlinePlayback();
    m_playbackController->openUrl(request.mediaUrl);
    m_playbackController->play();
    emit infoRequested(QStringLiteral("开始播放"),
                       m_onlineCoordinator->playbackStartedMessage(request));
}

void VideoPlayerController::onOnlinePlaybackResolveFailed(const QString& message)
{
    emit warningRequested(QStringLiteral("错误"), message);
}

void VideoPlayerController::onPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    const bool playing = state == QMediaPlayer::PlayingState;
    emit playbackStateChanged(playing);

    if (playing) {
        m_saveTimer->start();
        if (m_danmakuCoordinator) {
            m_danmakuCoordinator->onPlaybackStarted(position());
        }
        return;
    }

    if (state == QMediaPlayer::StoppedState) {
        m_saveTimer->stop();
        if (!m_historyCoordinator || !m_historyCoordinator->completionSaved()) {
            saveCurrentProgress();
        }
        if (m_historyCoordinator) {
            m_historyCoordinator->setCompletionSaved(false);
        }
        if (m_danmakuCoordinator) {
            m_danmakuCoordinator->onPlaybackStopped();
        }
        return;
    }

    if (state == QMediaPlayer::PausedState) {
        m_saveTimer->stop();
        if (m_danmakuCoordinator) {
            m_danmakuCoordinator->onPlaybackPaused();
        }
    }
}

void VideoPlayerController::onDurationChanged(qint64 durationValue)
{
    emit durationChanged(durationValue);
    if (m_historyCoordinator) {
        m_historyCoordinator->maybeMarkPlaybackStarted(durationValue);
    }
}

void VideoPlayerController::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (isReadyForDeferredSeek(status)) {
        tryApplyPendingSeek(status);
    }

    if (status == QMediaPlayer::EndOfMedia) {
        m_saveTimer->stop();
        if (m_historyCoordinator) {
            m_historyCoordinator->saveCompletedProgress(position(), duration());
            m_historyCoordinator->setCompletionSaved(true);
        }
        return;
    }

    if (status == QMediaPlayer::InvalidMedia) {
        emit playbackError(QStringLiteral("无法加载当前视频，请检查文件或网络地址是否有效。"));
    }
}

void VideoPlayerController::onPlayerError(QMediaPlayer::Error error, const QString& errorString)
{
    if (error == QMediaPlayer::NoError) {
        return;
    }

    QString errorMessage;
    switch (error) {
    case QMediaPlayer::ResourceError:
        errorMessage = QStringLiteral("资源错误：无法打开视频。");
        break;
    case QMediaPlayer::FormatError:
        errorMessage = QStringLiteral("格式错误：当前视频格式不受支持。");
        break;
    case QMediaPlayer::NetworkError:
        errorMessage = QStringLiteral("网络错误：无法访问远程媒体资源。");
        break;
    case QMediaPlayer::AccessDeniedError:
        errorMessage = QStringLiteral("访问被拒绝：当前媒体没有访问权限。");
        break;
    default:
        errorMessage = QStringLiteral("播放器发生未知错误。");
        break;
    }

    if (!errorString.trimmed().isEmpty()) {
        errorMessage += QLatin1Char('\n') + errorString.trimmed();
    }

    emit playbackError(errorMessage);
}

void VideoPlayerController::saveCurrentProgress()
{
    if (!m_historyCoordinator || !m_playbackController) {
        return;
    }

    m_historyCoordinator->saveCurrentProgress(position(), duration());
}

void VideoPlayerController::checkAndRestoreProgress(const QString& filePath)
{
    if (!m_historyCoordinator) {
        return;
    }

    m_historyCoordinator->restoreProgressIfNeeded(this, filePath, [this](qint64 savedPosition) {
        schedulePendingSeek(savedPosition, PendingSeekMode::Resume);
    });
}

void VideoPlayerController::clearPendingSeek()
{
    m_pendingSeekPosition = -1;
    m_pendingHighlightPosition = -1;
    m_pendingSeekMode = PendingSeekMode::None;
}

qint64 VideoPlayerController::resolvedPendingSeekPosition(qint64 requestedPosition,
                                                          PendingSeekMode mode) const
{
    const qint64 durationValue = duration();
    qint64 targetPosition = qMax<qint64>(0, requestedPosition);

    if (durationValue <= 0) {
        return targetPosition;
    }

    if (mode == PendingSeekMode::Resume
        && targetPosition >= qMax<qint64>(0, durationValue - kResumeNearEndThresholdMs)) {
        return 0;
    }

    return qBound<qint64>(0, targetPosition, qMax<qint64>(0, durationValue - 1));
}

void VideoPlayerController::schedulePendingSeek(qint64 positionValue,
                                                PendingSeekMode mode,
                                                qint64 highlightPosition)
{
    if (positionValue < 0 || mode == PendingSeekMode::None) {
        clearPendingSeek();
        return;
    }

    m_pendingSeekPosition = positionValue;
    m_pendingHighlightPosition = highlightPosition;
    m_pendingSeekMode = mode;

    const QMediaPlayer* player = m_playbackController ? m_playbackController->player() : nullptr;
    if (!player) {
        return;
    }

    tryApplyPendingSeek(player->mediaStatus());
}

void VideoPlayerController::tryApplyPendingSeek(QMediaPlayer::MediaStatus status)
{
    if (!isReadyForDeferredSeek(status) || m_pendingSeekMode == PendingSeekMode::None) {
        return;
    }

    if (m_pendingSeekMode == PendingSeekMode::Resume
        && status == QMediaPlayer::LoadedMedia
        && duration() <= 0) {
        return;
    }

    const qint64 highlightPosition = m_pendingHighlightPosition;
    const qint64 targetPosition = resolvedPendingSeekPosition(m_pendingSeekPosition, m_pendingSeekMode);
    clearPendingSeek();
    seekToPosition(targetPosition);

    if (highlightPosition >= 0 && m_danmakuCoordinator) {
        m_danmakuCoordinator->highlight(highlightPosition);
    }
}

void VideoPlayerController::clearLocalVideoStateForOnlinePlayback()
{
    m_currentVideoPath.clear();
    if (m_historyCoordinator) {
        m_historyCoordinator->clearCurrentVideo();
    }
    clearPendingSeek();
    m_skipNextRestorePrompt = false;
    if (m_danmakuCoordinator) {
        m_danmakuCoordinator->clearVideo();
    }
}
