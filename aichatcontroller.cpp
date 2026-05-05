#include "aichatcontroller.h"

#include "aichatview.h"
#include "captureservice.h"
#include "framecaptureservice.h"
#include "network/aichatservice.h"

AiChatController::AiChatController(AiChatView* view,
                                   AiChatService* chatService,
                                   QObject* parent)
    : QObject(parent)
    , m_view(view)
    , m_chatService(chatService)
    , m_frameCaptureService(new FrameCaptureService(nullptr, this))
{
    if (m_view) {
        connect(m_view, &AiChatView::captureRequested, this, &AiChatController::captureFrame);
        connect(m_view, &AiChatView::clearImageRequested, this, &AiChatController::clearImage);
        connect(m_view, &AiChatView::submitRequested, this, &AiChatController::submitOrCancel);
    }

    if (m_chatService) {
        connect(m_chatService, &AiChatService::chatFinished, this, &AiChatController::onChatFinished);
        connect(m_chatService,
                qOverload<QString>(&AiChatService::chatFailed),
                this,
                &AiChatController::onChatFailed);
    }

    refreshConfigurationStatus();
}

void AiChatController::setVideoWidget(QWidget* widget)
{
    if (m_frameCaptureService) {
        m_frameCaptureService->setSourceWidget(widget);
    }
}

void AiChatController::setCaptureService(CaptureService* captureService)
{
    m_captureService = captureService;
}

void AiChatController::refreshConfigurationStatus()
{
    if (!m_view || !m_chatService) {
        return;
    }

    m_view->setModelName(m_chatService->modelName());
    m_view->setApiKeyStatus(m_chatService->hasApiKey(), m_chatService->configurationMessage());
}

void AiChatController::captureFrame()
{
    const QPixmap pixmap = captureCurrentFrame();
    if (pixmap.isNull()) {
        if (m_view) {
            m_view->setStatusText(QStringLiteral("\u622a\u56fe\u5931\u8d25"));
        }
        return;
    }

    m_pendingPixmap = pixmap;
    if (m_view) {
        m_view->setThumbnail(pixmap);
        m_view->setStatusText(QStringLiteral("\u5df2\u622a\u56fe\uff0c\u53ef\u8f93\u5165\u95ee\u9898\u53d1\u9001"));
    }
}

void AiChatController::clearImage()
{
    m_pendingPixmap = QPixmap();
    if (!m_view) {
        return;
    }

    m_view->clearThumbnail();
    if (!m_requesting) {
        m_view->clearStatus();
    }
}

void AiChatController::submitOrCancel()
{
    if (m_requesting) {
        cancelActiveRequest();
        return;
    }

    if (!m_view || !m_chatService) {
        return;
    }

    refreshConfigurationStatus();

    const QString text = m_view->promptText();
    if (text.isEmpty() && m_pendingPixmap.isNull()) {
        m_view->setStatusText(QStringLiteral("\u8bf7\u8f93\u5165\u95ee\u9898\u6216\u622a\u53d6\u753b\u9762"));
        return;
    }

    m_view->appendUserMessage(text, m_pendingPixmap);
    m_view->clearPrompt();

    const QPixmap sentPixmap = m_pendingPixmap;
    clearImage();

    m_requesting = true;
    m_view->setRequesting(true);
    m_view->setStatusText(QStringLiteral("AI \u6b63\u5728\u601d\u8003\u4e2d..."));
    m_chatService->requestChat(text, sentPixmap);
}

void AiChatController::cancelActiveRequest()
{
    if (!m_requesting || !m_chatService || !m_view) {
        return;
    }

    m_view->setStatusText(QStringLiteral("\u6b63\u5728\u53d6\u6d88\u8bf7\u6c42..."));
    m_chatService->cancelActiveRequest();
}

void AiChatController::onChatFinished(const QString& reply)
{
    resetRequestState();
    if (m_view) {
        m_view->clearStatus();
        m_view->appendAssistantMessage(reply);
    }
}

void AiChatController::onChatFailed(const QString& message)
{
    resetRequestState();
    if (!m_view) {
        return;
    }

    if (message == QStringLiteral("\u8bf7\u6c42\u5df2\u53d6\u6d88")) {
        m_view->setStatusText(QStringLiteral("\u8bf7\u6c42\u5df2\u53d6\u6d88"));
    } else {
        m_view->setStatusText(QStringLiteral("\u8bf7\u6c42\u5931\u8d25"));
    }
    m_view->appendAssistantMessage(message);
}

QPixmap AiChatController::captureCurrentFrame() const
{
    if (m_captureService) {
        return m_captureService->captureCurrentFrame();
    }

    return m_frameCaptureService ? m_frameCaptureService->captureCurrentFrame() : QPixmap();
}

void AiChatController::resetRequestState()
{
    m_requesting = false;
    if (m_view) {
        m_view->setRequesting(false);
    }
    refreshConfigurationStatus();
}
