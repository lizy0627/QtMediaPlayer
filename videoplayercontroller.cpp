#include "videoplayercontroller.h"

#include <QDebug>
#include <QTimer>
#include <QUrl>

#include "localplaybackdiagnostics.h"
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

bool isPlayableRemoteUrl(const QUrl& url)
{
    const QString scheme = url.scheme().toLower();
    return url.isValid()
        && !url.isEmpty()
        && (scheme == QStringLiteral("http") || scheme == QStringLiteral("https"))
        && !url.host().isEmpty();
}

QString copyableOriginalLink(const OnlinePlaybackRequest& request)
{
    const QString pageUrl = request.pageUrl.trimmed();
    if (!pageUrl.isEmpty()) {
        return pageUrl;
    }
    return request.mediaUrl.toString();
}

QString browserOnlyMessage(const OnlinePlaybackRequest& request)
{
    QString message = request.errorMessage.trimmed();
    if (message.isEmpty()) {
        message = QStringLiteral("\u5f53\u524d\u7ed3\u679c\u53ea\u80fd\u5728\u6d4f\u89c8\u5668\u4e2d\u6253\u5f00\uff1a\u7b2c\u4e09\u65b9\u5e73\u53f0\u672a\u63d0\u4f9b\u53ef\u76f4\u63a5\u4ea4\u7ed9 QMediaPlayer \u7684\u5355\u4e00\u5a92\u4f53\u76f4\u94fe\u3002");
    }

    const QString originalLink = copyableOriginalLink(request);
    if (!originalLink.isEmpty()) {
        message += QStringLiteral("\n\n\u539f\u59cb\u94fe\u63a5\uff08\u53ef\u590d\u5236\u5230\u6d4f\u89c8\u5668\u6253\u5f00\uff09\uff1a\n%1")
                       .arg(originalLink);
    }
    return message;
}

QString invalidOnlineVideoUrlMessage(const OnlinePlaybackRequest& request, const QString& detail)
{
    QString message = detail.trimmed();
    if (message.isEmpty()) {
        message = QStringLiteral("\u64ad\u653e\u5730\u5740\u65e0\u6548\uff1a\u672a\u83b7\u5f97\u53ef\u64ad\u653e\u7684 http/https \u89c6\u9891\u76f4\u94fe\u3002");
    }

    const QString originalLink = copyableOriginalLink(request);
    if (!originalLink.isEmpty()) {
        message += QStringLiteral("\n\n\u539f\u59cb\u94fe\u63a5\uff08\u53ef\u590d\u5236\uff09\uff1a\n%1")
                       .arg(originalLink);
    } else if (!request.mediaUrl.isEmpty()) {
        message += QStringLiteral("\n\n\u7b2c\u4e09\u65b9\u8fd4\u56de\u5730\u5740\uff1a\n%1")
                       .arg(request.mediaUrl.toString());
    }

    return message;
}

QString onlineVideoRetryHint()
{
    return QStringLiteral("\n\n\u63d0\u793a\uff1a\u53ef\u518d\u6b21\u70b9\u51fb\u64ad\u653e\u6309\u94ae\u624b\u52a8\u91cd\u65b0\u89e3\u6790\u5f53\u524d\u89c6\u9891\uff1b\u5982\u679c\u4ecd\u7136\u5931\u8d25\uff0c\u8bf7\u91cd\u65b0\u641c\u7d22\u6216\u5728\u6d4f\u89c8\u5668\u4e2d\u6253\u5f00\u3002");
}

QString withOnlineVideoRetryHint(const QString& message)
{
    const QString trimmedMessage = message.trimmed();
    if (trimmedMessage.contains(QStringLiteral("\u624b\u52a8\u91cd\u65b0\u89e3\u6790"))) {
        return trimmedMessage;
    }
    return trimmedMessage + onlineVideoRetryHint();
}

QString playerErrorTitle(QMediaPlayer::Error error)
{
    switch (error) {
    case QMediaPlayer::ResourceError:
        return QStringLiteral("\u8d44\u6e90\u9519\u8bef");
    case QMediaPlayer::FormatError:
        return QStringLiteral("\u683c\u5f0f\u9519\u8bef");
    case QMediaPlayer::NetworkError:
        return QStringLiteral("\u7f51\u7edc\u9519\u8bef");
    case QMediaPlayer::AccessDeniedError:
        return QStringLiteral("\u8bbf\u95ee\u88ab\u62d2\u7edd");
    case QMediaPlayer::NoError:
        return QString();
    default:
        return QStringLiteral("\u64ad\u653e\u9519\u8bef");
    }
}

QString localPlaybackFailureHint()
{
    return QStringLiteral("\n\n提示：本地文件选择阶段只做快速过滤；扩展名支持不代表编码一定可播放。"
                          "如果播放失败，可能是文件损坏、编码不受支持，或系统缺少对应解码器。");
}

QString playerErrorMessage(QMediaPlayer::Error error, const QString& errorString, bool localFile)
{
    QString message;
    switch (error) {
    case QMediaPlayer::ResourceError:
        message = QStringLiteral("\u8d44\u6e90\u9519\u8bef\uff1a\u65e0\u6cd5\u6253\u5f00\u89c6\u9891\u8d44\u6e90\uff0c\u76f4\u94fe\u53ef\u80fd\u5df2\u8fc7\u671f\u3001\u4e3a\u7a7a\u6216\u88ab\u9632\u76d7\u94fe\u62e6\u622a\u3002");
        break;
    case QMediaPlayer::FormatError:
        message = QStringLiteral("\u683c\u5f0f\u9519\u8bef\uff1a\u5f53\u524d\u89c6\u9891\u683c\u5f0f\u6216\u7b2c\u4e09\u65b9\u8fd4\u56de\u5185\u5bb9\u4e0d\u53d7\u652f\u6301\u3002");
        break;
    case QMediaPlayer::NetworkError:
        message = QStringLiteral("\u7f51\u7edc\u9519\u8bef\uff1a\u65e0\u6cd5\u8bbf\u95ee\u8fdc\u7a0b\u89c6\u9891\u8d44\u6e90\uff0c\u8bf7\u68c0\u67e5\u7f51\u7edc\u6216\u7a0d\u540e\u91cd\u8bd5\u3002");
        break;
    case QMediaPlayer::AccessDeniedError:
        message = QStringLiteral("\u8bbf\u95ee\u88ab\u62d2\u7edd\uff1a\u8be5\u89c6\u9891\u53ef\u80fd\u9700\u8981 Cookie\u3001Referer\u3001\u767b\u5f55\u6743\u9650\u6216\u4e0d\u652f\u6301\u5728\u5ba2\u6237\u7aef\u76f4\u64ad\u3002");
        break;
    case QMediaPlayer::NoError:
        return QString();
    default:
        message = QStringLiteral("\u64ad\u653e\u5668\u53d1\u751f\u672a\u77e5\u9519\u8bef\u3002");
        break;
    }

    if (!errorString.trimmed().isEmpty()) {
        message += QLatin1Char('\n') + errorString.trimmed();
    }
    if (localFile) {
        message += localPlaybackFailureHint();
    }
    return message;
}

[[maybe_unused]] QString quickProbeNotice()
{
    return QStringLiteral("\n\n说明：本地文件选择阶段仅检查文件是否存在、可读、非空以及扩展名是否在支持列表中；"
                          "扩展名支持不代表编码一定可播放。");
}

[[maybe_unused]] QString mediaProbeWarningMessage(const ProbeResult& result)
{
    return QStringLiteral("\u4e0d\u652f\u6301\u64ad\u653e\u8be5\u6587\u4ef6\u3002\n\n"
                          "\u5177\u4f53\u539f\u56e0\uff1a\n%1\n\n"
                          "\u652f\u6301\u7684\u97f3\u9891\u683c\u5f0f\uff1a\n%2\n\n"
                          "\u652f\u6301\u7684\u89c6\u9891\u683c\u5f0f\uff1a\n%3%4")
        .arg(result.reason,
             result.supportedAudioFormats.join(QStringLiteral(", ")),
             result.supportedVideoFormats.join(QStringLiteral(", ")),
             LocalPlaybackDiagnostics::quickProbeNotice());
}

[[maybe_unused]] void debugMediaInfo(const QString& filePath)
{
    const MediaInfo info = MediaProbeService::probeMediaInfo(filePath);
    qDebug() << "FFmpegProbe media info"
             << "formatName:" << info.formatName
             << "videoCodec:" << info.videoCodec
             << "audioCodec:" << info.audioCodec
             << "width:" << info.width
             << "height:" << info.height
             << "fps:" << info.fps
             << "bitRate:" << info.bitRate
             << "duration:" << info.durationMs;
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

    if (!m_openingVideoQueueItem) {
        clearVideoQueue();
    }

    if (localFile) {
        const LocalPlaybackProbeResult playbackProbe =
            MediaProbeService::probeLocalPlayback(filePath, MediaRoute::Video);
        if (!playbackProbe.playable) {
            emit warningRequested(playbackProbe.title, playbackProbe.message);
            return false;
        }
    } else if (!isPlayableRemoteUrl(QUrl::fromUserInput(filePath))) {
        emit warningRequested(QStringLiteral("\u64ad\u653e\u5730\u5740\u65e0\u6548"),
                              QStringLiteral("\u64ad\u653e\u5730\u5740\u65e0\u6548\uff1a\u672a\u83b7\u5f97\u53ef\u64ad\u653e\u7684 http/https \u89c6\u9891\u76f4\u94fe\u3002\n\n%1")
                                  .arg(filePath));
        return false;
    }

    clearPendingSeek();
    saveCurrentProgress();

    if (localFile) {
        m_currentOnlinePlaybackFailed = false;
        m_currentVideoPath = filePath;
        emit previewVideoPathChanged(m_currentVideoPath);
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

        if (m_silentRestoreQueueItem) {
            checkAndRestoreProgressSilently(filePath);
        }
    } else {
        m_currentOnlinePlaybackFailed = false;
        clearLocalVideoStateForOnlinePlayback();
        m_playbackController->openUrl(QUrl(filePath));
    }

    m_silentRestoreQueueItem = false;
    m_playbackController->play();
    return true;
}

bool VideoPlayerController::openQueue(const QStringList& filePaths)
{
    QStringList queue;
    queue.reserve(filePaths.size());
    for (const QString& filePath : filePaths) {
        const QString trimmedPath = filePath.trimmed();
        if (!trimmedPath.isEmpty()) {
            queue.append(trimmedPath);
        }
    }

    if (queue.isEmpty()) {
        return false;
    }

    m_videoQueue = queue;
    m_videoQueueIndex = 0;
    return openQueuedVideo(m_videoQueueIndex);
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
    if (!m_playbackController) {
        return;
    }

    if (m_playbackController->isPlaying()) {
        m_playbackController->pause();
    } else {
        play();
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

    qint64 boundedPosition = qMax<qint64>(0, positionValue);
    const qint64 durationValue = player->duration();
    if (durationValue > 0) {
        boundedPosition = qBound<qint64>(0, boundedPosition, durationValue);
    }

    player->setPosition(boundedPosition);
    emit positionChanged(boundedPosition);

    if (m_danmakuCoordinator) {
        m_danmakuCoordinator->syncAfterSeek(boundedPosition);
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
    if (!m_playbackController) {
        return;
    }

    if (m_onlineCoordinator
        && m_onlineCoordinator->isResolving()
        && m_currentVideoPath.trimmed().isEmpty()) {
        return;
    }

    if (retryCurrentOnlineVideoIfFailed()) {
        return;
    }

    m_playbackController->play();
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

QStringList VideoPlayerController::videoQueue() const
{
    return m_videoQueue;
}

int VideoPlayerController::currentVideoQueueIndex() const
{
    if (m_videoQueueIndex < 0 || m_videoQueueIndex >= m_videoQueue.size()) {
        return -1;
    }

    return m_videoQueue.at(m_videoQueueIndex) == m_currentVideoPath ? m_videoQueueIndex : -1;
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
        m_captureCoordinator->requestScreenshot(isPlaying(), m_currentVideoPath, position());
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
        m_currentOnlinePlaybackFailed = false;
        m_onlineCoordinator->playOnlineVideo(video);
    }
}

bool VideoPlayerController::playQueuedVideo(int index)
{
    return openQueuedVideo(index, true);
}

bool VideoPlayerController::removeQueuedVideo(int index)
{
    if (index < 0 || index >= m_videoQueue.size()) {
        return false;
    }

    m_videoQueue.removeAt(index);
    if (m_videoQueue.isEmpty()) {
        m_videoQueueIndex = -1;
        emitVideoQueueChanged();
        return true;
    }

    if (index < m_videoQueueIndex) {
        --m_videoQueueIndex;
    } else if (index == m_videoQueueIndex) {
        m_videoQueueIndex = index - 1;
    }

    emitVideoQueueChanged();
    return true;
}

void VideoPlayerController::onOnlinePlaybackResolved(const OnlinePlaybackRequest& request)
{
    if (!m_onlineCoordinator || !m_playbackController) {
        return;
    }

    if (request.resolution == PlaybackResolution::BrowserOnly) {
        m_playbackController->stop();
        m_currentOnlinePlaybackFailed = true;
        emit warningRequested(QStringLiteral("\u53ea\u80fd\u5728\u6d4f\u89c8\u5668\u6253\u5f00"),
                              withOnlineVideoRetryHint(browserOnlyMessage(request)));
        return;
    }

    if (request.resolution != PlaybackResolution::DirectPlayable
        || !request.valid
        || !request.mediaUrl.isValid()) {
        m_playbackController->stop();
        m_currentOnlinePlaybackFailed = true;
        const QString detail = request.errorMessage.trimmed().isEmpty()
            ? QStringLiteral("\u672a\u83b7\u5f97\u53ef\u64ad\u653e\u7684 http/https \u89c6\u9891\u76f4\u94fe\u3002")
            : request.errorMessage.trimmed();
        emit warningRequested(QStringLiteral("\u64ad\u653e\u5730\u5740\u65e0\u6548"),
                              withOnlineVideoRetryHint(invalidOnlineVideoUrlMessage(request, detail)));
        return;
    }

    if (!isPlayableRemoteUrl(request.mediaUrl)) {
        m_playbackController->stop();
        m_currentOnlinePlaybackFailed = true;
        emit warningRequested(QStringLiteral("\u64ad\u653e\u5730\u5740\u65e0\u6548"),
                              withOnlineVideoRetryHint(invalidOnlineVideoUrlMessage(
                                  request,
                                  QStringLiteral("\u64ad\u653e\u5730\u5740\u65e0\u6548\uff1a\u7b2c\u4e09\u65b9\u63a5\u53e3\u8fd4\u56de\u7684\u4e0d\u662f\u53ef\u64ad\u653e\u7684 http/https \u89c6\u9891\u76f4\u94fe\u3002"))));
        return;
    }

    m_playbackController->stop();
    m_currentOnlinePlaybackFailed = false;
    clearLocalVideoStateForOnlinePlayback();
    m_playbackController->openUrl(request.mediaUrl);
    m_playbackController->play();
    emit infoRequested(QStringLiteral("\u5f00\u59cb\u64ad\u653e"),
                       m_onlineCoordinator->playbackStartedMessage(request));
}

void VideoPlayerController::onOnlinePlaybackResolveFailed(const QString& message)
{
    if (m_playbackController) {
        m_playbackController->stop();
    }
    m_currentOnlinePlaybackFailed = true;
    const QString failureMessage = message.trimmed().isEmpty()
        ? QStringLiteral("\u672a\u80fd\u89e3\u6790\u5728\u7ebf\u89c6\u9891\u64ad\u653e\u5730\u5740\u3002")
        : message.trimmed();
    emit warningRequested(QStringLiteral("\u89e3\u6790\u64ad\u653e\u5730\u5740\u5931\u8d25"),
                          withOnlineVideoRetryHint(failureMessage));
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
        if (playNextQueuedVideo()) {
            return;
        }
        return;
    }

    if (status == QMediaPlayer::InvalidMedia) {
        if (m_playbackController) {
            m_playbackController->stop();
        }
        const bool localFile = !m_currentVideoPath.trimmed().isEmpty();
        if (localFile) {
            const LocalPlaybackDiagnosis diagnosis =
                LocalPlaybackDiagnostics::diagnose(m_currentVideoPath, QMediaPlayer::FormatError, QString());
            emit warningRequested(diagnosis.title, diagnosis.message);
            if (playNextQueuedVideo()) {
                return;
            }
            return;
        }

        m_currentOnlinePlaybackFailed = true;
        emit warningRequested(QStringLiteral("\u64ad\u653e\u5730\u5740\u65e0\u6548"),
                              withOnlineVideoRetryHint(QStringLiteral("\u65e0\u6cd5\u52a0\u8f7d\u5f53\u524d\u89c6\u9891\uff1a\u5728\u7ebf\u64ad\u653e\u5730\u5740\u53ef\u80fd\u5df2\u8fc7\u671f\u3001\u4e3a\u7a7a\u6216\u88ab\u9632\u76d7\u94fe\u62e6\u622a\u3002")));
        return;
    }
}

void VideoPlayerController::onPlayerError(QMediaPlayer::Error error, const QString& errorString)
{
    if (error == QMediaPlayer::NoError) {
        return;
    }

    if (m_playbackController) {
        m_playbackController->stop();
    }

    if (!m_currentVideoPath.trimmed().isEmpty()) {
        const QString failedVideoPath = m_currentVideoPath;
        const LocalPlaybackDiagnosis diagnosis =
            LocalPlaybackDiagnostics::diagnose(failedVideoPath, error, errorString);
        emit warningRequested(diagnosis.title, diagnosis.message);
        QTimer::singleShot(0, this, [this, failedVideoPath]() {
            if (m_currentVideoPath == failedVideoPath) {
                playNextQueuedVideo();
            }
        });
        return;
    }

    m_currentOnlinePlaybackFailed = true;
    emit warningRequested(playerErrorTitle(error),
                          withOnlineVideoRetryHint(playerErrorMessage(error, errorString, false)));
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

void VideoPlayerController::checkAndRestoreProgressSilently(const QString& filePath)
{
    if (!m_historyCoordinator) {
        return;
    }

    m_historyCoordinator->restoreProgressSilentlyIfUseful(filePath, [this](qint64 savedPosition) {
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
    clearVideoQueue();
    m_currentVideoPath.clear();
    emit previewVideoPathChanged(QString());
    if (m_historyCoordinator) {
        m_historyCoordinator->clearCurrentVideo();
    }
    clearPendingSeek();
    m_skipNextRestorePrompt = false;
    if (m_danmakuCoordinator) {
        m_danmakuCoordinator->clearVideo();
    }
}

void VideoPlayerController::clearVideoQueue()
{
    if (m_videoQueue.isEmpty() && m_videoQueueIndex == -1) {
        return;
    }

    m_videoQueue.clear();
    m_videoQueueIndex = -1;
    emitVideoQueueChanged();
}

void VideoPlayerController::emitVideoQueueChanged()
{
    emit videoQueueChanged(m_videoQueue, currentVideoQueueIndex());
}

bool VideoPlayerController::openQueuedVideo(int index, bool restoreProgressSilently)
{
    if (index < 0 || index >= m_videoQueue.size()) {
        clearVideoQueue();
        return false;
    }

    m_videoQueueIndex = index;
    m_openingVideoQueueItem = true;
    const bool previousSkipRestorePrompt = m_skipNextRestorePrompt;
    const bool previousSilentRestoreQueueItem = m_silentRestoreQueueItem;
    if (restoreProgressSilently) {
        m_skipNextRestorePrompt = true;
        m_silentRestoreQueueItem = true;
    }
    const bool opened = open(m_videoQueue.at(index), true);
    if (!opened) {
        m_skipNextRestorePrompt = previousSkipRestorePrompt;
        m_silentRestoreQueueItem = previousSilentRestoreQueueItem;
    }
    m_openingVideoQueueItem = false;

    if (!opened) {
        return playNextQueuedVideo();
    }

    emitVideoQueueChanged();
    return true;
}

bool VideoPlayerController::playNextQueuedVideo()
{
    if (m_videoQueueIndex < 0 || m_videoQueueIndex + 1 >= m_videoQueue.size()) {
        clearVideoQueue();
        return false;
    }

    return openQueuedVideo(m_videoQueueIndex + 1, true);
}

bool VideoPlayerController::retryCurrentOnlineVideoIfFailed()
{
    if (!m_currentOnlinePlaybackFailed
        || !m_onlineCoordinator
        || !m_onlineCoordinator->hasCurrentVideo()
        || !m_currentVideoPath.trimmed().isEmpty()) {
        return false;
    }

    if (m_onlineCoordinator->isResolving()) {
        return true;
    }

    if (m_playbackController) {
        m_playbackController->stop();
    }
    m_currentOnlinePlaybackFailed = false;
    if (m_onlineCoordinator->retryCurrentVideo()) {
        emit infoRequested(QStringLiteral("\u6b63\u5728\u91cd\u65b0\u89e3\u6790"),
                           QStringLiteral("\u6b63\u5728\u91cd\u65b0\u89e3\u6790\u5f53\u524d\u5728\u7ebf\u89c6\u9891\u64ad\u653e\u5730\u5740\u3002"));
        return true;
    }

    m_currentOnlinePlaybackFailed = true;
    return false;
}
