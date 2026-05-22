#include "bilibiliplaybackresolver.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>
#include <QtGlobal>

namespace {
constexpr int kBilibiliRequestTimeoutMs = 12000;

QString browserOnlyText()
{
    return QStringLiteral("\n\n\u53ea\u80fd\u5728\u6d4f\u89c8\u5668\u6253\u5f00\uff1a\u5e73\u53f0\u672a\u8fd4\u56de\u53ef\u76f4\u63a5\u4ea4\u7ed9 QMediaPlayer \u7684\u5355\u4e00 http/https \u5a92\u4f53\u76f4\u94fe\u3002");
}

QString bilibiliUserAgent()
{
    return QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                          "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
}

bool isPageOnlyUrl(const QUrl& mediaUrl, const QString& pageUrl)
{
    if (!mediaUrl.isValid()) {
        return false;
    }

    const QUrl normalizedPageUrl = QUrl::fromUserInput(pageUrl);
    if (normalizedPageUrl.isValid() && mediaUrl == normalizedPageUrl) {
        return true;
    }

    const QString host = mediaUrl.host().toLower();
    const QString path = mediaUrl.path().toLower();
    return host.endsWith(QStringLiteral("bilibili.com"))
        && (path.startsWith(QStringLiteral("/video/"))
            || path.startsWith(QStringLiteral("/bangumi/play/")));
}

QString firstNonEmptyUrl(const QJsonObject& item)
{
    const QString url = item.value(QStringLiteral("url")).toString().trimmed();
    if (!url.isEmpty()) {
        return url;
    }

    const QJsonArray backupUrls = item.value(QStringLiteral("backup_url")).toArray();
    for (const QJsonValue& backupValue : backupUrls) {
        const QString backupUrl = backupValue.toString().trimmed();
        if (!backupUrl.isEmpty()) {
            return backupUrl;
        }
    }

    return {};
}

QString apiMessage(const QJsonObject& root)
{
    QString message = root.value(QStringLiteral("message")).toString().trimmed();
    if (message.isEmpty()) {
        message = root.value(QStringLiteral("msg")).toString().trimmed();
    }
    return message;
}

QJsonObject payloadObject(const QJsonObject& root)
{
    if (root.value(QStringLiteral("data")).isObject()) {
        return root.value(QStringLiteral("data")).toObject();
    }
    if (root.value(QStringLiteral("result")).isObject()) {
        return root.value(QStringLiteral("result")).toObject();
    }
    return {};
}

int pageNumberFromUrl(const QString& pageUrl)
{
    const QUrl url = QUrl::fromUserInput(pageUrl);
    const bool hasPageQuery = url.isValid() && QUrlQuery(url).hasQueryItem(QStringLiteral("p"));
    if (!hasPageQuery) {
        return 1;
    }

    bool ok = false;
    const int page = QUrlQuery(url).queryItemValue(QStringLiteral("p")).toInt(&ok);
    return ok && page > 0 ? page : 1;
}

QUrl bilibiliPagelistUrl(const QString& bvid)
{
    QUrl url(QStringLiteral("https://api.bilibili.com/x/player/pagelist"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("bvid"), bvid);
    query.addQueryItem(QStringLiteral("jsonp"), QStringLiteral("jsonp"));
    url.setQuery(query);
    return url;
}

QUrl bilibiliPlayUrl(const QString& bvid, qint64 cid, bool html5SingleStream)
{
    QUrl url(QStringLiteral("https://api.bilibili.com/x/player/playurl"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("bvid"), bvid);
    query.addQueryItem(QStringLiteral("cid"), QString::number(cid));
    query.addQueryItem(QStringLiteral("qn"), html5SingleStream ? QStringLiteral("16") : QStringLiteral("64"));
    query.addQueryItem(QStringLiteral("fnver"), QStringLiteral("0"));
    query.addQueryItem(QStringLiteral("fnval"), html5SingleStream ? QStringLiteral("1") : QStringLiteral("16"));
    query.addQueryItem(QStringLiteral("fourk"), QStringLiteral("0"));
    query.addQueryItem(QStringLiteral("otype"), QStringLiteral("json"));
    if (html5SingleStream) {
        query.addQueryItem(QStringLiteral("platform"), QStringLiteral("html5"));
        query.addQueryItem(QStringLiteral("high_quality"), QStringLiteral("1"));
    }
    url.setQuery(query);
    return url;
}

bool isSuccessfulBilibiliResponse(const QJsonObject& root, QString* errorMessage)
{
    const int code = root.value(QStringLiteral("code")).toInt(-1);
    if (code == 0) {
        return true;
    }

    if (errorMessage) {
        const QString message = apiMessage(root);
        *errorMessage = message.isEmpty()
            ? QStringLiteral("Bilibili 接口返回错误 code=%1。").arg(code)
            : QStringLiteral("Bilibili 接口返回错误 code=%1：%2").arg(code).arg(message);
    }
    return false;
}

QUrl resolveSingleStreamUrl(const QJsonObject& playData, QString* errorMessage)
{
    const QJsonArray durl = playData.value(QStringLiteral("durl")).toArray();
    if (durl.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Bilibili 未返回可直接播放的单流 durl。");
        }
        return {};
    }

    if (durl.size() > 1) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Bilibili 返回了多段 durl，当前播放链路不能自动串联多段媒体。");
        }
        return {};
    }

    const QString mediaUrlText = firstNonEmptyUrl(durl.first().toObject());
    const QUrl mediaUrl = QUrl::fromUserInput(mediaUrlText);
    if (mediaUrlText.isEmpty() || !mediaUrl.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Bilibili 返回的 durl 媒体地址无效。");
        }
        return {};
    }

    return mediaUrl;
}

bool responseContainsDash(const QJsonObject& playData)
{
    const QJsonObject dash = playData.value(QStringLiteral("dash")).toObject();
    return dash.value(QStringLiteral("video")).isArray()
        || dash.value(QStringLiteral("audio")).isArray();
}

QJsonObject jsonObjectFromResult(const NetworkResult& result, QString* errorMessage)
{
    if (!result.ok()) {
        if (errorMessage) {
            const QString detail = result.errorMessage.isEmpty()
                ? QStringLiteral("Bilibili \u63a5\u53e3\u8bf7\u6c42\u5931\u8d25\u3002")
                : result.errorMessage;
            if (result.timedOut || result.networkErrorCode != 0) {
                *errorMessage = QStringLiteral("\u7f51\u7edc\u9519\u8bef\uff1a\u65e0\u6cd5\u8bbf\u95ee Bilibili \u64ad\u653e\u63a5\u53e3\u3002\n%1").arg(detail);
            } else if (result.httpStatus >= 400) {
                *errorMessage = QStringLiteral("\u8d44\u6e90\u9519\u8bef\uff1aBilibili \u64ad\u653e\u63a5\u53e3\u8fd4\u56de HTTP %1\u3002\n%2")
                                    .arg(result.httpStatus)
                                    .arg(detail);
            } else {
                *errorMessage = detail;
            }
            return {};
        }
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(result.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("\u683c\u5f0f\u9519\u8bef\uff1aBilibili \u63a5\u53e3\u54cd\u5e94\u4e0d\u662f\u53ef\u89e3\u6790\u7684 JSON\u3002\n%1")
                                .arg(parseError.errorString());
            return {};
        }
        return {};
    }

    return document.object();
}
}

BilibiliPlaybackResolver::BilibiliPlaybackResolver(QObject* parent)
    : QObject(parent)
    , m_networkClient(new NetworkClient(this))
{
    connect(m_networkClient,
            &NetworkClient::requestFinished,
            this,
            &BilibiliPlaybackResolver::onRequestFinished);
}

QVariantMap BilibiliPlaybackResolver::bilibiliHeaders()
{
    QVariantMap headers;
    headers.insert(QStringLiteral("User-Agent"), bilibiliUserAgent());
    headers.insert(QStringLiteral("Referer"), QStringLiteral("https://www.bilibili.com/"));
    headers.insert(QStringLiteral("Accept"), QStringLiteral("application/json, text/plain, */*"));
    headers.insert(QStringLiteral("Accept-Language"), QStringLiteral("zh-CN,zh;q=0.9,en;q=0.8"));
    return headers;
}

QString BilibiliPlaybackResolver::extractBvid(const VideoInfo& video)
{
    if (!video.bvid.trimmed().isEmpty()) {
        return video.bvid.trimmed();
    }

    const QString combined = video.pageUrl + QLatin1Char(' ') + video.url + QLatin1Char(' ') + video.id;
    const QRegularExpression bvidPattern(QStringLiteral("(BV[0-9A-Za-z]{10})"));
    const QRegularExpressionMatch match = bvidPattern.match(combined);
    return match.hasMatch() ? match.captured(1) : QString();
}

QString BilibiliPlaybackResolver::pageUrlForVideo(const VideoInfo& video, const QString& bvid)
{
    if (!video.pageUrl.trimmed().isEmpty()) {
        return video.pageUrl.trimmed();
    }

    const QString resolvedBvid = bvid.trimmed().isEmpty() ? video.bvid.trimmed() : bvid.trimmed();
    if (!resolvedBvid.isEmpty()) {
        return QStringLiteral("https://www.bilibili.com/video/%1").arg(resolvedBvid);
    }

    return video.url.trimmed();
}

void BilibiliPlaybackResolver::resolve(const VideoInfo& video)
{
    PendingPlaybackContext context;
    context.video = video;
    context.bvid = extractBvid(video);
    context.request.title = video.title;
    context.request.author = video.author;
    context.request.pageUrl = pageUrlForVideo(video, context.bvid);

    if (context.bvid.isEmpty()) {
        if (QUrl::fromUserInput(context.request.pageUrl).isValid()) {
            emitBrowserOnly(context,
                            QStringLiteral("当前结果无法解析 bvid，不能获取可交给 QMediaPlayer 的直链。"),
                            QStringLiteral("video.playback.missing_bvid"));
            return;
        }

        emitFailure(QStringLiteral("当前结果无法解析 bvid，也没有可打开的视频网页。"),
                    QStringLiteral("video.playback.missing_bvid"));
        return;
    }

    const QUrl directUrl = QUrl::fromUserInput(video.url.trimmed());
    if (directUrl.isValid() && !isPageOnlyUrl(directUrl, context.request.pageUrl)) {
        context.request.mediaUrl = directUrl;
        context.request.mediaDescription = QStringLiteral("在线视频直链");
        context.request.resolution = PlaybackResolution::DirectPlayable;
        context.request.valid = true;
        emit playbackResolved(context.request);
        return;
    }

    if (video.cid > 0) {
        context.cid = video.cid;
        context.stage = PlaybackStage::ResolveSingleStream;
    } else {
        context.stage = PlaybackStage::ResolveCid;
    }
    startPlaybackRequest(context);
}

void BilibiliPlaybackResolver::startPlaybackRequest(const PendingPlaybackContext& context)
{
    QUrl url;
    switch (context.stage) {
    case PlaybackStage::ResolveCid:
        url = bilibiliPagelistUrl(context.bvid);
        break;
    case PlaybackStage::ResolveSingleStream:
        url = bilibiliPlayUrl(context.bvid, context.cid, true);
        break;
    case PlaybackStage::ProbeDash:
        url = bilibiliPlayUrl(context.bvid, context.cid, false);
        break;
    }

    RequestOptions options;
    options.headers = bilibiliHeaders();
    options.timeout = kBilibiliRequestTimeoutMs;
    options.retry = 1;
    const QString requestId = m_networkClient->get(url, options);
    m_pendingPlaybackRequests.insert(requestId, context);
}

void BilibiliPlaybackResolver::onRequestFinished(const QString& requestId, const NetworkResult& result)
{
    handlePlaybackResponse(requestId, result);
}

void BilibiliPlaybackResolver::handlePlaybackResponse(const QString& requestId,
                                                      const NetworkResult& result)
{
    if (!m_pendingPlaybackRequests.contains(requestId)) {
        return;
    }

    PendingPlaybackContext context = m_pendingPlaybackRequests.take(requestId);
    switch (context.stage) {
    case PlaybackStage::ResolveCid:
        handleCidResponse(result, context);
        break;
    case PlaybackStage::ResolveSingleStream:
        handleSingleStreamResponse(result, context);
        break;
    case PlaybackStage::ProbeDash:
        handleDashProbeResponse(result, context);
        break;
    }
}

void BilibiliPlaybackResolver::handleCidResponse(const NetworkResult& result,
                                                 PendingPlaybackContext context)
{
    QString requestError;
    const QJsonObject root = jsonObjectFromResult(result, &requestError);
    if (root.isEmpty()) {
        emitBrowserOnly(context,
                        QStringLiteral("无法解析 cid。\n%1")
                            .arg(requestError.isEmpty()
                                     ? QStringLiteral("Bilibili 未返回有效 cid。")
                                     : requestError),
                        QStringLiteral("video.playback.cid_request"));
        return;
    }

    QString apiError;
    if (!isSuccessfulBilibiliResponse(root, &apiError)) {
        emitBrowserOnly(context,
                        QStringLiteral("无法解析 cid。\n%1").arg(apiError),
                        QStringLiteral("video.playback.cid_api"));
        return;
    }

    const QJsonArray pages = root.value(QStringLiteral("data")).toArray();
    if (pages.isEmpty()) {
        emitBrowserOnly(context,
                        QStringLiteral("无法解析视频分 P 信息：Bilibili 未返回 cid。"),
                        QStringLiteral("video.playback.cid_missing"));
        return;
    }

    const int wantedPage = pageNumberFromUrl(context.video.pageUrl);
    for (const QJsonValue& value : pages) {
        const QJsonObject page = value.toObject();
        if (page.value(QStringLiteral("page")).toInt() == wantedPage) {
            context.cid = page.value(QStringLiteral("cid")).toVariant().toLongLong();
            break;
        }
    }
    if (context.cid <= 0) {
        context.cid = pages.first().toObject().value(QStringLiteral("cid")).toVariant().toLongLong();
    }

    if (context.cid <= 0) {
        emitBrowserOnly(context,
                        QStringLiteral("Bilibili 未返回有效 cid。"),
                        QStringLiteral("video.playback.cid_invalid"));
        return;
    }

    context.stage = PlaybackStage::ResolveSingleStream;
    startPlaybackRequest(context);
}

void BilibiliPlaybackResolver::handleSingleStreamResponse(const NetworkResult& result,
                                                          PendingPlaybackContext context)
{
    QString requestError;
    const QJsonObject playRoot = jsonObjectFromResult(result, &requestError);
    if (playRoot.isEmpty()) {
        emitBrowserOnly(context,
                        QStringLiteral("解析真实流地址失败。\n%1")
                            .arg(requestError.isEmpty()
                                     ? QStringLiteral("Bilibili 接口无响应。")
                                     : requestError),
                        QStringLiteral("video.playback.playurl_request"));
        return;
    }

    QString apiError;
    if (!isSuccessfulBilibiliResponse(playRoot, &apiError)) {
        emitBrowserOnly(context,
                        QStringLiteral("Bilibili 播放地址接口拒绝请求。\n%1\n\n可能原因：接口风控、需要登录/Cookie、区域或版权限制。")
                            .arg(apiError),
                        QStringLiteral("video.playback.playurl_api"));
        return;
    }

    const QJsonObject playData = payloadObject(playRoot);
    QString streamError;
    const QUrl directUrl = resolveSingleStreamUrl(playData, &streamError);
    if (!directUrl.isValid()) {
        context.streamError = streamError;
        context.stage = PlaybackStage::ProbeDash;
        startPlaybackRequest(context);
        return;
    }

    if (isPageOnlyUrl(directUrl, context.request.pageUrl)) {
        emitBrowserOnly(context,
                        QStringLiteral("解析结果仍是网页 URL，不是直连媒体 URL。"),
                        QStringLiteral("video.playback.page_url"));
        return;
    }

    context.request.mediaUrl = directUrl;
    context.request.mediaDescription = QStringLiteral("Bilibili HTML5 MP4 单流 durl（音视频合一，低清优先）");
    context.request.resolution = PlaybackResolution::DirectPlayable;
    context.request.valid = true;
    emit playbackResolved(context.request);
}

void BilibiliPlaybackResolver::handleDashProbeResponse(const NetworkResult& result,
                                                       PendingPlaybackContext context)
{
    QString dashHint;
    QString requestError;
    const QJsonObject dashRoot = jsonObjectFromResult(result, &requestError);
    if (!dashRoot.isEmpty()) {
        QString dashApiError;
        if (isSuccessfulBilibiliResponse(dashRoot, &dashApiError)
            && responseContainsDash(payloadObject(dashRoot))) {
            dashHint = QStringLiteral("\n已解析到 DASH 视频/音频分离流，但当前 Qt %1 的播放链路只能向 QMediaPlayer 提交一个媒体 URL，不能在这里把独立视频轨和音频轨合流；为避免只有画面或只有声音，本次不直接播放 DASH。")
                           .arg(QStringLiteral(QT_VERSION_STR));
        } else if (!dashApiError.isEmpty()) {
            dashHint = QStringLiteral("\nDASH 探测也失败：%1").arg(dashApiError);
        }
    } else if (!requestError.isEmpty()) {
        dashHint = QStringLiteral("\nDASH 探测请求失败：%1").arg(requestError);
    }

    const QString message = QStringLiteral("%1%2")
        .arg(context.streamError.isEmpty()
                 ? QStringLiteral("未拿到可用单流媒体地址。")
                 : context.streamError)
        .arg(dashHint);
    emitBrowserOnly(context, message, QStringLiteral("video.playback.no_single_stream"));
}

void BilibiliPlaybackResolver::emitBrowserOnly(PendingPlaybackContext context,
                                               const QString& message,
                                               const QString& code)
{
    context.request.resolution = PlaybackResolution::BrowserOnly;
    context.request.valid = false;
    context.request.errorMessage = QStringLiteral("当前结果不可直接播放：%1").arg(message)
        + browserOnlyText();

    if (QUrl::fromUserInput(context.request.pageUrl).isValid()) {
        emit playbackResolved(context.request);
        return;
    }

    emit playbackResolveFailed({code,
                                context.request.errorMessage
                                    + QStringLiteral("\n\n该结果也没有有效网页地址。"),
                                false});
}

void BilibiliPlaybackResolver::emitFailure(const QString& message, const QString& code)
{
    emit playbackResolveFailed({code, message, false});
}
