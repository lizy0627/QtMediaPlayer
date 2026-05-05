#ifndef AICHATPANEL_H
#define AICHATPANEL_H

#include <QObject>

class AiChatWidget;
class CaptureService;
class QVideoWidget;
class QWidget;

class AiChatPanel : public QObject
{
    Q_OBJECT

public:
    explicit AiChatPanel(QVideoWidget* videoWidget, QWidget* parentWidget, QObject* parent = nullptr);

    AiChatWidget* widget() const;
    void setCaptureService(CaptureService* captureService);
    void setVisible(bool visible);
    bool isVisible() const;

private:
    AiChatWidget* m_widget = nullptr;
};

#endif // AICHATPANEL_H
