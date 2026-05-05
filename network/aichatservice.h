#ifndef AICHATSERVICE_H
#define AICHATSERVICE_H

#include <QObject>
#include <QPixmap>
#include <QString>

#include "networkclient.h"

class AiChatService : public QObject
{
    Q_OBJECT

public:
    explicit AiChatService(QObject* parent = nullptr);

    void requestChat(const QString& prompt, const QPixmap& image = QPixmap());
    void cancelActiveRequest();
    QString modelName() const;
    bool hasApiKey() const;
    QString configurationMessage() const;

signals:
    void chatStarted();
    void chatFinished(QString reply);
    void chatFailed(const ServiceError& error);
    void chatFailed(QString message);

private:
    static QString readEnvValue(const char* key, const char* defaultValue = "");
    static QString readEnvValue(const char* primaryKey,
                                const char* secondaryKey,
                                const char* defaultValue);
    static QString extractReplyText(const QByteArray& data, QString* errorMessage);

private slots:
    void onRequestFinished(const QString& requestId, const NetworkResult& result);

private:
    NetworkClient* m_networkClient = nullptr;
    QString m_activeRequestId;
    QString m_apiKey;
    QString m_model;
    QString m_apiUrl;
};

#endif // AICHATSERVICE_H
