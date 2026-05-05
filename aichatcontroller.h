#ifndef AICHATCONTROLLER_H
#define AICHATCONTROLLER_H

#include <QObject>
#include <QPixmap>

class AiChatService;
class AiChatView;
class CaptureService;
class FrameCaptureService;
class QWidget;

class AiChatController : public QObject
{
    Q_OBJECT

public:
    explicit AiChatController(AiChatView* view,
                              AiChatService* chatService,
                              QObject* parent = nullptr);

    void setVideoWidget(QWidget* widget);
    void setCaptureService(CaptureService* captureService);

public slots:
    void refreshConfigurationStatus();
    void captureFrame();
    void clearImage();
    void submitOrCancel();
    void cancelActiveRequest();

private slots:
    void onChatFinished(const QString& reply);
    void onChatFailed(const QString& message);

private:
    QPixmap captureCurrentFrame() const;
    void resetRequestState();

    AiChatView* m_view = nullptr;
    AiChatService* m_chatService = nullptr;
    CaptureService* m_captureService = nullptr;
    FrameCaptureService* m_frameCaptureService = nullptr;
    QPixmap m_pendingPixmap;
    bool m_requesting = false;
};

#endif // AICHATCONTROLLER_H
