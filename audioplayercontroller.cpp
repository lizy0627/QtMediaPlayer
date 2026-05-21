#include "audioplayercontroller.h"

#include <QAudioDevice>
#include <QAudioOutput>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QUrl>
#include <QUrlQuery>

#include "audioplaybackcontroller.h"
#include "audiotrack.h"
#include "lyricservice.h"
#include "mediahistory.h"
#include "mediaprobeservice.h"
#include "network/onlinemusicservice.h"

namespace {
const QString kOnlineAudioPrefix = QStringLiteral("online-audio:");
const QString kDefaultOnlineAudioSource = QStringLiteral("netease");

bool isHttpUrl(const QString& value)
{
    return value.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        || value.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);
}

QString playbackErrorMessage(QMediaPlayer::Error error, const QString& errorString)
{
    QString message;
    switch (error) {
    case QMediaPlayer::NoError:
        return QString();
    case QMediaPlayer::ResourceError:
        message = QStringLiteral("资源错误：无法打开媒体文件");
        break;
    case QMediaPlayer::FormatError:
        message = QStringLiteral("格式错误：不支持的媒体格式");
        break;
    case QMediaPlayer::NetworkError:
        message = QStringLiteral("网络错误：无法访问网络资源");
        break;
    case QMediaPlayer::AccessDeniedError:
        message = QStringLiteral("访问被拒绝：没有权限访问该文件");
        break;
    default:
        message = QStringLiteral("未知播放错误");
        break;
    }

    if (!errorString.trimmed().isEmpty()) {
        message += QStringLiteral("\n") + errorString.trimmed();
    }
    return message;
}

QString mediaProbeWarningMessage(const QStringList& failedFiles)
{
    const QString reasonText = failedFiles.join(QStringLiteral("\n"));
    return QStringLiteral("\u4e0d\u652f\u6301\u64ad\u653e\u8be5\u6587\u4ef6\u3002\n\n"
                          "\u5177\u4f53\u539f\u56e0\uff1a\n%1\n\n"
                          "\u652f\u6301\u7684\u97f3\u9891\u683c\u5f0f\uff1a\n%2\n\n"
                          "\u652f\u6301\u7684\u89c6\u9891\u683c\u5f0f\uff1a\n%3")
        .arg(reasonText,
             MediaProbeService::supportedAudioFormats().join(QStringLiteral(", ")),
             MediaProbeService::supportedVideoFormats().join(QStringLiteral(", ")));
}

QString mediaProbeWarningMessage(const ProbeResult& result)
{
    return mediaProbeWarningMessage(QStringList{result.reason});
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
        emit warningRequested(QStringLiteral("错误"), QStringLiteral("在线歌曲缺少来源 ID，无法加入播放列表。"));
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
        m_playbackController->setPosition(record.lastPosition);
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
    }

    QMediaPlayer* player = m_playbackController->player();
    if (player->source() != track.url) {
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
    m_playlistModel->setCurrentIndex(index);
    play();
}

void AudioPlayerController::playPrevious()
{
    if (m_playlistModel->moveToPrevious()) {
        play();
    }
}

void AudioPlayerController::playNext()
{
    if (m_playlistModel->moveToNext()) {
        play();
    }
}

void AudioPlayerController::setPlayMode(PlaylistPlayMode mode)
{
    m_playlistModel->setPlayMode(mode);
}

void AudioPlayerController::setPosition(qint64 position)
{
    m_playbackController->setPosition(position);
}

void AudioPlayerController::setVolume(int volume)
{
    m_playbackController->setVolume(volume);
}

void AudioPlayerController::handleMediaStatusChanged(int status)
{
    if (status == QMediaPlayer::EndOfMedia) {
        m_lastHistoryStartKey.clear();
        if (m_playlistModel->playMode() == PlaylistSingleLoop) {
            m_playbackController->setPosition(0);
            m_playbackController->play();
        } else {
            playNext();
        }
    } else if (status == QMediaPlayer::InvalidMedia) {
        const bool isOnlineTrack = m_playlistModel
            && m_playlistModel->hasCurrent()
            && !m_playlistModel->currentTrack().isLocal;
        markCurrentTrackFailed(isOnlineTrack
                                   ? QStringLiteral("播放地址不可用")
                                   : QStringLiteral("媒体无效，无法播放。"));
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

    QString message = playbackErrorMessage(mediaError, errorString);
    if (m_playlistModel && m_playlistModel->hasCurrent() && !m_playlistModel->currentTrack().isLocal) {
        message = errorString.trimmed().isEmpty()
            ? QStringLiteral("播放地址不可用")
            : QStringLiteral("播放地址不可用\n%1").arg(errorString.trimmed());
    }
    markCurrentTrackFailed(message);
    emit warningRequested(QStringLiteral("播放错误"), message);
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
        || track.playbackStatus == AudioTrackPlaybackStatus::PendingValidation;
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
        markCurrentTrackFailed(QStringLiteral("播放地址不可用"));
        emit warningRequested(QStringLiteral("播放错误"), QStringLiteral("播放地址不可用"));
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

void AudioPlayerController::onOnlineSongUrlResolved(const SongInfo& song)
{
    if (!m_playlistModel || m_pendingResolveIndex < 0) {
        return;
    }

    const int index = m_pendingResolveIndex;
    const QString sourceId = song.sourceId.trimmed().isEmpty()
        ? song.id.trimmed()
        : song.sourceId.trimmed();
    if (sourceId != m_pendingResolveSourceId || index >= m_playlistModel->count()) {
        return;
    }

    AudioTrack track = m_playlistModel->at(index);
    if (track.sourceId.trimmed() != sourceId && track.id.trimmed() != sourceId) {
        return;
    }

    const QUrl resolvedUrl(song.url);
    if (!resolvedUrl.isValid()
        || (resolvedUrl.scheme() != QStringLiteral("http") && resolvedUrl.scheme() != QStringLiteral("https"))
        || resolvedUrl.host().isEmpty()) {
        onOnlineSongUrlResolveError(sourceId, QStringLiteral("播放地址不可用"));
        return;
    }

    track.url = resolvedUrl;
    if (track.sourceUrl.trimmed().isEmpty()) {
        track.sourceUrl = song.sourceUrl;
    }
    if (track.lyricUrl.trimmed().isEmpty()) {
        track.lyricUrl = song.lyricUrl;
    }
    track.playbackStatus = AudioTrackPlaybackStatus::Loading;
    track.statusMessage = QStringLiteral("播放地址已解析，正在加载在线音频。");
    m_playlistModel->updateTrack(index, track);

    m_pendingResolveIndex = -1;
    m_pendingResolveSourceId.clear();

    if (index == m_playlistModel->currentIndex()) {
        play();
    }
}

void AudioPlayerController::onOnlineSongUrlResolveError(const QString& songId, const QString& message)
{
    if (!m_playlistModel) {
        return;
    }

    const QString failureMessage = message.trimmed().isEmpty()
        ? QStringLiteral("播放地址不可用")
        : message.trimmed();
    const int index = m_pendingResolveIndex;
    if (songId.trimmed() == m_pendingResolveSourceId && index >= 0 && index < m_playlistModel->count()) {
        m_playlistModel->updateTrackPlaybackStatus(index,
                                                   AudioTrackPlaybackStatus::Failed,
                                                   failureMessage);
        if (index == m_playlistModel->currentIndex()) {
            emit warningRequested(QStringLiteral("播放错误"), failureMessage);
        }
        m_pendingResolveIndex = -1;
        m_pendingResolveSourceId.clear();
    }
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
