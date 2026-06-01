#include "audioplayercontroller.h"

#include <QAudioDevice>
#include <QAudioOutput>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QUrl>
#include <QUrlQuery>
#include <QtGlobal>

#include "audioplaybackcontroller.h"
#include "audiotrack.h"
#include "localplaybackdiagnostics.h"
#include "lyricservice.h"
#include "mediahistory.h"
#include "mediaprobeservice.h"
#include "network/onlinemusicservice.h"

namespace {
constexpr qint64 kResumeNearEndThresholdMs = 5000;
const QString kOnlineAudioPrefix = QStringLiteral("online-audio:");
const QString kDefaultOnlineAudioSource = QStringLiteral("netease");

bool isReadyForDeferredSeek(QMediaPlayer::MediaStatus status)
{
    return status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia;
}

bool isHttpUrl(const QString& value)
{
    return value.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        || value.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);
}

bool isPlayableRemoteUrl(const QUrl& url)
{
    const QString scheme = url.scheme().toLower();
    return url.isValid()
        && !url.isEmpty()
        && (scheme == QStringLiteral("http") || scheme == QStringLiteral("https"))
        && !url.host().isEmpty();
}

QString playbackErrorCategory(QMediaPlayer::Error error)
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

QString invalidOnlineAudioUrlMessage(const QString& detail = QString())
{
    QString message = QStringLiteral("\u64ad\u653e\u5730\u5740\u65e0\u6548\uff1a\u7b2c\u4e09\u65b9\u63a5\u53e3\u672a\u8fd4\u56de\u53ef\u64ad\u653e\u7684 http/https \u97f3\u9891\u76f4\u94fe\u3002");
    const QString trimmedDetail = detail.trimmed();
    if (!trimmedDetail.isEmpty()) {
        message += QLatin1Char('\n') + trimmedDetail;
    }
    return message;
}

QString onlineAudioRetryHint()
{
    return QStringLiteral("\n\n\u63d0\u793a\uff1a\u53ef\u5728\u64ad\u653e\u5217\u8868\u4e2d\u9009\u4e2d\u8be5\u6b4c\u66f2\uff0c\u70b9\u51fb\u201c\u91cd\u8bd5\u64ad\u653e\u201d\u624b\u52a8\u91cd\u65b0\u89e3\u6790\uff1b\u5982\u679c\u4ecd\u7136\u5931\u8d25\uff0c\u8bf7\u91cd\u65b0\u641c\u7d22\u540e\u64ad\u653e\u3002");
}

QString withOnlineAudioRetryHint(const QString& message)
{
    const QString trimmedMessage = message.trimmed();
    if (trimmedMessage.contains(QStringLiteral("\u624b\u52a8\u91cd\u65b0\u89e3\u6790"))) {
        return trimmedMessage;
    }
    return trimmedMessage + onlineAudioRetryHint();
}

QString playbackErrorMessage(QMediaPlayer::Error error, const QString& errorString)
{
    QString message;
    switch (error) {
    case QMediaPlayer::NoError:
        return QString();
    case QMediaPlayer::ResourceError:
        message = QStringLiteral("\u8d44\u6e90\u9519\u8bef\uff1a\u65e0\u6cd5\u6253\u5f00\u97f3\u9891\u8d44\u6e90\uff0c\u6587\u4ef6\u6216\u64ad\u653e\u5730\u5740\u53ef\u80fd\u5df2\u5931\u6548\u3002");
        break;
    case QMediaPlayer::FormatError:
        message = QStringLiteral("\u683c\u5f0f\u9519\u8bef\uff1a\u5f53\u524d\u97f3\u9891\u683c\u5f0f\u6216\u7b2c\u4e09\u65b9\u8fd4\u56de\u5185\u5bb9\u4e0d\u53d7\u652f\u6301\u3002");
        break;
    case QMediaPlayer::NetworkError:
        message = QStringLiteral("\u7f51\u7edc\u9519\u8bef\uff1a\u65e0\u6cd5\u8bbf\u95ee\u8fdc\u7a0b\u97f3\u9891\u8d44\u6e90\uff0c\u8bf7\u68c0\u67e5\u7f51\u7edc\u6216\u7a0d\u540e\u91cd\u8bd5\u3002");
        break;
    case QMediaPlayer::AccessDeniedError:
        message = QStringLiteral("\u8bbf\u95ee\u88ab\u62d2\u7edd\uff1a\u8be5\u97f3\u9891\u53ef\u80fd\u9700\u8981\u767b\u5f55\u3001Referer \u6216\u5176\u4ed6\u6388\u6743\u4fe1\u606f\u3002");
        break;
    default:
        message = QStringLiteral("\u64ad\u653e\u5668\u53d1\u751f\u672a\u77e5\u9519\u8bef\u3002");
        break;
    }

    if (!errorString.trimmed().isEmpty()) {
        message += QStringLiteral("\n") + errorString.trimmed();
    }
    return message;
}

QString localPlaybackFailureHint()
{
    return QStringLiteral("\n\n提示：本地文件选择阶段只做快速过滤；扩展名支持不代表编码一定可播放。"
                          "如果播放失败，可能是文件损坏、编码不受支持，或系统缺少对应解码器。");
}

[[maybe_unused]] QString localPlaybackErrorMessage(QMediaPlayer::Error error, const QString& errorString)
{
    return playbackErrorMessage(error, errorString) + localPlaybackFailureHint();
}

QString localPlaybackErrorTitle(const QString& filePath,
                                QMediaPlayer::Error error,
                                const QString& errorString)
{
    return LocalPlaybackDiagnostics::diagnose(filePath, error, errorString).title;
}

QString localPlaybackErrorMessage(const QString& filePath,
                                  QMediaPlayer::Error error,
                                  const QString& errorString)
{
    return LocalPlaybackDiagnostics::diagnose(filePath, error, errorString).message;
}

[[maybe_unused]] QString quickProbeNotice()
{
    return QStringLiteral("\n\n说明：本地文件选择阶段仅检查文件是否存在、可读、非空以及扩展名是否在支持列表中；"
                          "扩展名支持不代表编码一定可播放。");
}

QString mediaProbeWarningMessage(const QStringList& failedFiles)
{
    const QString reasonText = failedFiles.join(QStringLiteral("\n"));
    return QStringLiteral("\u4e0d\u652f\u6301\u64ad\u653e\u8be5\u6587\u4ef6\u3002\n\n"
                          "\u5177\u4f53\u539f\u56e0\uff1a\n%1\n\n"
                          "\u652f\u6301\u7684\u97f3\u9891\u683c\u5f0f\uff1a\n%2\n\n"
                          "\u652f\u6301\u7684\u89c6\u9891\u683c\u5f0f\uff1a\n%3%4")
        .arg(reasonText,
             MediaProbeService::supportedAudioFormats().join(QStringLiteral(", ")),
             MediaProbeService::supportedVideoFormats().join(QStringLiteral(", ")),
             LocalPlaybackDiagnostics::quickProbeNotice());
}

QString mediaProbeWarningMessage(const ProbeResult& result)
{
    return mediaProbeWarningMessage(QStringList{result.reason});
}

bool trackMatchesOnlineSource(const AudioTrack& track, const QString& sourceId)
{
    const QString normalizedSourceId = sourceId.trimmed();
    if (normalizedSourceId.isEmpty() || track.isLocal) {
        return false;
    }

    return track.sourceId.trimmed() == normalizedSourceId
        || track.id.trimmed() == normalizedSourceId;
}

int findOnlineTrackBySourceId(const PlaylistModel* playlistModel, const QString& sourceId)
{
    if (!playlistModel) {
        return -1;
    }

    for (int i = 0; i < playlistModel->count(); ++i) {
        if (trackMatchesOnlineSource(playlistModel->at(i), sourceId)) {
            return i;
        }
    }

    return -1;
}
}

AudioPlayerController::AudioPlayerController(PlaylistModel* playlistModel,
                                             AudioPlaybackController* playbackController,
                                             LyricService* lyricService,
                                             MediaHistoryService* mediaHistoryService,
                                             OnlineMusicService* onlineMusicService,
                                             QObject* parent)
    : QObject(parent)
    , m_playlistModel(playlistModel)
    , m_playbackController(playbackController)
    , m_lyricService(lyricService)
    , m_mediaHistoryService(mediaHistoryService)
    , m_onlineMusicService(onlineMusicService)
{
    if (m_onlineMusicService) {
        connect(m_onlineMusicService, &OnlineMusicService::resolveFinished,
                this, &AudioPlayerController::onOnlineSongUrlResolved);
        connect(m_onlineMusicService, &OnlineMusicService::resolveError,
                this, &AudioPlayerController::onOnlineSongUrlResolveError);
    }
}

PlaylistModel* AudioPlayerController::playlistModel() const
{
    return m_playlistModel;
}

void AudioPlayerController::addLocalFiles(const QStringList& files)
{
    const bool wasPlaying = m_playbackController->isPlaying();
    const int firstAddedIndex = m_playlistModel->count();
    QStringList rejectedFiles;

    for (const QString& file : files) {
        const ProbeResult probeResult = MediaProbeService::probeLocalFile(file);
        if (probeResult.status != ProbeStatus::Supported) {
            const QFileInfo fileInfo(file);
            const QString displayName = fileInfo.fileName().isEmpty() ? file : fileInfo.fileName();
            rejectedFiles.append(QStringLiteral("%1: %2").arg(displayName, probeResult.reason));
            continue;
        }

        const QFileInfo fileInfo(file);

        AudioTrack track;
        track.url = QUrl::fromLocalFile(file);
        track.title = fileInfo.fileName();
        track.isLocal = true;
        m_playlistModel->add(track);
    }

    if (!rejectedFiles.isEmpty()) {
        emit warningRequested(QStringLiteral("\u4e0d\u652f\u6301\u64ad\u653e\u8be5\u6587\u4ef6"),
                              mediaProbeWarningMessage(rejectedFiles));
    }

    if (m_playlistModel->count() == firstAddedIndex) {
        return;
    }

    if (!wasPlaying) {
        clearPendingSeek();
        m_playlistModel->setCurrentIndex(firstAddedIndex);
        play();
        return;
    }

    ensureCurrentTrackSelected();
}

void AudioPlayerController::addOnlineSong(const SongInfo& song)
{
    const QString sourceId = song.sourceId.trimmed().isEmpty()
        ? song.id.trimmed()
        : song.sourceId.trimmed();
    if (sourceId.isEmpty()) {
        emit warningRequested(QStringLiteral("\u5728\u7ebf\u6b4c\u66f2\u4fe1\u606f\u4e0d\u5b8c\u6574"),
                              QStringLiteral("\u7b2c\u4e09\u65b9\u7ed3\u679c\u7f3a\u5c11\u6765\u6e90 ID\uff0c\u65e0\u6cd5\u52a0\u5165\u64ad\u653e\u5217\u8868\u3002"));
        return;
    }

    AudioTrack track;
    track.id = song.id;
    track.sourceId = sourceId;
    track.sourceName = song.sourceName.trimmed().isEmpty()
        ? kDefaultOnlineAudioSource
        : song.sourceName.trimmed();
    track.title = song.title.trimmed().isEmpty() ? song.name : song.title;
    track.artist = song.artist;
    track.album = song.album;
    track.sourceUrl = song.sourceUrl;
    track.lyricUrl = song.lyricUrl;
    track.duration = song.duration;
    track.isLocal = false;
    track.playbackStatus = AudioTrackPlaybackStatus::PendingValidation;
    track.statusMessage = QStringLiteral("已加入播放列表，播放前会解析真实播放地址。");

    m_playlistModel->add(track);

    if (!m_playbackController->isPlaying()) {
        clearPendingSeek();
        m_playlistModel->setCurrentIndex(m_playlistModel->count() - 1);
        play();
    }

    emit informationRequested(
        QStringLiteral("已加入播放列表"),
        QStringLiteral("已加入：%1\n艺术家：%2\n\n提示：开始播放时会解析真实播放地址。")
            .arg(track.title, track.artist));
}

bool AudioPlayerController::playHistoryRecord(const MediaHistoryRecord& record)
{
    const QString historyPath = record.filePath.trimmed();
    if (historyPath.isEmpty()) {
        emit warningRequested(QStringLiteral("提示"), QStringLiteral("该音频历史记录为空，无法播放。"));
        return false;
    }

    AudioTrack track;
    track.title = record.fileName.trimmed().isEmpty() ? historyPath : record.fileName.trimmed();
    track.duration = record.duration > 0 ? static_cast<int>(record.duration / 1000) : 0;

    if (historyPath.startsWith(kOnlineAudioPrefix, Qt::CaseInsensitive)) {
        if (!populateTrackFromOnlineHistory(historyPath, record, &track)) {
            emit warningRequested(QStringLiteral("提示"),
                                  QStringLiteral("在线音频历史记录缺少可用的来源信息，无法恢复播放。"));
            return false;
        }
    } else if (isHttpUrl(historyPath)) {
        track.url = QUrl(historyPath);
        track.isLocal = false;
        track.sourceName = QStringLiteral("url");
        track.playbackStatus = AudioTrackPlaybackStatus::PendingValidation;
        track.statusMessage = QStringLiteral("从历史记录恢复，开始播放时会重新验证在线音频。");
    } else {
        const ProbeResult probeResult = MediaProbeService::probeLocalFile(historyPath);
        if (probeResult.status != ProbeStatus::Supported) {
            emit warningRequested(QStringLiteral("\u4e0d\u652f\u6301\u64ad\u653e\u8be5\u6587\u4ef6"),
                                  mediaProbeWarningMessage(probeResult));
            return false;
        }
        const QFileInfo fileInfo(historyPath);
        track.url = QUrl::fromLocalFile(historyPath);
        track.title = record.fileName.trimmed().isEmpty() ? fileInfo.fileName() : record.fileName.trimmed();
        track.isLocal = true;
    }

    if (!track.url.isValid()) {
        emit warningRequested(QStringLiteral("提示"), QStringLiteral("该音频历史记录的播放地址无效。"));
        return false;
    }

    m_playlistModel->add(track);
    m_playlistModel->setCurrentIndex(m_playlistModel->count() - 1);
    play();
    if (!record.isCompleted && record.lastPosition > 0) {
        schedulePendingSeek(record.lastPosition);
    }
    return true;
}

bool AudioPlayerController::removeTrackAt(int index)
{
    if (index < 0 || index >= m_playlistModel->count()) {
        return false;
    }

    const int oldIndex = m_playlistModel->currentIndex();
    const bool removedCurrentTrack = index == oldIndex;
    const bool wasPlayingCurrentTrack =
        removedCurrentTrack && m_playbackController->isPlaying();

    if (!m_playlistModel->removeAt(index)) {
        return false;
    }

    if (!removedCurrentTrack) {
        return true;
    }

    clearPendingSeek();
    m_playbackController->stop();
    if (m_playlistModel->isEmpty()) {
        syncLyricsForCurrentTrack();
        return true;
    }

    if (wasPlayingCurrentTrack) {
        play();
    } else {
        syncLyricsForCurrentTrack();
    }

    return true;
}

void AudioPlayerController::clearPlaylist()
{
    clearPendingSeek();
    m_forceReloadResolvedOnlineTrack = false;
    m_playbackController->stop();
    m_playlistModel->clear();
    syncLyricsForCurrentTrack();
}

void AudioPlayerController::play()
{
    ensureCurrentTrackSelected();
    if (!m_playlistModel->hasCurrent()) {
        return;
    }

    const AudioTrack track = m_playlistModel->currentTrack();
    if (currentTrackNeedsOnlineResolve(track)) {
        resolveCurrentOnlineTrack();
        return;
    }

    if (track.isLocal) {
        const ProbeResult probeResult = MediaProbeService::probeLocalFile(track.url.toLocalFile());
        if (probeResult.status != ProbeStatus::Supported) {
            markCurrentTrackFailed(probeResult.reason);
            emit warningRequested(QStringLiteral("\u4e0d\u652f\u6301\u64ad\u653e\u8be5\u6587\u4ef6"),
                                  mediaProbeWarningMessage(probeResult));
            return;
        }
    } else if (!isPlayableRemoteUrl(track.url)) {
        const QString message = withOnlineAudioRetryHint(invalidOnlineAudioUrlMessage(track.url.toString()));
        markCurrentTrackFailed(message);
        emit warningRequested(QStringLiteral("\u64ad\u653e\u5730\u5740\u65e0\u6548"), message);
        return;
    }

    QMediaPlayer* player = m_playbackController->player();
    const bool forceReload = m_forceReloadResolvedOnlineTrack && !track.isLocal;
    if (player->source() != track.url || forceReload) {
        m_forceReloadResolvedOnlineTrack = false;
        m_playlistModel->updateCurrentTrackPlaybackStatus(
            AudioTrackPlaybackStatus::Loading,
            track.isLocal
                ? QStringLiteral("正在加载本地音频。")
                : QStringLiteral("正在验证在线音频是否可播放。"));
        m_playbackController->open(track.url);
        m_lastHistoryStartKey.clear();
        syncLyricsForCurrentTrack();
    }

    m_playbackController->ensureAudioOutput();
    if (m_playbackController->volume() < 1) {
        m_playbackController->setVolume(80);
        emit volumeAdjusted(80);
    }

    m_playbackController->play();
}

void AudioPlayerController::pause()
{
    m_playbackController->pause();
}

void AudioPlayerController::togglePlayback()
{
    if (m_playbackController->isPlaying()) {
        pause();
    } else {
        play();
    }
}

void AudioPlayerController::playAt(int index)
{
    clearPendingSeek();
    m_forceReloadResolvedOnlineTrack = false;
    m_playlistModel->setCurrentIndex(index);
    play();
}

bool AudioPlayerController::retryOnlineTrackAt(int index)
{
    if (!m_playlistModel || index < 0 || index >= m_playlistModel->count()) {
        return false;
    }

    const AudioTrack track = m_playlistModel->at(index);
    if (track.isLocal) {
        emit warningRequested(QStringLiteral("\u65e0\u6cd5\u91cd\u65b0\u89e3\u6790"),
                              QStringLiteral("\u53ea\u6709\u5728\u7ebf\u97f3\u4e50\u53ef\u4ee5\u91cd\u65b0\u89e3\u6790\u64ad\u653e\u5730\u5740\u3002"));
        return false;
    }

    clearPendingSeek();
    m_forceReloadResolvedOnlineTrack = false;
    m_playlistModel->setCurrentIndex(index);
    resolveCurrentOnlineTrack();
    return true;
}

void AudioPlayerController::playPrevious()
{
    if (m_playlistModel->moveToPrevious()) {
        clearPendingSeek();
        m_forceReloadResolvedOnlineTrack = false;
        play();
    }
}

void AudioPlayerController::playNext()
{
    if (m_playlistModel->moveToNext()) {
        clearPendingSeek();
        m_forceReloadResolvedOnlineTrack = false;
        play();
    }
}

void AudioPlayerController::setPlayMode(PlaylistPlayMode mode)
{
    m_playlistModel->setPlayMode(mode);
}

void AudioPlayerController::setPosition(qint64 position)
{
    clearPendingSeek();
    m_playbackController->setPosition(position);
}

void AudioPlayerController::setVolume(int volume)
{
    m_playbackController->setVolume(volume);
}

void AudioPlayerController::handleMediaStatusChanged(int status)
{
    tryApplyPendingSeek(status);

    if (status == QMediaPlayer::EndOfMedia) {
        clearPendingSeek();
        m_lastHistoryStartKey.clear();
        if (m_playlistModel->playMode() == PlaylistSingleLoop) {
            m_playbackController->setPosition(0);
            m_playbackController->play();
        } else {
            playNext();
        }
    } else if (status == QMediaPlayer::InvalidMedia) {
        clearPendingSeek();
        const bool isOnlineTrack = m_playlistModel
            && m_playlistModel->hasCurrent()
            && !m_playlistModel->currentTrack().isLocal;

        if (!isOnlineTrack && m_playlistModel && m_playlistModel->hasCurrent()) {
            const QString filePath = m_playlistModel->currentTrack().url.toLocalFile();
            const LocalPlaybackDiagnosis diagnosis =
                LocalPlaybackDiagnostics::diagnose(filePath, QMediaPlayer::FormatError, QString());
            markCurrentTrackFailed(diagnosis.message);
            emit warningRequested(diagnosis.title, diagnosis.message);
            return;
        }

    const QString message =
            withOnlineAudioRetryHint(invalidOnlineAudioUrlMessage(QStringLiteral("\u64ad\u653e\u5668\u62a5\u544a\u5a92\u4f53\u65e0\u6548\uff0c\u8be5\u76f4\u94fe\u53ef\u80fd\u5df2\u8fc7\u671f\u6216\u88ab\u9632\u76d7\u94fe\u62e6\u622a\u3002")));
        markCurrentTrackFailed(message);
        emit warningRequested(QStringLiteral("\u64ad\u653e\u5730\u5740\u65e0\u6548"), message);
        return;
    }
}

void AudioPlayerController::handlePlaybackStateChanged(int state)
{
    if (state == QMediaPlayer::PlayingState) {
        m_playlistModel->updateCurrentTrackPlaybackStatus(AudioTrackPlaybackStatus::Playing);
        markCurrentTrackPlaybackStarted();
    }
}

void AudioPlayerController::handlePlayerError(int error, const QString& errorString)
{
    const auto mediaError = static_cast<QMediaPlayer::Error>(error);
    if (mediaError == QMediaPlayer::NoError) {
        return;
    }

    const bool hasCurrentTrack = m_playlistModel && m_playlistModel->hasCurrent();
    if (hasCurrentTrack && !m_playlistModel->currentTrack().isLocal) {
        QString message;
        message = QStringLiteral("%1\uff1a\u5728\u7ebf\u97f3\u9891\u64ad\u653e\u5730\u5740\u4e0d\u53ef\u7528\uff0c\u53ef\u80fd\u5df2\u8fc7\u671f\u3001\u4e3a\u7a7a\u3001\u4e0d\u53ef\u8bbf\u95ee\u6216\u88ab\u9632\u76d7\u94fe\u62e6\u622a\u3002")
                      .arg(playbackErrorCategory(mediaError));
        if (!errorString.trimmed().isEmpty()) {
            message += QLatin1Char('\n') + errorString.trimmed();
        }
        message = withOnlineAudioRetryHint(message);
        markCurrentTrackFailed(message);
        emit warningRequested(playbackErrorCategory(mediaError), message);
        return;
    }

    if (hasCurrentTrack && m_playlistModel->currentTrack().isLocal) {
        const QString filePath = m_playlistModel->currentTrack().url.toLocalFile();
        const QString title = localPlaybackErrorTitle(filePath, mediaError, errorString);
        const QString message = localPlaybackErrorMessage(filePath, mediaError, errorString);
        markCurrentTrackFailed(message);
        emit warningRequested(title, message);
        return;
    }

    const QString message = playbackErrorMessage(mediaError, errorString);
    markCurrentTrackFailed(message);
    emit warningRequested(playbackErrorCategory(mediaError), message);
}

QString AudioPlayerController::diagnosticText() const
{
    QString info = QStringLiteral("=== 音频系统诊断 ===\n\n");

    QAudioOutput* audioOutput = m_playbackController ? m_playbackController->audioOutput() : nullptr;
    QMediaPlayer* player = m_playbackController ? m_playbackController->player() : nullptr;

    info += QStringLiteral("【音频输出设备】\n");
    if (audioOutput) {
        info += QStringLiteral("设备: %1\n").arg(audioOutput->device().description());
        info += QStringLiteral("音量: %1%\n").arg(audioOutput->volume() * 100, 0, 'f', 0);
        info += QStringLiteral("静音: %1\n\n")
                    .arg(audioOutput->isMuted() ? QStringLiteral("是") : QStringLiteral("否"));
    } else {
        info += QStringLiteral("错误：音频输出未初始化。\n\n");
    }

    info += QStringLiteral("【播放器状态】\n播放状态: ");
    if (!player) {
        info += QStringLiteral("播放器未初始化\n");
    } else {
        switch (player->playbackState()) {
        case QMediaPlayer::StoppedState:
            info += QStringLiteral("停止\n");
            break;
        case QMediaPlayer::PlayingState:
            info += QStringLiteral("播放中\n");
            break;
        case QMediaPlayer::PausedState:
            info += QStringLiteral("暂停\n");
            break;
        }

        info += QStringLiteral("媒体状态: ");
        switch (player->mediaStatus()) {
        case QMediaPlayer::NoMedia:
            info += QStringLiteral("无媒体\n");
            break;
        case QMediaPlayer::LoadingMedia:
            info += QStringLiteral("加载中\n");
            break;
        case QMediaPlayer::LoadedMedia:
            info += QStringLiteral("已加载\n");
            break;
        case QMediaPlayer::BufferingMedia:
            info += QStringLiteral("缓冲中\n");
            break;
        case QMediaPlayer::BufferedMedia:
            info += QStringLiteral("已缓冲\n");
            break;
        case QMediaPlayer::EndOfMedia:
            info += QStringLiteral("播放结束\n");
            break;
        case QMediaPlayer::InvalidMedia:
            info += QStringLiteral("无效媒体\n");
            break;
        default:
            info += QStringLiteral("未知\n");
            break;
        }

        info += QStringLiteral("当前源: %1\n").arg(player->source().toString());
        info += QStringLiteral("时长: %1ms\n").arg(player->duration());
        info += QStringLiteral("位置: %1ms\n\n").arg(player->position());
    }

    info += QStringLiteral("【播放列表】\n");
    info += QStringLiteral("歌曲数量: %1\n").arg(m_playlistModel ? m_playlistModel->count() : 0);
    info += QStringLiteral("当前索引: %1\n\n").arg(m_playlistModel ? m_playlistModel->currentIndex() : -1);

    if (player && player->error() != QMediaPlayer::NoError) {
        info += QStringLiteral("【错误信息】\n");
        info += QStringLiteral("错误代码: %1\n").arg(player->error());
        info += QStringLiteral("错误描述: %1\n\n").arg(player->errorString());
    }

    info += QStringLiteral("【建议】\n");
    if (audioOutput && audioOutput->volume() < 0.01) {
        info += QStringLiteral("音量过低，请调高音量滑块。\n");
    }
    if (audioOutput && audioOutput->isMuted()) {
        info += QStringLiteral("音频已静音，请取消静音。\n");
    }
    if (!m_playlistModel || m_playlistModel->isEmpty()) {
        info += QStringLiteral("播放列表为空，请添加音乐文件。\n");
    }
    if (player && player->error() != QMediaPlayer::NoError) {
        info += QStringLiteral("播放器出现错误，请检查文件格式或在线资源可用性。\n");
    }

    return info;
}

void AudioPlayerController::syncLyricsForCurrentTrack()
{
    if (!m_playlistModel->hasCurrent()) {
        m_lyricService->loadLyricsForAudio(QString());
        return;
    }

    const AudioTrack track = m_playlistModel->currentTrack();
    const QString audioPath = track.url.isLocalFile()
        ? track.url.toLocalFile()
        : historyKeyForTrack(track);

    if (audioPath.isEmpty()) {
        m_lyricService->loadLyricsForAudio(QString());
        return;
    }

    m_lyricService->loadLyricsForAudio(audioPath, track.lyricUrl);
}

void AudioPlayerController::ensureCurrentTrackSelected()
{
    if (!m_playlistModel->hasCurrent() && !m_playlistModel->isEmpty()) {
        m_playlistModel->setCurrentIndex(0);
    }
}

void AudioPlayerController::markCurrentTrackFailed(const QString& message)
{
    if (!m_playlistModel) {
        return;
    }

    clearPendingSeek();
    if (m_playbackController) {
        m_playbackController->stop();
    }
    m_playlistModel->updateCurrentTrackPlaybackStatus(AudioTrackPlaybackStatus::Failed, message);
}

bool AudioPlayerController::currentTrackNeedsOnlineResolve(const AudioTrack& track) const
{
    if (track.isLocal) {
        return false;
    }

    const QString sourceId = !track.sourceId.trimmed().isEmpty()
        ? track.sourceId.trimmed()
        : track.id.trimmed();
    if (sourceId.isEmpty()) {
        return false;
    }

    return track.url.isEmpty()
        || !track.url.isValid()
        || track.playbackStatus == AudioTrackPlaybackStatus::PendingValidation
        || track.playbackStatus == AudioTrackPlaybackStatus::Failed;
}

void AudioPlayerController::resolveCurrentOnlineTrack()
{
    if (!m_playlistModel || !m_playlistModel->hasCurrent()) {
        return;
    }

    AudioTrack track = m_playlistModel->currentTrack();
    const QString sourceId = !track.sourceId.trimmed().isEmpty()
        ? track.sourceId.trimmed()
        : track.id.trimmed();
    if (sourceId.isEmpty() || !m_onlineMusicService) {
        const QString message = sourceId.isEmpty()
            ? QStringLiteral("\u65e0\u6cd5\u89e3\u6790\u5728\u7ebf\u6b4c\u66f2\uff1a\u7f3a\u5c11\u7b2c\u4e09\u65b9\u6765\u6e90 ID\u3002")
            : QStringLiteral("\u65e0\u6cd5\u89e3\u6790\u5728\u7ebf\u6b4c\u66f2\uff1a\u5728\u7ebf\u97f3\u4e50\u670d\u52a1\u672a\u521d\u59cb\u5316\u3002");
        markCurrentTrackFailed(message);
        emit warningRequested(QStringLiteral("\u89e3\u6790\u64ad\u653e\u5730\u5740\u5931\u8d25"), message);
        return;
    }

    if (m_pendingResolveIndex == m_playlistModel->currentIndex()
        && m_pendingResolveSourceId == sourceId) {
        return;
    }

    if (m_pendingResolveIndex >= 0 && m_pendingResolveIndex < m_playlistModel->count()
        && m_pendingResolveIndex != m_playlistModel->currentIndex()) {
        m_playlistModel->updateTrackPlaybackStatus(
            m_pendingResolveIndex,
            AudioTrackPlaybackStatus::PendingValidation,
            QStringLiteral("等待播放时重新解析在线歌曲播放地址。"));
    }

    m_pendingResolveIndex = m_playlistModel->currentIndex();
    m_pendingResolveSourceId = sourceId;
    m_playlistModel->updateCurrentTrackPlaybackStatus(
        AudioTrackPlaybackStatus::Loading,
        QStringLiteral("正在解析在线歌曲播放地址。"));
    m_onlineMusicService->resolveSongUrlAsync(sourceId);
}

void AudioPlayerController::clearPendingSeek()
{
    m_pendingSeekPosition = -1;
}

qint64 AudioPlayerController::resolvedPendingSeekPosition(qint64 requestedPosition) const
{
    const QMediaPlayer* player = m_playbackController ? m_playbackController->player() : nullptr;
    const qint64 durationValue = player ? player->duration() : qint64(0);
    qint64 targetPosition = qMax<qint64>(0, requestedPosition);

    if (durationValue <= 0) {
        return targetPosition;
    }

    if (targetPosition >= qMax<qint64>(0, durationValue - kResumeNearEndThresholdMs)) {
        return 0;
    }

    return qBound<qint64>(0, targetPosition, qMax<qint64>(0, durationValue - 1));
}

void AudioPlayerController::schedulePendingSeek(qint64 position)
{
    if (position <= 0) {
        clearPendingSeek();
        return;
    }

    m_pendingSeekPosition = position;

    const QMediaPlayer* player = m_playbackController ? m_playbackController->player() : nullptr;
    if (!player) {
        return;
    }

    tryApplyPendingSeek(static_cast<int>(player->mediaStatus()));
}

void AudioPlayerController::tryApplyPendingSeek(int status)
{
    const auto mediaStatus = static_cast<QMediaPlayer::MediaStatus>(status);
    if (!isReadyForDeferredSeek(mediaStatus) || m_pendingSeekPosition < 0) {
        return;
    }

    if (mediaStatus == QMediaPlayer::LoadedMedia) {
        const QMediaPlayer* player = m_playbackController ? m_playbackController->player() : nullptr;
        if (player && player->duration() <= 0) {
            return;
        }
    }

    const qint64 targetPosition = resolvedPendingSeekPosition(m_pendingSeekPosition);
    clearPendingSeek();
    m_playbackController->setPosition(targetPosition);
}

void AudioPlayerController::onOnlineSongUrlResolved(const SongInfo& song)
{
    if (!m_playlistModel) {
        return;
    }

    const QString sourceId = song.sourceId.trimmed().isEmpty()
        ? song.id.trimmed()
        : song.sourceId.trimmed();
    if (sourceId.isEmpty()) {
        return;
    }

    int index = -1;
    if (sourceId == m_pendingResolveSourceId
        && m_pendingResolveIndex >= 0
        && m_pendingResolveIndex < m_playlistModel->count()) {
        index = m_pendingResolveIndex;
    }
    if (index < 0 || !trackMatchesOnlineSource(m_playlistModel->at(index), sourceId)) {
        index = findOnlineTrackBySourceId(m_playlistModel, sourceId);
    }
    if (index < 0) {
        return;
    }

    AudioTrack track = m_playlistModel->at(index);
    if (!trackMatchesOnlineSource(track, sourceId)) {
        return;
    }

    const QUrl resolvedUrl(song.url);
    if (!isPlayableRemoteUrl(resolvedUrl)) {
        onOnlineSongUrlResolveError(sourceId, invalidOnlineAudioUrlMessage(song.url));
        return;
    }

    track.url = resolvedUrl;
    if (track.sourceUrl.trimmed().isEmpty()) {
        track.sourceUrl = song.sourceUrl;
    }
    if (track.lyricUrl.trimmed().isEmpty()) {
        track.lyricUrl = song.lyricUrl;
    }
    const bool isCurrentTrack = index == m_playlistModel->currentIndex();
    track.playbackStatus = isCurrentTrack
        ? AudioTrackPlaybackStatus::Loading
        : AudioTrackPlaybackStatus::Idle;
    track.statusMessage = isCurrentTrack
        ? QStringLiteral("\u64ad\u653e\u5730\u5740\u5df2\u89e3\u6790\uff0c\u6b63\u5728\u52a0\u8f7d\u5728\u7ebf\u97f3\u9891\u3002")
        : QStringLiteral("\u64ad\u653e\u5730\u5740\u5df2\u89e3\u6790\uff0c\u7b49\u5f85\u64ad\u653e\u3002");
    m_playlistModel->updateTrack(index, track);

    m_pendingResolveIndex = -1;
    m_pendingResolveSourceId.clear();

    if (isCurrentTrack) {
        m_forceReloadResolvedOnlineTrack = true;
        play();
    }
}

void AudioPlayerController::onOnlineSongUrlResolveError(const QString& songId, const QString& message)
{
    if (!m_playlistModel) {
        return;
    }

    const QString sourceId = songId.trimmed();
    const QString failureMessage = withOnlineAudioRetryHint(message.trimmed().isEmpty()
        ? QStringLiteral("\u89e3\u6790\u64ad\u653e\u5730\u5740\u5931\u8d25\uff1a\u7b2c\u4e09\u65b9\u63a5\u53e3\u672a\u8fd4\u56de\u53ef\u64ad\u653e\u7684\u97f3\u9891\u76f4\u94fe\u3002")
        : message.trimmed());

    int index = -1;
    if (sourceId == m_pendingResolveSourceId
        && m_pendingResolveIndex >= 0
        && m_pendingResolveIndex < m_playlistModel->count()) {
        index = m_pendingResolveIndex;
    }
    if (index < 0 || !trackMatchesOnlineSource(m_playlistModel->at(index), sourceId)) {
        index = findOnlineTrackBySourceId(m_playlistModel, sourceId);
    }

    if (index >= 0) {
        if (index == m_playlistModel->currentIndex()) {
            markCurrentTrackFailed(failureMessage);
            emit warningRequested(QStringLiteral("\u89e3\u6790\u64ad\u653e\u5730\u5740\u5931\u8d25"), failureMessage);
        } else {
            m_playlistModel->updateTrackPlaybackStatus(index,
                                                       AudioTrackPlaybackStatus::Failed,
                                                       failureMessage);
        }
    }

    if (sourceId == m_pendingResolveSourceId) {
        m_pendingResolveIndex = -1;
        m_pendingResolveSourceId.clear();
    }
    m_forceReloadResolvedOnlineTrack = false;
}

void AudioPlayerController::markCurrentTrackPlaybackStarted()
{
    if (!m_mediaHistoryService || !m_playlistModel->hasCurrent()) {
        return;
    }

    const AudioTrack track = m_playlistModel->currentTrack();
    const QString historyKey = historyKeyForTrack(track);
    if (historyKey.isEmpty() || historyKey == m_lastHistoryStartKey) {
        return;
    }

    const qint64 duration = m_playbackController && m_playbackController->player()
        ? m_playbackController->player()->duration()
        : qint64(0);

    MediaHistoryRecord record;
    record.filePath = historyKey;
    record.fileType = mediaKindToString(MediaKind::Audio);
    record.fileName = track.displayText();
    record.duration = duration > 0 ? duration : static_cast<qint64>(track.duration) * 1000;

    if (m_mediaHistoryService->savePlaybackStart(record, MediaKind::Audio)) {
        m_lastHistoryStartKey = historyKey;
    }
}

QString AudioPlayerController::historyKeyForTrack(const AudioTrack& track) const
{
    if (track.url.isLocalFile()) {
        return track.url.toLocalFile();
    }

    const QString sourceId = !track.sourceId.trimmed().isEmpty()
        ? track.sourceId.trimmed()
        : track.id.trimmed();

    if (!sourceId.isEmpty()) {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("source"),
                           track.sourceName.trimmed().isEmpty()
                               ? kDefaultOnlineAudioSource
                               : track.sourceName.trimmed());
        query.addQueryItem(QStringLiteral("source_id"), sourceId);
        query.addQueryItem(QStringLiteral("url"), track.url.toString());
        query.addQueryItem(QStringLiteral("title"), track.title);
        query.addQueryItem(QStringLiteral("artist"), track.artist);
        query.addQueryItem(QStringLiteral("album"), track.album);
        query.addQueryItem(QStringLiteral("source_url"), track.sourceUrl);
        query.addQueryItem(QStringLiteral("lyric_url"), track.lyricUrl);
        return kOnlineAudioPrefix + query.toString(QUrl::FullyEncoded);
    }

    return track.url.toString();
}

bool AudioPlayerController::populateTrackFromOnlineHistory(const QString& historyPath,
                                                           const MediaHistoryRecord& record,
                                                           AudioTrack* track) const
{
    if (!track) {
        return false;
    }

    const QString payload = historyPath.mid(kOnlineAudioPrefix.size()).trimmed();
    if (payload.isEmpty()) {
        return false;
    }

    track->isLocal = false;
    track->playbackStatus = AudioTrackPlaybackStatus::PendingValidation;
    track->statusMessage = QStringLiteral("从历史记录恢复，播放前会重新解析在线歌曲播放地址。");

    QUrlQuery query(payload);
    const QString sourceId = query.queryItemValue(QStringLiteral("source_id")).trimmed();
    const QString urlText = query.queryItemValue(QStringLiteral("url")).trimmed();
    if (!sourceId.isEmpty()) {
        track->id = sourceId;
        track->sourceId = sourceId;
        track->sourceName = query.queryItemValue(QStringLiteral("source")).trimmed();
        track->title = query.queryItemValue(QStringLiteral("title")).trimmed();
        track->artist = query.queryItemValue(QStringLiteral("artist")).trimmed();
        track->album = query.queryItemValue(QStringLiteral("album")).trimmed();
        track->sourceUrl = query.queryItemValue(QStringLiteral("source_url")).trimmed();
        track->lyricUrl = query.queryItemValue(QStringLiteral("lyric_url")).trimmed();
        if (track->title.isEmpty()) {
            track->title = record.fileName.trimmed().isEmpty() ? sourceId : record.fileName.trimmed();
        }

        return true;
    }

    if (!urlText.isEmpty()) {
        track->sourceName = QStringLiteral("url");
        track->title = record.fileName.trimmed().isEmpty() ? urlText : record.fileName.trimmed();
        track->url = QUrl(urlText);
        return track->url.isValid() && !track->url.isEmpty();
    }

    track->id = payload;
    track->sourceId = payload;
    track->sourceName = kDefaultOnlineAudioSource;
    track->title = record.fileName.trimmed().isEmpty() ? payload : record.fileName.trimmed();
    return true;
}
