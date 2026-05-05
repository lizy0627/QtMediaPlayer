#include "lyricservice.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QUrl>

#include "lyricparser.h"
#include "network/lyricdownloadservice.h"
#include "network/networkclient.h"

namespace {
QList<LyricLine> parseLyricText(const QString& lyricText)
{
    const QString trimmed = lyricText.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    return LyricParser::parseLrcText(trimmed);
}

QString lyricTextFromResponse(const QByteArray& body)
{
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (document.isObject()) {
        const QJsonObject root = document.object();
        const QString lrc = root.value(QStringLiteral("lrc")).toObject().value(QStringLiteral("lyric")).toString();
        if (!lrc.trimmed().isEmpty()) {
            return lrc;
        }

        const QString lyric = root.value(QStringLiteral("lyric")).toString();
        if (!lyric.trimmed().isEmpty()) {
            return lyric;
        }
    }

    return QString::fromUtf8(body);
}

LyricDisplayState stateForDownloadFailure(const QString& message)
{
    if (message.contains(QStringLiteral("无权限保存歌词"))
        || message.contains(QStringLiteral("保存歌词"))) {
        return LyricDisplayState::SavePermissionDenied;
    }

    if (message.contains(QStringLiteral("未找到歌曲"))
        || message.contains(QStringLiteral("歌词内容为空"))
        || message.contains(QStringLiteral("歌曲 ID 无效"))) {
        return LyricDisplayState::NoLyric;
    }

    if (message.contains(QStringLiteral("解析"))) {
        return LyricDisplayState::ParseFailed;
    }

    return LyricDisplayState::DownloadFailed;
}
}

LyricService::LyricService(QObject* parent)
    : QObject(parent)
    , m_downloader(new LyricDownloadService(this))
    , m_networkClient(new NetworkClient(this))
{
    qRegisterMetaType<LyricDisplayState>("LyricDisplayState");

    connect(m_downloader, &LyricDownloadService::lyricDownloaded,
            this, &LyricService::onLyricDownloaded);
    connect(m_networkClient, &NetworkClient::requestFinished,
            this, &LyricService::onRequestFinished);
}

void LyricService::loadLyricsForAudio(const QString& audioPath)
{
    m_currentAudioPath = audioPath;
    m_pendingOnlineLyricRequestId.clear();
    m_pendingOnlineLyricUrl.clear();

    if (audioPath.isEmpty()) {
        emit lyricsCleared();
        return;
    }

    loadLocalOrDownloadLyrics(audioPath, true);
}

void LyricService::loadLyricsForAudio(const QString& audioPath, const QString& lyricUrl)
{
    m_currentAudioPath = audioPath;
    m_pendingOnlineLyricRequestId.clear();
    m_pendingOnlineLyricUrl.clear();

    if (audioPath.isEmpty()) {
        emit lyricsCleared();
        return;
    }

    const QUrl url = QUrl::fromUserInput(lyricUrl.trimmed());
    if (url.isValid() && !url.isEmpty()) {
        QVariantMap headers;
        headers.insert(QStringLiteral("User-Agent"), QStringLiteral("QtMediaPlayer/1.0"));
        headers.insert(QStringLiteral("Accept"), QStringLiteral("text/plain, application/json, */*"));

        m_pendingOnlineLyricUrl = url.toString();
        RequestOptions options;
        options.headers = headers;
        options.timeout = 15000;
        options.retry = 1;
        m_pendingOnlineLyricRequestId = m_networkClient->get(url, options);
        emit statusMessage(QStringLiteral("正在加载在线歌词..."));
        return;
    }

    loadLocalOrDownloadLyrics(audioPath, true);
}

QString LyricService::currentAudioPath() const
{
    return m_currentAudioPath;
}

void LyricService::onLyricDownloaded(const QString& audioPath, bool success, const QString& message)
{
    if (audioPath != m_currentAudioPath) {
        return;
    }

    emit statusMessage(message);

    if (!success) {
        emitUnavailable(stateForDownloadFailure(message), message);
        return;
    }

    const QList<LyricLine> lyrics = LyricParser::autoLoadLyrics(audioPath);
    if (lyrics.isEmpty()) {
        emitUnavailable(LyricDisplayState::ParseFailed, QStringLiteral("歌词已下载，但解析结果为空。"));
        return;
    }

    emit lyricsReady(audioPath, lyrics);
}

void LyricService::onRequestFinished(const QString& requestId, const NetworkResult& result)
{
    if (requestId != m_pendingOnlineLyricRequestId) {
        return;
    }

    m_pendingOnlineLyricRequestId.clear();
    m_pendingOnlineLyricUrl.clear();
    handleOnlineLyricResponse(result);
}

void LyricService::loadLocalOrDownloadLyrics(const QString& audioPath, bool clearBeforeDownload)
{
    if (audioPath.isEmpty()) {
        emit lyricsCleared();
        return;
    }

    if (!QFileInfo::exists(audioPath)) {
        emitUnavailable(LyricDisplayState::NoLyric, QStringLiteral("未找到音频文件，无法查找歌词。"));
        return;
    }

    const QString lyricFile = LyricParser::findLyricFile(audioPath);
    if (!lyricFile.isEmpty()) {
        const QList<LyricLine> lyrics = LyricParser::parseLrcFile(lyricFile);
        if (!lyrics.isEmpty()) {
            emit lyricsReady(audioPath, lyrics);
            return;
        }

        emitUnavailable(LyricDisplayState::ParseFailed,
                        QStringLiteral("本地歌词文件解析失败：%1").arg(QFileInfo(lyricFile).fileName()));
        return;
    }

    if (clearBeforeDownload) {
        emit lyricsCleared();
    }
    emit statusMessage("正在下载歌词...");
    m_downloader->autoDownloadLyricAsync(audioPath);
}

void LyricService::handleOnlineLyricResponse(const NetworkResult& result)
{
    if (m_currentAudioPath.isEmpty()) {
        return;
    }

    if (!result.ok()) {
        emit statusMessage(QStringLiteral("在线歌词加载失败，正在尝试本地歌词..."));
        loadLocalOrDownloadLyrics(m_currentAudioPath, false);
        return;
    }

    const QList<LyricLine> lyrics = parseLyricText(lyricTextFromResponse(result.body));
    if (lyrics.isEmpty()) {
        emitUnavailable(LyricDisplayState::ParseFailed, QStringLiteral("在线歌词解析失败或内容为空。"));
        return;
    }

    emit lyricsReady(m_currentAudioPath, lyrics);
    emit statusMessage(QStringLiteral("在线歌词加载成功。"));
}

void LyricService::emitUnavailable(LyricDisplayState state, const QString& message)
{
    emit lyricsCleared();
    emit lyricsUnavailable(state, message);
    if (!message.trimmed().isEmpty()) {
        emit statusMessage(message.trimmed());
    }
}
