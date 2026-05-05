#include "networkclient.h"

#include <QDebug>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QStringList>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>

Q_LOGGING_CATEGORY(lcNetworkClient, "qtmediaplayer.network.client", QtWarningMsg)

namespace {
bool isSensitiveName(const QString& name)
{
    const QString lower = name.toLower();
    return lower.contains(QStringLiteral("authorization"))
        || lower.contains(QStringLiteral("token"))
        || lower.contains(QStringLiteral("api_key"))
        || lower.contains(QStringLiteral("apikey"))
        || lower.contains(QStringLiteral("key"))
        || lower.contains(QStringLiteral("secret"))
        || lower.contains(QStringLiteral("password"))
        || lower.contains(QStringLiteral("cookie"))
        || lower.contains(QStringLiteral("session"));
}

QString redactedUrl(const QUrl& url)
{
    QUrl sanitized = url;
    QUrlQuery query(sanitized);
    QList<QPair<QString, QString>> items = query.queryItems(QUrl::FullyDecoded);
    query.clear();

    for (const auto& item : items) {
        query.addQueryItem(item.first,
                           isSensitiveName(item.first) ? QStringLiteral("<redacted>") : item.second);
    }

    sanitized.setQuery(query);
    return sanitized.toString(QUrl::RemoveUserInfo);
}

QString basicUrlLabel(const QUrl& url)
{
    return QStringLiteral("%1://%2%3")
        .arg(url.scheme(), url.host(), url.path());
}

NetworkLogPolicy configuredDefaultLogPolicy()
{
    const QString value = QString::fromLocal8Bit(qgetenv("QT_MEDIA_NETWORK_LOG"))
                              .trimmed()
                              .toLower();
    if (value == QStringLiteral("detailed")) {
        return NetworkLogPolicy::Detailed;
    }
    if (value == QStringLiteral("basic") || value == QStringLiteral("1")
        || value == QStringLiteral("true")) {
        return NetworkLogPolicy::Basic;
    }
    return NetworkLogPolicy::Silent;
}

NetworkLogPolicy effectiveLogPolicy(NetworkLogPolicy policy)
{
    return policy == NetworkLogPolicy::Default ? configuredDefaultLogPolicy() : policy;
}

bool shouldLog(NetworkLogPolicy policy)
{
    return policy == NetworkLogPolicy::Basic || policy == NetworkLogPolicy::Detailed;
}

bool shouldLogDetailed(NetworkLogPolicy policy)
{
    return policy == NetworkLogPolicy::Detailed;
}

QString redactedHeaders(const QVariantMap& headers)
{
    QStringList entries;
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
        entries << QStringLiteral("%1=%2")
                       .arg(it.key(),
                            isSensitiveName(it.key())
                                ? QStringLiteral("<redacted>")
                                : it.value().toString());
    }
    return entries.join(QStringLiteral(", "));
}

QString errorCodeForResult(const NetworkResult& result, const QString& fallbackCode)
{
    if (result.canceled) {
        return QStringLiteral("request.canceled");
    }
    if (result.timedOut) {
        return QStringLiteral("network.timeout");
    }
    if (result.networkErrorCode != 0) {
        return QStringLiteral("network.%1").arg(result.networkErrorCode);
    }
    if (result.httpStatus >= 400) {
        return QStringLiteral("http.%1").arg(result.httpStatus);
    }
    return fallbackCode.isEmpty() ? QStringLiteral("service.error") : fallbackCode;
}

bool isRetryableResult(const NetworkResult& result)
{
    return !result.canceled
        && (result.timedOut
            || result.networkErrorCode != 0
            || result.httpStatus == 408
            || result.httpStatus == 429
            || result.httpStatus >= 500);
}
}

ServiceError NetworkResult::toServiceError(const QString& fallbackCode,
                                           const QString& fallbackMessage) const
{
    if (!serviceError.isEmpty()) {
        return serviceError;
    }

    ServiceError error;
    error.code = errorCodeForResult(*this, fallbackCode);
    error.message = errorMessage.trimmed().isEmpty() ? fallbackMessage : errorMessage.trimmed();
    if (error.message.isEmpty()) {
        error.message = QStringLiteral("Service request failed.");
    }
    error.retryable = isRetryableResult(*this);
    return error;
}

NetworkClient::NetworkClient(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<ServiceError>("ServiceError");
    qRegisterMetaType<NetworkResult>("NetworkResult");
}

NetworkClient::~NetworkClient()
{
    cancelAll();
}

QString NetworkClient::get(const QUrl& url,
                           const QVariantMap& headers,
                           int timeoutMs,
                           int maxRetries)
{
    RequestOptions options;
    options.headers = headers;
    options.timeout = timeoutMs;
    options.retry = maxRetries;
    return get(url, options);
}

QString NetworkClient::get(const QUrl& url, const RequestOptions& options)
{
    return enqueue(HttpMethod::Get, url, {}, options);
}

QString NetworkClient::postJson(const QUrl& url,
                                const QJsonObject& body,
                                const QVariantMap& headers,
                                int timeoutMs,
                                int maxRetries)
{
    RequestOptions options;
    options.headers = headers;
    options.timeout = timeoutMs;
    options.retry = maxRetries;
    return postJson(url, body, options);
}

QString NetworkClient::postJson(const QUrl& url,
                                const QJsonObject& body,
                                const RequestOptions& options)
{
    RequestOptions mergedOptions = options;
    QVariantMap mergedHeaders = mergedOptions.headers;
    if (!mergedHeaders.contains("Content-Type")) {
        mergedHeaders.insert("Content-Type", "application/json");
    }
    mergedOptions.headers = mergedHeaders;

    return enqueue(HttpMethod::PostJson,
                   url,
                   QJsonDocument(body).toJson(QJsonDocument::Compact),
                   mergedOptions);
}

void NetworkClient::cancel(const QString& requestId)
{
    PendingRequest* request = m_pendingRequests.value(requestId, nullptr);
    if (!request) {
        return;
    }

    request->canceled = true;
    if (request->reply) {
        request->reply->setProperty("canceled", true);
        request->reply->abort();
        return;
    }

    NetworkResult result;
    result.canceled = true;
    result.errorMessage = QStringLiteral("Request canceled.");
    result.serviceError = result.toServiceError();
    finishRequest(request, result);
}

void NetworkClient::cancelAll()
{
    const QStringList requestIds = m_pendingRequests.keys();
    for (const QString& requestId : requestIds) {
        cancel(requestId);
    }
}

QString NetworkClient::enqueue(HttpMethod method,
                               const QUrl& url,
                               const QByteArray& body,
                               const RequestOptions& options)
{
    PendingRequest* request = new PendingRequest;
    request->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    request->method = method;
    request->url = url;
    request->headers = options.headers;
    request->body = body;
    const int requestedTimeout = options.timeoutMs >= 0 ? options.timeoutMs : options.timeout;
    request->timeoutMs = qMax(1000, requestedTimeout);
    request->maxRetries = qMax(0, options.retry);
    request->logPolicy = effectiveLogPolicy(options.logPolicy);

    m_pendingRequests.insert(request->id, request);
    dispatchRequest(request);
    return request->id;
}

void NetworkClient::dispatchRequest(PendingRequest* request)
{
    if (!request || request->canceled) {
        return;
    }

    ++request->attempt;

    QNetworkRequest networkRequest(request->url);
    networkRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                QNetworkRequest::NoLessSafeRedirectPolicy);
    applyHeaders(&networkRequest, request->headers);

    if (shouldLog(request->logPolicy)) {
        QString logLine = QStringLiteral("%1 %2 (attempt %3/%4)")
                              .arg(request->method == HttpMethod::Get ? "GET" : "POST")
                              .arg(shouldLogDetailed(request->logPolicy)
                                       ? redactedUrl(request->url)
                                       : basicUrlLabel(request->url))
                              .arg(request->attempt)
                              .arg(request->maxRetries + 1);
        if (shouldLogDetailed(request->logPolicy) && !request->headers.isEmpty()) {
            logLine += QStringLiteral(" headers={%1}").arg(redactedHeaders(request->headers));
        }
        qCDebug(lcNetworkClient).noquote() << logLine;
    }

    QNetworkReply* reply = nullptr;
    switch (request->method) {
    case HttpMethod::Get:
        reply = m_networkManager.get(networkRequest);
        break;
    case HttpMethod::PostJson:
        reply = m_networkManager.post(networkRequest, request->body);
        break;
    }

    request->reply = reply;

    if (request->timer) {
        request->timer->stop();
        request->timer->deleteLater();
        request->timer = nullptr;
    }

    request->timer = new QTimer(this);
    request->timer->setSingleShot(true);
    connect(request->timer, &QTimer::timeout, this, [request]() {
        if (!request->reply) {
            return;
        }

        request->reply->setProperty("timedOut", true);
        request->reply->abort();
    });
    request->timer->start(request->timeoutMs);

    connect(reply, &QNetworkReply::finished, this, [this, request]() {
        if (!request || !m_pendingRequests.contains(request->id) || !request->reply) {
            return;
        }

        if (request->timer) {
            request->timer->stop();
            request->timer->deleteLater();
            request->timer = nullptr;
        }

        QNetworkReply* reply = request->reply;
        request->reply = nullptr;

        NetworkResult result;
        result.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        result.networkErrorCode = static_cast<int>(reply->error());
        result.body = reply->readAll();
        result.timedOut = reply->property("timedOut").toBool();
        result.canceled = request->canceled || reply->property("canceled").toBool();

        if (result.canceled) {
            result.errorMessage = QStringLiteral("Request canceled.");
        } else if (result.timedOut) {
            result.errorMessage = QStringLiteral("Request timed out.");
        } else if (reply->error() != QNetworkReply::NoError) {
            result.errorMessage = describeNetworkError(result.networkErrorCode, reply->errorString());
        } else if (result.httpStatus >= 400) {
            result.errorMessage = QStringLiteral("HTTP %1").arg(result.httpStatus);
        }
        if (!result.ok()) {
            result.serviceError = result.toServiceError();
        }

        if (shouldLog(request->logPolicy)) {
            qCDebug(lcNetworkClient).noquote()
                << QStringLiteral("finished %1 status=%2 qtError=%3")
                       .arg(request->id)
                       .arg(result.httpStatus)
                       .arg(result.networkErrorCode);
        }

        reply->deleteLater();

        const bool shouldRetry = !result.canceled
            && request->attempt <= request->maxRetries
            && (result.timedOut
                || result.networkErrorCode != 0
                || result.httpStatus >= 500);

        if (shouldRetry) {
            if (shouldLog(request->logPolicy)) {
                qCInfo(lcNetworkClient).noquote()
                    << QStringLiteral("retrying %1 after failure: %2")
                           .arg(request->id, result.errorMessage);
            }

            QTimer::singleShot(400 * request->attempt, this, [this, request]() {
                if (request && m_pendingRequests.contains(request->id) && !request->canceled) {
                    dispatchRequest(request);
                }
            });
            return;
        }

        finishRequest(request, result);
    });
}

void NetworkClient::finishRequest(PendingRequest* request, const NetworkResult& result)
{
    if (!request) {
        return;
    }

    const QString requestId = request->id;
    m_pendingRequests.remove(requestId);

    if (request->timer) {
        request->timer->stop();
        request->timer->deleteLater();
        request->timer = nullptr;
    }

    delete request;
    emit requestFinished(requestId, result);
}

void NetworkClient::applyHeaders(QNetworkRequest* request, const QVariantMap& headers)
{
    if (!request) {
        return;
    }

    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
        request->setRawHeader(it.key().toUtf8(), it.value().toString().toUtf8());
    }
}

QString NetworkClient::describeNetworkError(int errorCode, const QString& fallback)
{
    switch (static_cast<QNetworkReply::NetworkError>(errorCode)) {
    case QNetworkReply::NoError:
        return {};
    case QNetworkReply::ConnectionRefusedError:
        return QStringLiteral("Connection refused.");
    case QNetworkReply::RemoteHostClosedError:
        return QStringLiteral("Remote host closed the connection.");
    case QNetworkReply::HostNotFoundError:
        return QStringLiteral("Host not found.");
    case QNetworkReply::TimeoutError:
        return QStringLiteral("Network request timed out.");
    case QNetworkReply::OperationCanceledError:
        return QStringLiteral("Network request canceled.");
    case QNetworkReply::SslHandshakeFailedError:
        return QStringLiteral("SSL handshake failed.");
    case QNetworkReply::TemporaryNetworkFailureError:
        return QStringLiteral("Temporary network failure.");
    case QNetworkReply::ProxyAuthenticationRequiredError:
        return QStringLiteral("Proxy authentication required.");
    case QNetworkReply::ContentNotFoundError:
        return QStringLiteral("Requested content was not found.");
    default:
        return fallback.trimmed().isEmpty() ? QStringLiteral("Unknown network error.") : fallback.trimmed();
    }
}
