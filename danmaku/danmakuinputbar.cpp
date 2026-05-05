#include "danmakuinputbar.h"

#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>

#include "usersession.h"

DanmakuInputBar::DanmakuInputBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(50);
    createUi();
    connectSignals();
    updateUi();
}

void DanmakuInputBar::setUserSession(UserSession* session)
{
    if (m_userSession == session) {
        return;
    }

    if (m_userSession) {
        disconnect(m_userSession, &UserSession::sessionChanged, this, &DanmakuInputBar::setSessionState);
    }

    m_userSession = session;
    setCurrentUser(m_userSession ? m_userSession->currentUser() : QString());

    if (m_userSession) {
        connect(m_userSession, &UserSession::sessionChanged, this, &DanmakuInputBar::setSessionState);
    }
}

QString DanmakuInputBar::currentUser() const
{
    return m_currentUser;
}

bool DanmakuInputBar::isLoggedIn() const
{
    return !m_currentUser.isEmpty();
}

void DanmakuInputBar::setCurrentUser(const QString& username)
{
    m_currentUser = username;
    updateUi();
}

void DanmakuInputBar::setSessionState(const SessionState& state)
{
    setCurrentUser(state.username);
}

bool DanmakuInputBar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_inputEdit
        && !isLoggedIn()
        && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::FocusIn)) {
        emit loginRequired();
        return true;
    }

    return QWidget::eventFilter(watched, event);
}

void DanmakuInputBar::createUi()
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 5, 10, 5);
    layout->setSpacing(10);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(QStringLiteral("滚动"), 0);
    m_typeCombo->addItem(QStringLiteral("顶部"), 1);
    m_typeCombo->addItem(QStringLiteral("底部"), 2);
    m_typeCombo->setFixedWidth(100);
    m_typeCombo->setStyleSheet(
        "QComboBox {"
        "   background: rgba(45, 55, 72, 0.9);"
        "   color: #e2e8f0;"
        "   border: 2px solid rgba(102, 126, 234, 0.5);"
        "   border-radius: 8px;"
        "   padding: 6px 10px;"
        "   font-size: 10pt;"
        "   font-weight: 500;"
        "}"
        "QComboBox::drop-down {"
        "   border: none;"
        "   width: 20px;"
        "}"
        "QComboBox::down-arrow {"
        "   image: none;"
        "   border-left: 4px solid transparent;"
        "   border-right: 4px solid transparent;"
        "   border-top: 6px solid #a0aec0;"
        "}"
        "QComboBox QAbstractItemView {"
        "   background: #2d3748;"
        "   color: #e2e8f0;"
        "   border: 2px solid rgba(102, 126, 234, 0.5);"
        "   selection-background-color: #667eea;"
        "}");

    m_colorButton = new QPushButton(QStringLiteral("色"), this);
    m_colorButton->setFixedSize(40, 40);
    m_colorButton->setToolTip(QStringLiteral("选择弹幕颜色"));
    updateColorButtonStyle();

    m_inputEdit = new QLineEdit(this);
    m_inputEdit->setMaxLength(100);
    m_inputEdit->setStyleSheet(
        "QLineEdit {"
        "   background: rgba(45, 55, 72, 0.9);"
        "   color: #ffffff;"
        "   border: 2px solid rgba(102, 126, 234, 0.5);"
        "   border-radius: 8px;"
        "   padding: 8px 12px;"
        "   font-size: 11pt;"
        "   font-family: 'Microsoft YaHei';"
        "}"
        "QLineEdit:focus {"
        "   border: 2px solid #667eea;"
        "   background: rgba(55, 65, 82, 0.9);"
        "}"
        "QLineEdit::placeholder {"
        "   color: #9ca3af;"
        "}");
    m_inputEdit->installEventFilter(this);

    m_sendButton = new QPushButton(QStringLiteral("发送"), this);
    m_sendButton->setFixedSize(80, 40);
    m_sendButton->setStyleSheet(
        "QPushButton {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "       stop:0 #667eea, stop:1 #5568d3);"
        "   color: white;"
        "   border: none;"
        "   border-radius: 8px;"
        "   font-size: 11pt;"
        "   font-weight: bold;"
        "   font-family: 'Microsoft YaHei';"
        "}"
        "QPushButton:hover {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "       stop:0 #7c8ff0, stop:1 #667eea);"
        "}"
        "QPushButton:pressed {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "       stop:0 #5568d3, stop:1 #4451b8);"
        "}");

    layout->addWidget(m_typeCombo);
    layout->addWidget(m_colorButton);
    layout->addWidget(m_inputEdit, 1);
    layout->addWidget(m_sendButton);
    setLayout(layout);
}

void DanmakuInputBar::connectSignals()
{
    connect(m_sendButton, &QPushButton::clicked, this, &DanmakuInputBar::onSendClicked);
    connect(m_inputEdit, &QLineEdit::returnPressed, this, &DanmakuInputBar::onSendClicked);
    connect(m_colorButton, &QPushButton::clicked, this, &DanmakuInputBar::onColorButtonClicked);
}

void DanmakuInputBar::updateUi()
{
    const bool loggedIn = isLoggedIn();
    m_inputEdit->setReadOnly(!loggedIn);
    m_typeCombo->setEnabled(loggedIn);
    m_colorButton->setEnabled(loggedIn);

    if (loggedIn) {
        m_inputEdit->setPlaceholderText(QStringLiteral("发个弹幕吧，%1...").arg(m_currentUser));
    } else {
        m_inputEdit->clear();
        m_inputEdit->setPlaceholderText(QStringLiteral("请先登录后发送弹幕..."));
    }
}

void DanmakuInputBar::updateColorButtonStyle()
{
    const QString style = QString(
        "QPushButton {"
        "   background: %1;"
        "   color: #ffffff;"
        "   border: 2px solid rgba(255, 255, 255, 0.3);"
        "   border-radius: 8px;"
        "   font-size: 10pt;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   border: 2px solid rgba(255, 255, 255, 0.6);"
        "}")
                              .arg(m_selectedColor);
    m_colorButton->setStyleSheet(style);
}

void DanmakuInputBar::onSendClicked()
{
    if (!isLoggedIn()) {
        emit loginRequired();
        return;
    }

    const QString content = m_inputEdit->text().trimmed();
    if (content.isEmpty()) {
        return;
    }

    emit danmakuSubmitted(content, m_selectedColor, m_typeCombo->currentData().toInt());
    m_inputEdit->clear();
    m_inputEdit->setFocus();
}

void DanmakuInputBar::onColorButtonClicked()
{
    const QColor color = QColorDialog::getColor(QColor(m_selectedColor), this, QStringLiteral("选择弹幕颜色"));
    if (!color.isValid()) {
        return;
    }

    m_selectedColor = color.name();
    updateColorButtonStyle();
}
