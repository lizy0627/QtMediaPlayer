#include "onlinemusicservice.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUrl>

#include "networkclient.h"
#include "onlinemusicsearch.h"

namespace {
void emitSearchFailure(OnlineMusicService* service, const ServiceError& error)
{
    emit service->searchFailed(error);
    emit service->searchError(error.message);
}
}

OnlineMusicService::OnlineMusicService(QObject* parent)
    : QObject(parent)
    , m_networkClient(new NetworkClient(this))
{
    connect(m_networkClient, &NetworkClient::requestFinished,
            this, &OnlineMusicService::onRequestFinished);
}

void OnlineMusicService::searchMusic(QString keyword)
{
    searchSongsAsync(keyword);
}

void OnlineMusicService::showSearchDialog(QWidget* parent)
{
    OnlineMusicSearch searchDialog(this, parent);
    connect(&searchDialog, &OnlineMusicSearch::songSelected,
            this, &OnlineMusicService::songSelected);
    searchDialog.exec();
}

void OnlineMusicService::search(const QString& keyword)
{
    searchSongsAsync(keyword);
}

void OnlineMusicService::searchSongsAsync(const QString& keyword)
{
    const QString encodedKeyword = QString::fromLatin1(QUrl::toPercentEncoding(keyword));
    const QString apiUrl = QStringLiteral("https://music.163.com/api/search/get/web?s=%1&type=1&offset=0&limit=30")
        .arg(encodedKeyword);

    QVariantMap headers;
    headers.insert("User-Agent",
                   "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    headers.insert("Referer", "https://music.163.com");

    RequestOptions options;
    options.headers = headers;
    options.timeout = 15000;
    options.retry = 1;
    m_pendingSearchRequestId = m_networkClient->get(QUrl(apiUrl), options);
}

void OnlineMusicService::resolveSongUrlAsync(const QString& songId)
{
    const QString trimmedSongId = songId.trimmed();
    if (trimmedSongId.isEmpty()) {
        emit resolveError(trimmedSongId, QStringLiteral("播放地址不可用"));
        return;
    }

    const QString encodedSongId = QString::fromLatin1(QUrl::toPercentEncoding(trimmedSongId));
    const QString apiUrl = QStringLiteral("https://music.163.com/api/song/enhance/player/url?id=%1&ids=[%1]&br=320000")
        .arg(encodedSongId);

    QVariantMap headers;
    headers.insert("User-Agent",
                   "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    headers.insert("Referer", "https://music.163.com");

    RequestOptions options;
    options.headers = headers;
    options.timeout = 15000;
    options.retry = 1;
    const QString requestId = m_networkClient->get(QUrl(apiUrl), options);
    m_pendingResolveRequestIds.insert(requestId, trimmedSongId);
}

void OnlineMusicService::onRequestFinished(const QString& requestId, const NetworkResult& result)
{
    if (m_pendingResolveRequestIds.contains(requestId)) {
        const QString songId = m_pendingResolveRequestIds.take(requestId);

        if (!result.ok()) {
            emit resolveError(songId, QStringLiteral("播放地址不可用"));
            return;
        }

        QString statusMessage;
        bool parseOk = false;
        const SongInfo song = parseResolvedSongUrl(result.body, songId, &statusMessage, &parseOk);
        if (!parseOk || song.url.trimmed().isEmpty()) {
            emit resolveError(songId,
                              statusMessage.isEmpty()
                                  ? QStringLiteral("播放地址不可用")
                                  : statusMessage);
            return;
        }

        emit resolveFinished(song);
        return;
    }

    if (requestId != m_pendingSearchRequestId) {
        return;
    }

    m_pendingSearchRequestId.clear();

    if (!result.ok()) {
        const QString message = QStringLiteral("搜索失败：%1。")
                                    .arg(result.errorMessage.isEmpty()
                                             ? QStringLiteral("网络请求未成功")
                                             : result.errorMessage);
        emitSearchFailure(this, result.toServiceError(QStringLiteral("music.search.network"), message));
        return;
    }

    QString statusMessage;
    bool parseOk = false;
    QList<SongInfo> songs = parseSearchResults(result.body, &statusMessage, &parseOk);
    if (!parseOk) {
        const QString message = statusMessage.isEmpty()
            ? QStringLiteral("解析搜索结果失败。")
            : statusMessage;
        emitSearchFailure(this, {QStringLiteral("music.search.parse"), message, false});
        return;
    }

    if (songs.isEmpty()) {
        if (statusMessage.isEmpty()) {
            statusMessage = QStringLiteral("未找到相关歌曲。");
        }
    }

    QStringList legacyResults;
    for (const SongInfo& song : songs) {
        legacyResults << QStringLiteral("%1 - %2")
                             .arg(song.title.isEmpty() ? song.name : song.title,
                                  song.artist);
    }
    emit searchFinished(legacyResults);
    emit searchFinished(songs, statusMessage);
}

QList<SongInfo> OnlineMusicService::parseSearchResults(const QByteArray& data,
                                                       QString* statusMessage,
                                                       bool* parseOk) const
{
    if (parseOk) {
        *parseOk = false;
    }

    QList<SongInfo> resultSongs;
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (doc.isNull() || !doc.isObject()) {
        if (statusMessage) {
            *statusMessage = parseError.error == QJsonParseError::NoError
                ? QStringLiteral("解析搜索结果失败。")
                : QStringLiteral("解析搜索结果失败：%1。").arg(parseError.errorString());
        }
        return resultSongs;
    }

    if (parseOk) {
        *parseOk = true;
    }

    const QJsonObject root = doc.object();
    const QJsonObject result = root.value(QStringLiteral("result")).toObject();
    const QJsonArray songs = result.value(QStringLiteral("songs")).toArray();

    if (songs.isEmpty()) {
        if (statusMessage) {
            *statusMessage = QStringLiteral("未找到相关歌曲。");
        }
        return resultSongs;
    }

    for (const QJsonValue& value : songs) {
        const QJsonObject songObj = value.toObject();

        SongInfo song;
        song.id = QString::number(songObj.value(QStringLiteral("id")).toInt());
        song.sourceId = song.id;
        song.sourceName = QStringLiteral("netease");
        song.title = songObj.value(QStringLiteral("name")).toString();
        song.name = song.title;
        song.duration = songObj.value(QStringLiteral("duration")).toInt() / 1000;

        const QJsonArray artists = songObj.value(QStringLiteral("artists")).toArray();
        QStringList artistNames;
        for (const QJsonValue& artist : artists) {
            artistNames << artist.toObject().value(QStringLiteral("name")).toString();
        }
        song.artist = artistNames.join(QStringLiteral(", "));

        const QJsonObject album = songObj.value(QStringLiteral("album")).toObject();
        song.album = album.value(QStringLiteral("name")).toString();
        song.sourceUrl = QStringLiteral("https://music.163.com/#/song?id=%1").arg(song.id);
        song.lyricUrl = QStringLiteral("https://music.163.com/api/song/lyric?id=%1&lv=1&kv=1&tv=-1")
                            .arg(song.id);

        resultSongs.append(song);
    }

    if (statusMessage) {
        *statusMessage = resultSongs.isEmpty()
            ? QStringLiteral("未找到相关歌曲。")
            : QStringLiteral("找到 %1 首候选歌曲。").arg(resultSongs.size());
    }

    return resultSongs;
}

SongInfo OnlineMusicService::parseResolvedSongUrl(const QByteArray& data,
                                                  const QString& songId,
                                                  QString* statusMessage,
                                                  bool* parseOk) const
{
    if (parseOk) {
        *parseOk = false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (doc.isNull() || !doc.isObject()) {
        if (statusMessage) {
            *statusMessage = QStringLiteral("播放地址不可用");
        }
        return SongInfo();
    }

    const QJsonArray dataArray = doc.object().value(QStringLiteral("data")).toArray();
    if (dataArray.isEmpty()) {
        if (statusMessage) {
            *statusMessage = QStringLiteral("播放地址不可用");
        }
        return SongInfo();
    }

    const QJsonObject songObj = dataArray.first().toObject();
    const QString resolvedUrl = songObj.value(QStringLiteral("url")).toString().trimmed();
    const QUrl url(resolvedUrl);
    if (resolvedUrl.isEmpty()
        || !url.isValid()
        || (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https"))
        || url.host().isEmpty()) {
        if (statusMessage) {
            *statusMessage = QStringLiteral("播放地址不可用");
        }
        return SongInfo();
    }

    SongInfo song;
    song.id = songId;
    song.sourceId = songId;
    song.sourceName = QStringLiteral("netease");
    song.url = resolvedUrl;
    song.sourceUrl = QStringLiteral("https://music.163.com/#/song?id=%1").arg(songId);
    song.lyricUrl = QStringLiteral("https://music.163.com/api/song/lyric?id=%1&lv=1&kv=1&tv=-1")
                        .arg(songId);

    if (parseOk) {
        *parseOk = true;
    }
    return song;
}
