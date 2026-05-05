#include "aichatservice.h"

#include <QBuffer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>

namespace {
void emitChatFailure(AiChatService* service, const ServiceError& error)
{
    emit service->chatFailed(error);
    emit service->chatFailed(error.message);
}
}

AiChatService::AiChatService(QObject* parent)
    : QObject(parent)
    , m_networkClient(new NetworkClient(this))
    , m_apiKey(readEnvValue("QT_AI_API_KEY", "DASHSCOPE_API_KEY", ""))
    , m_model(readEnvValue("QT_AI_MODEL", "qwen-vl-max"))
    , m_apiUrl(readEnvValue("QT_AI_API_URL", "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions"))
{
    connect(m_networkClient, &NetworkClient::requestFinished,
            this, &AiChatService::onRequestFinished);
}

void AiChatService::requestChat(const QString& prompt, const QPixmap& image)
{
    if (!m_activeRequestId.isEmpty()) {
        const QString message = QStringLiteral("当前已有 AI 请求正在处理中，请稍后再试。");
        emitChatFailure(this, {QStringLiteral("ai.busy"), message, true});
        return;
    }

    if (m_apiKey.trimmed().isEmpty()) {
        const QString message = QStringLiteral("未读取到 API Key，请先设置系统环境变量 QT_AI_API_KEY 或 DASHSCOPE_API_KEY。");
        emitChatFailure(this, {QStringLiteral("ai.missing_api_key"), message, false});
        return;
    }

    QJsonArray contentArray;
    if (!image.isNull()) {
        QByteArray imageBytes;
        QBuffer buffer(&imageBytes);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "JPEG", 85);
        buffer.close();

        QJsonObject imagePart;
        imagePart["type"] = "image_url";

        QJsonObject imageUrl;
        imageUrl["url"] = QStringLiteral("data:image/jpeg;base64,") + QString::fromLatin1(imageBytes.toBase64());
        imagePart["image_url"] = imageUrl;
        contentArray.append(imagePart);
    }

    QJsonObject textPart;
    textPart["type"] = "text";
    textPart["text"] = prompt.trimmed().isEmpty()
        ? QStringLiteral("请分析这个画面的内容")
        : prompt.trimmed();
    contentArray.append(textPart);

    QJsonObject systemMessage;
    systemMessage["role"] = "system";
    systemMessage["content"] = QStringLiteral("你是专业影视分析助手，能够识别人像、场景和剧情信息，请始终使用中文回答。");

    QJsonObject userMessage;
    userMessage["role"] = "user";
    userMessage["content"] = contentArray;

    QJsonArray messages;
    messages.append(systemMessage);
    messages.append(userMessage);

    QJsonObject body;
    body["model"] = m_model;
    body["messages"] = messages;
    body["max_tokens"] = 1024;

    QVariantMap headers;
    headers.insert("Authorization", QStringLiteral("Bearer %1").arg(m_apiKey));
    headers.insert("Content-Type", "application/json");

    emit chatStarted();
    RequestOptions options;
    options.headers = headers;
    options.timeout = 45000;
    options.retry = 1;
    m_activeRequestId = m_networkClient->postJson(QUrl(m_apiUrl), body, options);
}

void AiChatService::cancelActiveRequest()
{
    if (m_activeRequestId.isEmpty()) {
        return;
    }

    m_networkClient->cancel(m_activeRequestId);
}

QString AiChatService::modelName() const
{
    return m_model;
}

bool AiChatService::hasApiKey() const
{
    return !m_apiKey.trimmed().isEmpty();
}

QString AiChatService::configurationMessage() const
{
    if (hasApiKey()) {
        return QStringLiteral("API Key \u5df2\u914d\u7f6e");
    }

    return QStringLiteral("\u672a\u914d\u7f6e API Key\uff1a\u8bf7\u8bbe\u7f6e QT_AI_API_KEY \u6216 DASHSCOPE_API_KEY");
}

QString AiChatService::readEnvValue(const char* key, const char* defaultValue)
{
    const QString value = QProcessEnvironment::systemEnvironment().value(QString::fromLatin1(key)).trimmed();
    return value.isEmpty() ? QString::fromLatin1(defaultValue) : value;
}

QString AiChatService::readEnvValue(const char* primaryKey,
                                    const char* secondaryKey,
                                    const char* defaultValue)
{
    QString value = readEnvValue(primaryKey);
    if (!value.isEmpty()) {
        return value;
    }

    value = readEnvValue(secondaryKey);
    return value.isEmpty() ? QString::fromLatin1(defaultValue) : value;
}

QString AiChatService::extractReplyText(const QByteArray& data, QString* errorMessage)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("AI 响应解析失败：%1").arg(parseError.errorString());
        }
        return {};
    }

    const QJsonObject root = document.object();
    const QJsonArray choices = root.value("choices").toArray();
    if (!choices.isEmpty()) {
        const QJsonObject messageObject = choices.first().toObject().value("message").toObject();
        const QJsonValue content = messageObject.value("content");

        if (content.isString()) {
            return content.toString().trimmed();
        }

        if (content.isArray()) {
            QString answer;
            const QJsonArray parts = content.toArray();
            for (const QJsonValue& partValue : parts) {
                const QJsonObject part = partValue.toObject();
                if (part.value("type").toString() == "text") {
                    answer += part.value("text").toString();
                }
            }
            return answer.trimmed();
        }
    }

    if (root.contains("error")) {
        const QString message = root.value("error").toObject().value("message").toString().trimmed();
        if (errorMessage) {
            *errorMessage = message.isEmpty()
                ? QStringLiteral("AI 服务返回了错误响应。")
                : message;
        }
        return {};
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("未获取到有效的 AI 回复。");
    }
    return {};
}

void AiChatService::onRequestFinished(const QString& requestId, const NetworkResult& result)
{
    if (requestId != m_activeRequestId) {
        return;
    }

    m_activeRequestId.clear();

    if (result.canceled) {
        const QString message = QStringLiteral("请求已取消");
        emitChatFailure(this, result.toServiceError(QStringLiteral("ai.request.canceled"), message));
        return;
    }

    QString parsedError;
    const QString reply = extractReplyText(result.body, &parsedError);

    if (!result.ok()) {
        QString message = parsedError;
        if (message.isEmpty()) {
            message = result.errorMessage;
        }
        if (message.isEmpty()) {
            message = QStringLiteral("AI 请求失败。");
        }
        emitChatFailure(this, result.toServiceError(QStringLiteral("ai.request.failed"), message));
        return;
    }

    if (reply.isEmpty()) {
        const QString message = parsedError.isEmpty()
            ? QStringLiteral("未获取到有效的 AI 回复。")
            : parsedError;
        emitChatFailure(this, {QStringLiteral("ai.response.empty"), message, false});
        return;
    }

    emit chatFinished(reply);
}
