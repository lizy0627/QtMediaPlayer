#include "lyricdownloadservice.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>

#include <QStringConverter>

namespace {
QString safeLyricBaseName(const QString& audioFilePath)
{
    QString baseName = QFileInfo(audioFilePath).completeBaseName().trimmed();
    if (baseName.isEmpty()) {
        baseName = audioFilePath.trimmed();
    }
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("untitled");
    }

    baseName.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|]+)")), QStringLiteral("_"));
    baseName.replace(QRegularExpression(QStringLiteral(R"(\s+)")), QStringLiteral(" "));
    return baseName.left(120).trimmed();
}

QString lyricCacheDir()
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty()) {
        appDataPath = QDir::homePath() + QStringLiteral("/.QtMediaPlayer");
    }

    return QDir(appDataPath).filePath(QStringLiteral("lyrics"));
}
}

LyricDownloadService::LyricDownloadService(QObject* parent)
    : QObject(parent)
    , m_networkClient(new NetworkClient(this))
{
    connect(m_networkClient, &NetworkClient::requestFinished,
            this, &LyricDownloadService::onRequestFinished);
}

QString LyricDownloadService::lastError() const
{
    return m_lastError;
}

LyricDownloadService::SongInfo LyricDownloadService::parseSongInfo(const QString& filePath)
{
    const QFileInfo fileInfo(filePath);
    const QString baseName = fileInfo.completeBaseName();

    if (baseName.contains(QStringLiteral(" - "))) {
        const QStringList parts = baseName.split(QStringLiteral(" - "));
        if (parts.size() >= 2) {
            return SongInfo(parts[1].trimmed(), parts[0].trimmed());
        }
    }

    return SongInfo(baseName, QString());
}

bool LyricDownloadService::saveLyricToFile(const QString& lyricText,
                                           const QString& audioFilePath,
                                           LyricSaveStrategy strategy,
                                           QString* savedPath,
                                           QString* errorMessage)
{
    const QFileInfo audioInfo(audioFilePath);
    QString targetDir;
    QString baseName;

    if (strategy == LyricSaveStrategy::AudioDirectory) {
        targetDir = audioInfo.absolutePath();
        baseName = audioInfo.completeBaseName().trimmed();
    } else {
        targetDir = lyricCacheDir();
        baseName = safeLyricBaseName(audioFilePath);
    }

    if (targetDir.trimmed().isEmpty() || baseName.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法确定歌词保存目录。");
        }
        return false;
    }

    QDir dir(targetDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无权限保存歌词：无法创建目录 %1")
                                .arg(QDir::toNativeSeparators(targetDir));
        }
        return false;
    }

    const QString lrcPath = dir.filePath(baseName + QStringLiteral(".lrc"));

    QFile file(lrcPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无权限保存歌词：%1").arg(file.errorString());
        }
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << lyricText;
    file.close();
    if (savedPath) {
        *savedPath = lrcPath;
    }
    return true;
}

void LyricDownloadService::autoDownloadLyricAsync(const QString& audioFilePath)
{
    m_lastError.clear();

    const SongInfo info = parseSongInfo(audioFilePath);
    emit downloadProgress(QStringLiteral("正在搜索歌词：%1 %2").arg(info.artist, info.title).trimmed());
    searchAndDownloadLyricAsync(info.title, info.artist, audioFilePath);
}

void LyricDownloadService::searchAndDownloadLyricAsync(const QString& songName,
                                                       const QString& artistName,
                                                       const QString& audioFilePath)
{
    m_lastError.clear();

    QString keyword = songName.trimmed();
    if (!artistName.trimmed().isEmpty()) {
        keyword = artistName.trimmed() + QLatin1Char(' ') + keyword;
    }

    if (keyword.trimmed().isEmpty()) {
        finish(audioFilePath, false, QStringLiteral("歌曲名称为空，无法搜索歌词。"));
        return;
    }

    const QString searchUrl =
        QStringLiteral("https://netease-cloud-music-api-psi-drab.vercel.app/search?keywords=%1&limit=1")
            .arg(QString::fromLatin1(QUrl::toPercentEncoding(keyword)));

    QVariantMap headers;
    headers.insert(QStringLiteral("User-Agent"), QStringLiteral("QtMediaPlayer/1.0"));

    RequestOptions options;
    options.headers = headers;
    options.timeout = 15000;
    options.retry = 1;
    const QString requestId = m_networkClient->get(QUrl(searchUrl), options);
    PendingRequestContext context;
    context.stage = RequestStage::SearchSong;
    context.audioFilePath = audioFilePath;
    m_pendingRequests.insert(requestId, context);
}

void LyricDownloadService::downloadLyricByIdAsync(int songId, const QString& audioFilePath)
{
    const QString lyricUrl =
        QStringLiteral("https://netease-cloud-music-api-psi-drab.vercel.app/lyric?id=%1").arg(songId);

    QVariantMap headers;
    headers.insert(QStringLiteral("User-Agent"), QStringLiteral("QtMediaPlayer/1.0"));

    RequestOptions options;
    options.headers = headers;
    options.timeout = 15000;
    options.retry = 1;
    const QString requestId = m_networkClient->get(QUrl(lyricUrl), options);
    PendingRequestContext context;
    context.stage = RequestStage::FetchLyric;
    context.audioFilePath = audioFilePath;
    m_pendingRequests.insert(requestId, context);
}

void LyricDownloadService::finish(const QString& audioFilePath,
                                  bool success,
                                  const QString& message,
                                  const QString& lyricText,
                                  const ServiceError& error)
{
    m_lastError = success ? QString() : message;

    if (success) {
        emit downloadFinished(lyricText);
    } else {
        const ServiceError serviceError = error.isEmpty()
            ? ServiceError{QStringLiteral("lyric.download.failed"), message, false}
            : error;
        emit downloadFailed(serviceError);
        emit downloadError(serviceError);
        emit downloadError(message);
    }

    emit lyricDownloaded(audioFilePath, success, message);
}

void LyricDownloadService::handleSearchResponse(const NetworkResult& result,
                                                const PendingRequestContext& context)
{
    if (!result.ok()) {
        const QString message = QStringLiteral("搜索歌词失败：%1").arg(result.errorMessage);
        finish(context.audioFilePath,
               false,
               message,
               QString(),
               result.toServiceError(QStringLiteral("lyric.search.network"), message));
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(result.body);
    if (doc.isNull() || !doc.isObject()) {
        finish(context.audioFilePath, false, QStringLiteral("解析搜索结果失败。"));
        return;
    }

    const QJsonObject obj = doc.object();
    const QJsonObject resultObject = obj.value(QStringLiteral("result")).toObject();
    const QJsonArray songs = resultObject.value(QStringLiteral("songs")).toArray();

    if (songs.isEmpty()) {
        finish(context.audioFilePath, false, QStringLiteral("未找到歌曲。"));
        return;
    }

    const int songId = songs.first().toObject().value(QStringLiteral("id")).toInt();
    if (songId <= 0) {
        finish(context.audioFilePath, false, QStringLiteral("歌曲 ID 无效。"));
        return;
    }

    emit downloadProgress(QStringLiteral("已找到歌曲，正在下载歌词..."));
    downloadLyricByIdAsync(songId, context.audioFilePath);
}

void LyricDownloadService::handleLyricResponse(const NetworkResult& result,
                                               const PendingRequestContext& context)
{
    if (!result.ok()) {
        const QString message = QStringLiteral("下载歌词失败：%1").arg(result.errorMessage);
        finish(context.audioFilePath,
               false,
               message,
               QString(),
               result.toServiceError(QStringLiteral("lyric.fetch.network"), message));
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(result.body);
    if (doc.isNull() || !doc.isObject()) {
        finish(context.audioFilePath, false, QStringLiteral("解析歌词失败。"));
        return;
    }

    const QJsonObject obj = doc.object();
    const QString lyricText = obj.value(QStringLiteral("lrc")).toObject().value(QStringLiteral("lyric")).toString();
    if (lyricText.trimmed().isEmpty()) {
        finish(context.audioFilePath, false, QStringLiteral("歌词内容为空。"));
        return;
    }

    QString savedPath;
    QString saveError;
    if (!context.audioFilePath.isEmpty()
        && !saveLyricToFile(lyricText,
                            context.audioFilePath,
                            LyricSaveStrategy::AppDataLocation,
                            &savedPath,
                            &saveError)) {
        const QString message = saveError.trimmed().isEmpty()
            ? QStringLiteral("无权限保存歌词。")
            : saveError.trimmed();
        finish(context.audioFilePath,
               false,
               message,
               QString(),
               ServiceError{QStringLiteral("lyric.save.permission"), message, false});
        return;
    }

    finish(context.audioFilePath, true, QStringLiteral("歌词下载成功。"), lyricText);
}

void LyricDownloadService::onRequestFinished(const QString& requestId, const NetworkResult& result)
{
    if (!m_pendingRequests.contains(requestId)) {
        return;
    }

    const PendingRequestContext context = m_pendingRequests.take(requestId);
    switch (context.stage) {
    case RequestStage::SearchSong:
        handleSearchResponse(result, context);
        break;
    case RequestStage::FetchLyric:
        handleLyricResponse(result, context);
        break;
    }
}
