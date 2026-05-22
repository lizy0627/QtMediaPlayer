#include "onlinemusicservice.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMap>
#include <QUrl>

#include "networkclient.h"
#include "onlinemusicsearch.h"

namespace {
constexpr int SearchPage = 1;
constexpr int SearchPageSize = 30;

void emitSearchFailure(OnlineMusicService* service, const ServiceError& error)
{
    emit service->searchFailed(error);
    emit service->searchError(error.message);
}

QString musicResolveNetworkMessage(const NetworkResult& result)
{
    const QString detail = result.errorMessage.trimmed().isEmpty()
        ? QStringLiteral("\u7b2c\u4e09\u65b9\u97f3\u4e50\u63a5\u53e3\u8bf7\u6c42\u5931\u8d25\u3002")
        : result.errorMessage.trimmed();

    if (result.timedOut) {
        return QStringLiteral("\u7f51\u7edc\u9519\u8bef\uff1a\u89e3\u6790\u5728\u7ebf\u6b4c\u66f2\u64ad\u653e\u5730\u5740\u8d85\u65f6\u3002\n%1").arg(detail);
    }
    if (result.networkErrorCode != 0) {
        return QStringLiteral("\u7f51\u7edc\u9519\u8bef\uff1a\u65e0\u6cd5\u8bbf\u95ee\u7b2c\u4e09\u65b9\u97f3\u4e50\u63a5\u53e3\u3002\n%1").arg(detail);
    }
    if (result.httpStatus >= 400) {
        return QStringLiteral("\u8d44\u6e90\u9519\u8bef\uff1a\u7b2c\u4e09\u65b9\u97f3\u4e50\u63a5\u53e3\u62d2\u7edd\u8bf7\u6c42\u6216\u8fd4\u56de HTTP %1\u3002\n%2")
            .arg(result.httpStatus)
            .arg(detail);
    }
    return QStringLiteral("\u89e3\u6790\u64ad\u653e\u5730\u5740\u5931\u8d25\uff1a%1").arg(detail);
}

QString invalidMusicDirectUrlMessage(const QString& detail = QString())
{
    QString message = QStringLiteral("\u64ad\u653e\u5730\u5740\u65e0\u6548\uff1a\u7b2c\u4e09\u65b9\u63a5\u53e3\u672a\u8fd4\u56de\u53ef\u64ad\u653e\u7684 http/https \u97f3\u9891\u76f4\u94fe\uff0c\u53ef\u80fd\u662f\u7248\u6743\u9650\u5236\u3001\u5730\u5740\u8fc7\u671f\u6216\u9700\u8981\u5e73\u53f0\u767b\u5f55\u3002");
    const QString trimmedDetail = detail.trimmed();
    if (!trimmedDetail.isEmpty()) {
        message += QLatin1Char('\n') + trimmedDetail;
    }
    return message;
}
}

OnlineMusicService::OnlineMusicService(QObject* parent)
    : QObject(parent)
    , m_networkClient(new NetworkClient(this))
    , m_searchCache(QStringLiteral("music"))
{
    connect(m_networkClient, &NetworkClient::requestFinished,
            this, &OnlineMusicService::onRequestFinished);
    m_searchCache.init();
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

QString OnlineMusicService::buildSearchCacheKey(const QString& keyword)
{
    QMap<QString, QString> parameters;
    parameters.insert(QStringLiteral("page"), QString::number(SearchPage));
    parameters.insert(QStringLiteral("pageSize"), QString::number(SearchPageSize));
    parameters.insert(QStringLiteral("searchType"), QStringLiteral("song"));
    return SearchCache::buildKey(QStringLiteral("netease"), keyword, parameters);
}

void OnlineMusicService::searchSongsAsync(const QString& keyword)
{
    const QString trimmedKeyword = keyword.trimmed();
    if (trimmedKeyword.isEmpty()) {
        emitSearchFailure(this, {QStringLiteral("music.search.empty_keyword"),
                                 QStringLiteral("请输入搜索关键词。"),
                                 false});
        return;
    }

    const QString cacheKey = buildSearchCacheKey(trimmedKeyword);
    if (m_searchCache.hasValidCache(cacheKey)) {
        const QString cachedJson = m_searchCache.getCache(cacheKey);
        if (!cachedJson.isEmpty()
            && emitSearchResultsFromJson(cachedJson.toUtf8())) {
            return;
        }
    }

    // 相同关键词优先复用本地缓存，避免重复网络请求。
    if (m_searchCache.hasValidCache(trimmedKeyword)) {
        const QString cachedJson = m_searchCache.getCache(trimmedKeyword);
        if (!cachedJson.isEmpty()
            && emitSearchResultsFromJson(cachedJson.toUtf8())) {
            return;
        }
    }

    const QString encodedKeyword = QString::fromLatin1(QUrl::toPercentEncoding(trimmedKeyword));
    const QString apiUrl = QStringLiteral("https://music.163.com/api/search/get/web?s=%1&type=1&offset=0&limit=%2")
        .arg(encodedKeyword, QString::number(SearchPageSize));

    QVariantMap headers;
    headers.insert("User-Agent",
                   "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    headers.insert("Referer", "https://music.163.com");

    RequestOptions options;
    options.headers = headers;
    options.timeout = 15000;
    options.retry = 1;
    m_pendingSearchKeyword = trimmedKeyword;
    m_pendingSearchCacheKey = cacheKey;
    m_pendingSearchRequestId = m_networkClient->get(QUrl(apiUrl), options);
}

void OnlineMusicService::resolveSongUrlAsync(const QString& songId)
{
    const QString trimmedSongId = songId.trimmed();
    if (trimmedSongId.isEmpty()) {
        emit resolveError(trimmedSongId,
                          QStringLiteral("\u89e3\u6790\u64ad\u653e\u5730\u5740\u5931\u8d25\uff1a\u7f3a\u5c11\u7b2c\u4e09\u65b9\u6b4c\u66f2 ID\u3002"));
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
            emit resolveError(songId, musicResolveNetworkMessage(result));
            return;
        }

        QString statusMessage;
        bool parseOk = false;
        const SongInfo song = parseResolvedSongUrl(result.body, songId, &statusMessage, &parseOk);
        if (!parseOk || song.url.trimmed().isEmpty()) {
            emit resolveError(songId,
                              statusMessage.isEmpty()
                                  ? invalidMusicDirectUrlMessage()
                                  : statusMessage);
            return;
        }

        emit resolveFinished(song);
        return;
    }

    if (requestId != m_pendingSearchRequestId) {
        return;
    }

    const QString pendingKeyword = m_pendingSearchKeyword;
    const QString pendingCacheKey = m_pendingSearchCacheKey;
    m_pendingSearchRequestId.clear();
    m_pendingSearchKeyword.clear();
    m_pendingSearchCacheKey.clear();

    QString staleSearchJson = m_searchCache.getCache(pendingCacheKey);
    if (staleSearchJson.isEmpty()) {
        staleSearchJson = m_searchCache.getCache(pendingKeyword);
    }

    if (!result.ok()) {
        if (!staleSearchJson.isEmpty()) {
            qWarning() << "Music search network failed, trying stale cache for"
                       << pendingCacheKey;
        }
        if (!staleSearchJson.isEmpty()
            && emitSearchResultsFromJson(staleSearchJson.toUtf8(),
                                         QStringLiteral("（缓存数据可能不是最新）"))) {
            return;
        }

        const QString message = QStringLiteral("搜索失败：%1。")
                                    .arg(result.errorMessage.isEmpty()
                                             ? QStringLiteral("网络请求未成功")
                                             : result.errorMessage);
        emitSearchFailure(this, result.toServiceError(QStringLiteral("music.search.network"), message));
        return;
    }

    if (emitSearchResultsFromJson(result.body)) {
        m_searchCache.saveCache(pendingCacheKey, QString::fromUtf8(result.body));
    }
}

bool OnlineMusicService::emitSearchResultsFromJson(const QByteArray& data,
                                                   const QString& statusSuffix)
{
    QString statusMessage;
    bool parseOk = false;
    QList<SongInfo> songs = parseSearchResults(data, &statusMessage, &parseOk);
    if (!parseOk) {
        const QString message = statusMessage.isEmpty()
            ? QStringLiteral("解析搜索结果失败。")
            : statusMessage;
        emitSearchFailure(this, {QStringLiteral("music.search.parse"), message, false});
        return false;
    }

    if (songs.isEmpty() && statusMessage.isEmpty()) {
        statusMessage = QStringLiteral("未找到相关歌曲。");
    }
    if (!statusSuffix.isEmpty()) {
        statusMessage += statusSuffix;
    }

    QStringList legacyResults;
    for (const SongInfo& song : songs) {
        legacyResults << QStringLiteral("%1 - %2")
                             .arg(song.title.isEmpty() ? song.name : song.title,
                                  song.artist);
    }
    emit searchFinished(legacyResults);
    emit searchFinished(songs, statusMessage);
    return true;
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
            const QString parseDetail = parseError.error == QJsonParseError::NoError
                ? QStringLiteral("\u54cd\u5e94\u4f53\u4e0d\u662f JSON \u5bf9\u8c61\u3002")
                : parseError.errorString();
            *statusMessage = QStringLiteral("\u683c\u5f0f\u9519\u8bef\uff1a\u7b2c\u4e09\u65b9\u97f3\u4e50\u63a5\u53e3\u8fd4\u56de\u7684\u4e0d\u662f\u53ef\u89e3\u6790\u7684 JSON\u3002\n%1")
                                 .arg(parseDetail);
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
            *statusMessage = QStringLiteral("\u683c\u5f0f\u9519\u8bef\uff1a\u7b2c\u4e09\u65b9\u97f3\u4e50\u63a5\u53e3\u8fd4\u56de\u7684\u4e0d\u662f\u53ef\u89e3\u6790\u7684 JSON\u3002");
            return SongInfo();
        }
        return SongInfo();
    }

    const QJsonArray dataArray = doc.object().value(QStringLiteral("data")).toArray();
    if (dataArray.isEmpty()) {
        if (statusMessage) {
            *statusMessage = invalidMusicDirectUrlMessage(QStringLiteral("\u63a5\u53e3\u54cd\u5e94\u4e2d\u6ca1\u6709 data \u64ad\u653e\u6570\u636e\u3002"));
            return SongInfo();
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
            *statusMessage = invalidMusicDirectUrlMessage(resolvedUrl);
            return SongInfo();
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
