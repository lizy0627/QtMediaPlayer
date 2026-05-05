#include "aichatwidget.h"

#include "aichatcontroller.h"
#include "aichatview.h"
#include "network/aichatservice.h"

#include <QShowEvent>
#include <QVBoxLayout>

AiChatWidget::AiChatWidget(QWidget* videoWidget, QWidget* parent)
    : QWidget(parent)
    , m_view(new AiChatView(this))
    , m_chatService(new AiChatService(this))
    , m_controller(new AiChatController(m_view, m_chatService, this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_view);

    setMinimumWidth(320);
    setVideoWidget(videoWidget);
}

void AiChatWidget::setVideoWidget(QWidget* widget)
{
    if (m_controller) {
        m_controller->setVideoWidget(widget);
    }
}

void AiChatWidget::setCaptureService(CaptureService* captureService)
{
    if (m_controller) {
        m_controller->setCaptureService(captureService);
    }
}

void AiChatWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    if (m_controller) {
        m_controller->refreshConfigurationStatus();
    }
}
