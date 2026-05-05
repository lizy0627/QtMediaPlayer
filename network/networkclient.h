#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>
#include <QVariantMap>

class QNetworkReply;
class QTimer;

enum class NetworkLogPolicy {
    Default,
    Silent,
    Basic,
    Detailed
};

struct ServiceError
{
    QString code;
    QString message;
    bool retryable = false;

    bool isEmpty() const
    {
        return code.isEmpty() && message.isEmpty();
    }
};

Q_DECLARE_METATYPE(ServiceError)

struct RequestOptions
{
    int timeout = 30000;
    int timeoutMs = -1;
    int retry = 0;
    QVariantMap headers;
    NetworkLogPolicy logPolicy = NetworkLogPolicy::Default;
};

struct NetworkResult
{
    int httpStatus = 0;
    int networkErrorCode = 0;
    QByteArray body;
    QString errorMessage;
    ServiceError serviceError;
    bool timedOut = false;
    bool canceled = false;

    bool ok() const
    {
        return !canceled
            && networkErrorCode == 0
            && httpStatus >= 200
            && httpStatus < 300;
    }

    ServiceError toServiceError(const QString& fallbackCode = QString(),
                                const QString& fallbackMessage = QString()) const;
};

Q_DECLARE_METATYPE(NetworkResult)

class NetworkClient : public QObject
{
    Q_OBJECT

public:
    explicit NetworkClient(QObject* parent = nullptr);
    ~NetworkClient() override;

    QString get(const QUrl& url,
                const QVariantMap& headers = {},
                int timeoutMs = 30000,
                int maxRetries = 0);
    QString get(const QUrl& url, const RequestOptions& options);
    QString postJson(const QUrl& url,
                     const QJsonObject& body,
                     const QVariantMap& headers = {},
                     int timeoutMs = 30000,
                     int maxRetries = 0);
    QString postJson(const QUrl& url,
                     const QJsonObject& body,
                     const RequestOptions& options);
    void cancel(const QString& requestId);
    void cancelAll();

signals:
    void requestFinished(const QString& requestId, const NetworkResult& result);

private:
    enum class HttpMethod {
        Get,
        PostJson
    };

    struct PendingRequest
    {
        QString id;
        HttpMethod method = HttpMethod::Get;
        QUrl url;
        QVariantMap headers;
        QByteArray body;
        int timeoutMs = 30000;
        int maxRetries = 0;
        int attempt = 0;
        bool canceled = false;
        NetworkLogPolicy logPolicy = NetworkLogPolicy::Default;
        QPointer<QNetworkReply> reply;
        QPointer<QTimer> timer;
    };

    QString enqueue(HttpMethod method,
                    const QUrl& url,
                    const QByteArray& body,
                    const RequestOptions& options);
    void dispatchRequest(PendingRequest* request);
    void finishRequest(PendingRequest* request, const NetworkResult& result);
    static void applyHeaders(QNetworkRequest* request, const QVariantMap& headers);
    static QString describeNetworkError(int errorCode, const QString& fallback);

    QNetworkAccessManager m_networkManager;
    QHash<QString, PendingRequest*> m_pendingRequests;
};

#endif // NETWORKCLIENT_H
