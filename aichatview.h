#ifndef AICHATVIEW_H
#define AICHATVIEW_H

#include <QPixmap>
#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;
class QScrollArea;
class QTextEdit;
class QTimer;
class QVBoxLayout;

class ChatBubble : public QWidget
{
    Q_OBJECT

public:
    enum Role {
        User,
        Assistant
    };

    explicit ChatBubble(Role role,
                        const QString& text,
                        const QPixmap& thumb = QPixmap(),
                        QWidget* parent = nullptr);
};

class ThinkingBubble : public QWidget
{
    Q_OBJECT

public:
    explicit ThinkingBubble(QWidget* parent = nullptr);
    ~ThinkingBubble() override;

private:
    QLabel* m_label = nullptr;
    QTimer* m_timer = nullptr;
    int m_step = 0;
};

class AiChatView : public QWidget
{
    Q_OBJECT

public:
    explicit AiChatView(QWidget* parent = nullptr);

    QString promptText() const;
    void clearPrompt();
    void appendUserMessage(const QString& text, const QPixmap& thumb = QPixmap());
    void appendAssistantMessage(const QString& text);
    void setModelName(const QString& modelName);
    void setApiKeyStatus(bool configured, const QString& message);
    void setStatusText(const QString& text);
    void clearStatus();
    void setThumbnail(const QPixmap& pixmap);
    void clearThumbnail();
    void setRequesting(bool requesting);

signals:
    void captureRequested();
    void clearImageRequested();
    void submitRequested();

private:
    void buildUI();
    void appendBubble(ChatBubble::Role role,
                      const QString& text,
                      const QPixmap& thumb = QPixmap());
    void showThinking();
    void removeThinking();
    void scrollToBottom();

    QVBoxLayout* m_mainLayout = nullptr;
    QWidget* m_scrollContent = nullptr;
    QVBoxLayout* m_chatLayout = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QLabel* m_thumbLabel = nullptr;
    QLabel* m_apiKeyStatusLabel = nullptr;
    QLabel* m_modelLabel = nullptr;
    QPushButton* m_btnCapture = nullptr;
    QPushButton* m_btnClearImg = nullptr;
    QTextEdit* m_inputEdit = nullptr;
    QPushButton* m_btnSend = nullptr;
    QLabel* m_statusLabel = nullptr;
    ThinkingBubble* m_thinking = nullptr;
};

#endif // AICHATVIEW_H
