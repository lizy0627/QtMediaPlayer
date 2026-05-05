#include "aichatpanel.h"

#include "aichatwidget.h"

#include <QVideoWidget>

AiChatPanel::AiChatPanel(QVideoWidget* videoWidget, QWidget* parentWidget, QObject* parent)
    : QObject(parent)
{
    m_widget = new AiChatWidget(videoWidget, parentWidget);
    m_widget->setMinimumWidth(320);
    m_widget->hide();
}

AiChatWidget* AiChatPanel::widget() const
{
    return m_widget;
}

void AiChatPanel::setCaptureService(CaptureService* captureService)
{
    if (m_widget) {
        m_widget->setCaptureService(captureService);
    }
}

void AiChatPanel::setVisible(bool visible)
{
    m_widget->setVisible(visible);
}

bool AiChatPanel::isVisible() const
{
    return m_widget->isVisible();
}
