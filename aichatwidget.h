#ifndef AICHATWIDGET_H
#define AICHATWIDGET_H

#include <QWidget>

class AiChatController;
class AiChatService;
class AiChatView;
class CaptureService;
class QShowEvent;

class AiChatWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AiChatWidget(QWidget* videoWidget, QWidget* parent = nullptr);

    void setVideoWidget(QWidget* widget);
    void setCaptureService(CaptureService* captureService);

protected:
    void showEvent(QShowEvent* event) override;

private:
    AiChatView* m_view = nullptr;
    AiChatService* m_chatService = nullptr;
    AiChatController* m_controller = nullptr;
};

#endif // AICHATWIDGET_H
