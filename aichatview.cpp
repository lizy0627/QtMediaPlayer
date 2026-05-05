#include "aichatview.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

ChatBubble::ChatBubble(Role role,
                       const QString& text,
                       const QPixmap& thumb,
                       QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(8, 3, 8, 3);
    outer->setSpacing(0);

    auto* bubble = new QWidget(this);
    bubble->setMaximumWidth(300);

    auto* inner = new QVBoxLayout(bubble);
    inner->setContentsMargins(11, 8, 11, 8);
    inner->setSpacing(5);

    if (!thumb.isNull()) {
        auto* imageLabel = new QLabel(bubble);
        imageLabel->setPixmap(thumb.scaled(220, 130, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        imageLabel->setStyleSheet("border-radius:6px; background:transparent;");
        inner->addWidget(imageLabel);
    }

    if (!text.isEmpty()) {
        auto* textLabel = new QLabel(text, bubble);
        textLabel->setWordWrap(true);
        textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        textLabel->setStyleSheet(
            role == User
                ? "color:#ffffff; font-size:10pt; font-family:Microsoft YaHei; background:transparent;"
                : "color:#e2e8f0; font-size:10pt; font-family:Microsoft YaHei; background:transparent;");
        inner->addWidget(textLabel);
    }

    auto* timeLabel = new QLabel(QDateTime::currentDateTime().toString("hh:mm"), bubble);
    timeLabel->setStyleSheet("color:rgba(255,255,255,0.35); font-size:8pt; background:transparent;");
    timeLabel->setAlignment(role == User ? Qt::AlignRight : Qt::AlignLeft);
    inner->addWidget(timeLabel);

    if (role == User) {
        bubble->setStyleSheet(
            "QWidget{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,"
            "stop:0 #667eea,stop:1 #764ba2);border-radius:14px;}");
        outer->addStretch();
        outer->addWidget(bubble);
    } else {
        bubble->setStyleSheet(
            "QWidget{background:rgba(45,55,72,0.92);"
            "border-radius:14px;"
            "border:1px solid rgba(102,126,234,0.2);}");
        outer->addWidget(bubble);
        outer->addStretch();
    }
}

ThinkingBubble::ThinkingBubble(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(8, 3, 8, 3);

    auto* bubble = new QWidget(this);
    bubble->setMaximumWidth(160);
    bubble->setStyleSheet(
        "QWidget{background:rgba(45,55,72,0.92);"
        "border-radius:14px;"
        "border:1px solid rgba(102,126,234,0.2);}");

    auto* inner = new QHBoxLayout(bubble);
    inner->setContentsMargins(14, 10, 14, 10);

    m_label = new QLabel(QStringLiteral("..."), bubble);
    m_label->setStyleSheet("color:#667eea; font-size:11pt; background:transparent;");
    inner->addWidget(m_label);

    outer->addWidget(bubble);
    outer->addStretch();

    m_timer = new QTimer(this);
    m_timer->setInterval(450);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        static const QString frames[] = {
            QStringLiteral(".  "),
            QStringLiteral(".. "),
            QStringLiteral("...")
        };
        m_label->setText(frames[m_step % 3]);
        ++m_step;
    });
    m_timer->start();
}

ThinkingBubble::~ThinkingBubble()
{
    if (m_timer) {
        m_timer->stop();
    }
}

AiChatView::AiChatView(QWidget* parent)
    : QWidget(parent)
{
    buildUI();
}

QString AiChatView::promptText() const
{
    return m_inputEdit ? m_inputEdit->toPlainText().trimmed() : QString();
}

void AiChatView::clearPrompt()
{
    if (m_inputEdit) {
        m_inputEdit->clear();
    }
}

void AiChatView::appendUserMessage(const QString& text, const QPixmap& thumb)
{
    appendBubble(ChatBubble::User, text, thumb);
}

void AiChatView::appendAssistantMessage(const QString& text)
{
    appendBubble(ChatBubble::Assistant, text);
}

void AiChatView::setModelName(const QString& modelName)
{
    if (m_modelLabel) {
        m_modelLabel->setText(modelName);
    }
}

void AiChatView::setApiKeyStatus(bool configured, const QString& message)
{
    if (!m_apiKeyStatusLabel) {
        return;
    }

    m_apiKeyStatusLabel->setText(message);
    m_apiKeyStatusLabel->setStyleSheet(
        configured
            ? "color:#68d391;font-size:8pt;font-family:Microsoft YaHei;"
              "background:rgba(72,187,120,0.10);border:1px solid rgba(72,187,120,0.25);"
              "border-radius:7px;padding:4px 7px;"
            : "color:#fbd38d;font-size:8pt;font-family:Microsoft YaHei;"
              "background:rgba(237,137,54,0.12);border:1px solid rgba(237,137,54,0.35);"
              "border-radius:7px;padding:4px 7px;");
}

void AiChatView::setStatusText(const QString& text)
{
    if (m_statusLabel) {
        m_statusLabel->setText(text);
    }
}

void AiChatView::clearStatus()
{
    setStatusText(QString());
}

void AiChatView::setThumbnail(const QPixmap& pixmap)
{
    if (!m_thumbLabel) {
        return;
    }

    m_thumbLabel->setPixmap(pixmap.scaled(m_thumbLabel->width(),
                                          m_thumbLabel->height(),
                                          Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation));
}

void AiChatView::clearThumbnail()
{
    if (!m_thumbLabel) {
        return;
    }

    m_thumbLabel->setPixmap(QPixmap());
    m_thumbLabel->setText(QStringLiteral("\u6682\u65e0\u622a\u56fe\uff0c\u70b9\u51fb\u4e0b\u65b9\u6309\u94ae\u622a\u53d6\u5f53\u524d\u753b\u9762"));
}

void AiChatView::setRequesting(bool requesting)
{
    if (requesting) {
        if (m_btnSend) {
            m_btnSend->setEnabled(true);
            m_btnSend->setText(QStringLiteral("\u53d6\u6d88"));
        }
        if (m_inputEdit) {
            m_inputEdit->setEnabled(false);
        }
        if (m_btnCapture) {
            m_btnCapture->setEnabled(false);
        }
        if (m_btnClearImg) {
            m_btnClearImg->setEnabled(false);
        }
        showThinking();
        return;
    }

    removeThinking();
    if (m_btnSend) {
        m_btnSend->setEnabled(true);
        m_btnSend->setText(QStringLiteral("\u53d1\u9001"));
    }
    if (m_inputEdit) {
        m_inputEdit->setEnabled(true);
    }
    if (m_btnCapture) {
        m_btnCapture->setEnabled(true);
    }
    if (m_btnClearImg) {
        m_btnClearImg->setEnabled(true);
    }
}

void AiChatView::buildUI()
{
    setMinimumWidth(320);
    setObjectName("AiChatView");
    setStyleSheet(
        "#AiChatView{background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "stop:0 #0f1117,stop:1 #1a202c);"
        "border-left:1px solid rgba(102,126,234,0.25);}");

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    auto* header = new QWidget(this);
    header->setFixedHeight(46);
    header->setStyleSheet(
        "background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #1a1f2e,stop:1 #252d40);"
        "border-bottom:1px solid rgba(102,126,234,0.3);");

    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 0, 12, 0);
    headerLayout->setSpacing(6);

    auto* iconLabel = new QLabel(QStringLiteral("[AI]"), header);
    iconLabel->setStyleSheet("font-size:16pt; background:transparent;");

    auto* titleLabel = new QLabel(QStringLiteral("AI \u5f71\u7247\u52a9\u624b"), header);
    titleLabel->setStyleSheet("color:#e2e8f0; font-size:11pt; font-weight:bold; font-family:Microsoft YaHei; background:transparent;");

    m_modelLabel = new QLabel(header);
    m_modelLabel->setStyleSheet(
        "color:#667eea; font-size:8pt; background:rgba(102,126,234,0.12);"
        "border:1px solid rgba(102,126,234,0.35); border-radius:7px; padding:2px 7px;");

    headerLayout->addWidget(iconLabel);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_modelLabel);
    m_mainLayout->addWidget(header);

    auto* captureArea = new QWidget(this);
    captureArea->setObjectName("capArea");
    captureArea->setStyleSheet("#capArea{background:rgba(15,17,23,0.7);border-bottom:1px solid rgba(102,126,234,0.12);}");

    auto* captureLayout = new QVBoxLayout(captureArea);
    captureLayout->setContentsMargins(10, 8, 10, 8);
    captureLayout->setSpacing(7);

    m_apiKeyStatusLabel = new QLabel(captureArea);
    m_apiKeyStatusLabel->setWordWrap(true);
    captureLayout->addWidget(m_apiKeyStatusLabel);

    m_thumbLabel = new QLabel(captureArea);
    m_thumbLabel->setFixedHeight(108);
    m_thumbLabel->setAlignment(Qt::AlignCenter);
    m_thumbLabel->setStyleSheet(
        "background:rgba(26,32,44,0.9); border:1px dashed rgba(102,126,234,0.35);"
        "border-radius:8px; color:rgba(160,174,192,0.55); font-size:9pt; font-family:Microsoft YaHei;");
    clearThumbnail();
    captureLayout->addWidget(m_thumbLabel);

    auto* captureButtonRow = new QHBoxLayout();
    captureButtonRow->setSpacing(7);

    m_btnCapture = new QPushButton(QStringLiteral("[CAM] \u622a\u53d6\u5f53\u524d\u753b\u9762"), captureArea);
    m_btnCapture->setFixedHeight(32);
    m_btnCapture->setStyleSheet(
        "QPushButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #4facfe,stop:1 #00f2fe);"
        "color:#0a1628;font-weight:bold;font-size:9pt;font-family:Microsoft YaHei;border:none;border-radius:7px;}"
        "QPushButton:hover{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #6fbdfe,stop:1 #33f5fe);}"
        "QPushButton:pressed{background:#3d9ae8;}");
    connect(m_btnCapture, &QPushButton::clicked, this, &AiChatView::captureRequested);

    m_btnClearImg = new QPushButton(QStringLiteral("\u6e05\u9664"), captureArea);
    m_btnClearImg->setFixedSize(52, 32);
    m_btnClearImg->setToolTip(QStringLiteral("\u6e05\u9664\u622a\u56fe"));
    m_btnClearImg->setStyleSheet(
        "QPushButton{background:rgba(113,128,150,0.25);color:#a0aec0;"
        "border:1px solid rgba(113,128,150,0.35);border-radius:7px;font-size:9pt;}"
        "QPushButton:hover{background:rgba(229,62,62,0.25);color:#fc8181;}");
    connect(m_btnClearImg, &QPushButton::clicked, this, &AiChatView::clearImageRequested);

    captureButtonRow->addWidget(m_btnCapture, 1);
    captureButtonRow->addWidget(m_btnClearImg);
    captureLayout->addLayout(captureButtonRow);
    m_mainLayout->addWidget(captureArea);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet(
        "QScrollArea{background:transparent;border:none;}"
        "QScrollBar:vertical{background:rgba(15,17,23,0.4);width:5px;border-radius:2px;}"
        "QScrollBar::handle:vertical{background:rgba(102,126,234,0.45);border-radius:2px;min-height:20px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");

    m_scrollContent = new QWidget();
    m_scrollContent->setStyleSheet("background:transparent;");
    m_chatLayout = new QVBoxLayout(m_scrollContent);
    m_chatLayout->setContentsMargins(0, 6, 0, 6);
    m_chatLayout->setSpacing(4);
    m_chatLayout->addStretch();
    m_scrollArea->setWidget(m_scrollContent);
    m_mainLayout->addWidget(m_scrollArea, 1);

    appendAssistantMessage(QStringLiteral("\u4f60\u597d\uff0c\u6211\u662f AI \u5f71\u7247\u52a9\u624b\u3002\n"
                                          "\u53ef\u4ee5\u622a\u53d6\u5f53\u524d\u753b\u9762\u540e\u63d0\u95ee\uff0c"
                                          "\u4e5f\u53ef\u4ee5\u76f4\u63a5\u8f93\u5165\u95ee\u9898\u3002"));

    auto* divider = new QFrame(this);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet("color:rgba(102,126,234,0.15);");
    m_mainLayout->addWidget(divider);

    auto* inputArea = new QWidget(this);
    inputArea->setObjectName("inputArea");
    inputArea->setStyleSheet("#inputArea{background:rgba(15,17,23,0.85);border-top:1px solid rgba(102,126,234,0.18);}");

    auto* inputLayout = new QVBoxLayout(inputArea);
    inputLayout->setContentsMargins(10, 8, 10, 10);
    inputLayout->setSpacing(7);

    m_inputEdit = new QTextEdit(inputArea);
    m_inputEdit->setFixedHeight(72);
    m_inputEdit->setPlaceholderText(QStringLiteral("\u8f93\u5165\u4f60\u7684\u95ee\u9898\uff0c\u4f8b\u5982\uff1a\u8fd9\u4e2a\u4eba\u7269\u662f\u8c01\uff1f"));
    m_inputEdit->setStyleSheet(
        "QTextEdit{background:rgba(26,32,44,0.9);color:#e2e8f0;"
        "border:1px solid rgba(102,126,234,0.3);border-radius:8px;"
        "padding:7px 10px;font-size:10pt;font-family:Microsoft YaHei;}"
        "QTextEdit:focus{border:1px solid rgba(102,126,234,0.7);}");
    inputLayout->addWidget(m_inputEdit);

    auto* sendRow = new QHBoxLayout();
    sendRow->setSpacing(8);

    m_statusLabel = new QLabel(QString(), inputArea);
    m_statusLabel->setStyleSheet("color:rgba(160,174,192,0.72);font-size:8pt;font-family:Microsoft YaHei;");
    m_statusLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_statusLabel->setWordWrap(true);

    m_btnSend = new QPushButton(QStringLiteral("\u53d1\u9001"), inputArea);
    m_btnSend->setFixedSize(72, 32);
    m_btnSend->setStyleSheet(
        "QPushButton{background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #667eea,stop:1 #5568d3);"
        "color:white;font-weight:bold;font-size:10pt;font-family:Microsoft YaHei;border:none;border-radius:8px;}"
        "QPushButton:hover{background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #7c8ff0,stop:1 #667eea);}"
        "QPushButton:pressed{background:#4a5bd0;}"
        "QPushButton:disabled{background:rgba(102,126,234,0.3);color:rgba(255,255,255,0.4);}");
    connect(m_btnSend, &QPushButton::clicked, this, &AiChatView::submitRequested);

    sendRow->addWidget(m_statusLabel);
    sendRow->addWidget(m_btnSend);
    inputLayout->addLayout(sendRow);
    m_mainLayout->addWidget(inputArea);
}

void AiChatView::appendBubble(ChatBubble::Role role,
                              const QString& text,
                              const QPixmap& thumb)
{
    auto* bubble = new ChatBubble(role, text, thumb, m_scrollContent);
    m_chatLayout->insertWidget(qMax(0, m_chatLayout->count() - 1), bubble);
    scrollToBottom();
}

void AiChatView::showThinking()
{
    if (m_thinking) {
        return;
    }

    m_thinking = new ThinkingBubble(m_scrollContent);
    m_chatLayout->insertWidget(qMax(0, m_chatLayout->count() - 1), m_thinking);
    scrollToBottom();
}

void AiChatView::removeThinking()
{
    if (!m_thinking) {
        return;
    }

    m_chatLayout->removeWidget(m_thinking);
    m_thinking->deleteLater();
    m_thinking = nullptr;
}

void AiChatView::scrollToBottom()
{
    QTimer::singleShot(50, this, [this]() {
        if (m_scrollArea && m_scrollArea->verticalScrollBar()) {
            m_scrollArea->verticalScrollBar()->setValue(m_scrollArea->verticalScrollBar()->maximum());
        }
    });
}
