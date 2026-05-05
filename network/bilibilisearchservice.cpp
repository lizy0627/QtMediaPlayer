#include "bilibilisearchservice.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

namespace {
QString bilibiliUserAgent()
{
    return QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                          "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
}
}

BilibiliSearchService::BilibiliSearchService(QObject* parent)
    : QObject(parent)
    , m_networkClient(new NetworkClient(this))
{
    connect(m_networkClient,
            &NetworkClient::requestFinished,
            this,
            &BilibiliSearchService::onRequestFinished);
}

QVariantMap BilibiliSearchService::bilibiliHeaders()
{
    QVariantMap headers;
    headers.insert(QStringLiteral("User-Agent"), bilibiliUserAgent());
    headers.insert(QStringLiteral("Referer"), QStringLiteral("https://www.bilibili.com/"));
    headers.insert(QStringLiteral("Origin"), QStringLiteral("https://www.bilibili.com"));
    headers.insert(QStringLiteral("Accept"), QStringLiteral("application/json, text/plain, */*"));
    headers.insert(QStringLiteral("Accept-Language"), QStringLiteral("zh-CN,zh;q=0.9,en;q=0.8"));
    return headers;
}

void BilibiliSearchService::searchVideo(const QString& keyword)
{
    const QString trimmedKeyword = keyword.trimmed();
    if (trimmedKeyword.isEmpty()) {
        emit searchFailed({QStringLiteral("video.search.empty_keyword"),
                           QStringLiteral("请输入视频搜索关键词。"),
                           false});
        return;
    }

    QUrl url(QStringLiteral("https://api.bilibili.com/x/web-interface/search/type"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("search_type"), QStringLiteral("video"));
    query.addQueryItem(QStringLiteral("keyword"), trimmedKeyword);
    query.addQueryItem(QStringLiteral("page"), QStringLiteral("1"));
    url.setQuery(query);

    emit searchStarted(trimmedKeyword);
    RequestOptions options;
    options.headers = bilibiliHeaders();
    options.timeout = 30000;
    options.retry = 2;
    m_pendingSearchRequestId = m_networkClient->get(url, options);
}

void BilibiliSearchService::onRequestFinished(const QString& requestId, const NetworkResult& result)
{
    if (requestId != m_pendingSearchRequestId) {
        return;
    }

    m_pendingSearchRequestId.clear();

    if (!result.ok()) {
        QString message = result.errorMessage;
        if (message.isEmpty()) {
            message = QStringLiteral("视频搜索请求失败。");
        }
        emit searchFailed(result.toServiceError(QStringLiteral("video.search.network"), message));
        return;
    }

    QString errorMessage;
    const QList<VideoInfo> videos = parseSearchResults(result.body, &errorMessage);
    if (!errorMessage.isEmpty()) {
        emit searchFailed({QStringLiteral("video.search.parse"), errorMessage, false});
        return;
    }

    const QString statusMessage = videos.isEmpty()
        ? QStringLiteral("未找到相关视频。")
        : QStringLiteral("找到 %1 个视频，双击可发送到播放器，也可以在浏览器中打开。").arg(videos.size());
    emit searchFinished(videos, statusMessage);
}

QString BilibiliSearchService::removeHtmlTags(const QString& html)
{
    QString result = html;
    result.replace(QRegularExpression(QStringLiteral("<[^>]*>")), QString());
    result.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    result.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    result.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    result.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    return result.trimmed();
}

QString BilibiliSearchService::formatPlayCount(int count)
{
    if (count >= 10000) {
        return QStringLiteral("%1万").arg(count / 10000.0, 0, 'f', 1);
    }
    return QString::number(count);
}

QList<VideoInfo> BilibiliSearchService::parseSearchResults(const QByteArray& data,
                                                           QString* errorMessage) const
{
    QList<VideoInfo> videos;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("视频搜索响应解析失败：%1").arg(parseError.errorString());
        }
        return videos;
    }

    const QJsonObject root = document.object();
    const int code = root.value(QStringLiteral("code")).toInt(-1);
    if (code != 0) {
        if (errorMessage) {
            const QString apiMessage = root.value(QStringLiteral("message")).toString().trimmed();
            *errorMessage = apiMessage.isEmpty()
                ? QStringLiteral("Bilibili 搜索接口返回错误。")
                : apiMessage;
        }
        return videos;
    }

    const QJsonObject dataObject = root.value(QStringLiteral("data")).toObject();
    const QJsonArray results = dataObject.value(QStringLiteral("result")).toArray();

    for (const QJsonValue& value : results) {
        const QJsonObject item = value.toObject();

        VideoInfo video;
        video.bvid = item.value(QStringLiteral("bvid")).toString().trimmed();
        video.id = video.bvid;
        video.title = removeHtmlTags(item.value(QStringLiteral("title")).toString());
        video.author = item.value(QStringLiteral("author")).toString().trimmed();
        video.description = removeHtmlTags(item.value(QStringLiteral("description")).toString());
        video.cid = item.value(QStringLiteral("cid")).toVariant().toLongLong();
        video.play = item.value(QStringLiteral("play")).toInt();
        video.url = item.value(QStringLiteral("url")).toString().trimmed();
        if (video.url.isEmpty()) {
            video.url = item.value(QStringLiteral("arcurl")).toString().trimmed();
        }
        video.pageUrl = item.value(QStringLiteral("arcurl")).toString().trimmed();
        if (video.pageUrl.isEmpty()) {
            video.pageUrl = QStringLiteral("https://www.bilibili.com/video/%1").arg(video.bvid);
        }
        video.thumbnail = item.value(QStringLiteral("pic")).toString().trimmed();

        const QJsonValue durationValue = item.value(QStringLiteral("duration"));
        if (durationValue.isString()) {
            video.duration = durationValue.toString().trimmed();
        } else if (durationValue.isDouble()) {
            const int seconds = durationValue.toInt();
            video.duration = QStringLiteral("%1:%2")
                                 .arg(seconds / 60)
                                 .arg(seconds % 60, 2, 10, QChar('0'));
        }

        if (video.bvid.isEmpty() || video.title.isEmpty()) {
            continue;
        }

        if (video.author.isEmpty()) {
            video.author = QStringLiteral("未知作者");
        }
        if (video.duration.isEmpty()) {
            video.duration = QStringLiteral("未知时长");
        }
        if (video.description.isEmpty()) {
            video.description = QStringLiteral("播放量：%1").arg(formatPlayCount(video.play));
        }

        videos.append(video);
    }

    return videos;
}
