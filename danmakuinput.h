#ifndef DANMAKUINPUT_H
#define DANMAKUINPUT_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QColorDialog>
#include <QMessageBox>
#include "danmakumanager.h"
#include "usersession.h"

// Legacy danmaku input widget.
// Kept for compatibility only; new code should use danmaku/DanmakuInputBar.
// 弹幕输入组件
class DanmakuInput : public QWidget
{
    Q_OBJECT

private:
    QLineEdit* m_inputEdit;          // 输入框
    QPushButton* m_sendButton;       // 发送按钮
    QPushButton* m_colorButton;      // 颜色选择按钮
    QComboBox* m_typeCombo;          // 弹幕类型选择
    QString m_selectedColor;         // 选中的颜色
    QString m_currentUser;           // 当前用户
    UserSession* m_userSession;      // 用户会话
    bool m_enabled;                  // 是否启用

public:
    explicit DanmakuInput(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_selectedColor("#FFFFFF")
        , m_userSession(nullptr)
        , m_enabled(false)
    {
        setFixedHeight(50);
        createUI();
        connectSignals();
    }

    // 设置当前用户
    void setCurrentUser(const QString &username)
    {
        m_currentUser = username;
        m_enabled = !username.isEmpty();
        updateUI();
    }

    void setSessionState(const SessionState& state)
    {
        setCurrentUser(state.username);
    }

    // 绑定全局用户会话
    void setUserSession(UserSession* session)
    {
        if (m_userSession == session) {
            return;
        }

        if (m_userSession) {
            disconnect(m_userSession, &UserSession::sessionChanged, this, &DanmakuInput::setSessionState);
        }

        m_userSession = session;
        setCurrentUser(m_userSession ? m_userSession->currentUser() : QString());

        if (m_userSession) {
            connect(m_userSession, &UserSession::sessionChanged, this, &DanmakuInput::setSessionState);
        }
    }

    // 获取当前用户
    QString currentUser() const
    {
        return m_currentUser;
    }

    // 是否已登录
    bool isLoggedIn() const
    {
        return !m_currentUser.isEmpty();
    }

signals:
    // 发送弹幕信号
    void sendDanmaku(const QString &content, const QString &color, int type);
    
    // 请求登录信号
    void loginRequired();

private:
    void createUI()
    {
        QHBoxLayout* layout = new QHBoxLayout(this);
        layout->setContentsMargins(10, 5, 10, 5);
        layout->setSpacing(10);

        // 弹幕类型选择
        m_typeCombo = new QComboBox(this);
        m_typeCombo->addItem("💬 滚动", 0);
        m_typeCombo->addItem("⬆️ 顶部", 1);
        m_typeCombo->addItem("⬇️ 底部", 2);
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
            "}"
        );

        // 颜色选择按钮
        m_colorButton = new QPushButton("🎨", this);
        m_colorButton->setFixedSize(40, 40);
        m_colorButton->setToolTip("选择弹幕颜色");
        updateColorButtonStyle();

        // 输入框
        m_inputEdit = new QLineEdit(this);
        m_inputEdit->setPlaceholderText("请先登录后发送弹幕...");
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
            "}"
        );

        // 发送按钮
        m_sendButton = new QPushButton("发送", this);
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
            "}"
            "QPushButton:disabled {"
            "   background: rgba(160, 174, 192, 0.3);"
            "   color: #718096;"
            "}"
        );

        layout->addWidget(m_typeCombo);
        layout->addWidget(m_colorButton);
        layout->addWidget(m_inputEdit, 1);
        layout->addWidget(m_sendButton);

        setLayout(layout);
        updateUI();
    }

    void connectSignals()
    {
        // 发送按钮点击
        connect(m_sendButton, &QPushButton::clicked, this, &DanmakuInput::onSendClicked);

        // 回车发送
        connect(m_inputEdit, &QLineEdit::returnPressed, this, &DanmakuInput::onSendClicked);

        // 颜色选择
        connect(m_colorButton, &QPushButton::clicked, this, &DanmakuInput::onColorButtonClicked);
        
        // 输入框点击
        connect(m_inputEdit, &QLineEdit::selectionChanged, this, [this]() {
            if (!m_enabled) {
                m_inputEdit->clearFocus();
                emit loginRequired();
            }
        });
    }

    void onSendClicked()
    {
        if (!m_enabled) {
            QMessageBox::information(this, "提示", "请先登录后再发送弹幕！");
            emit loginRequired();
            return;
        }

        QString content = m_inputEdit->text().trimmed();
        
        if (content.isEmpty()) {
            QMessageBox::warning(this, "提示", "弹幕内容不能为空！");
            return;
        }

        if (content.length() > 100) {
            QMessageBox::warning(this, "提示", "弹幕内容不能超过100个字符！");
            return;
        }

        int type = m_typeCombo->currentData().toInt();
        
        emit sendDanmaku(content, m_selectedColor, type);
        
        // 清空输入框
        m_inputEdit->clear();
        m_inputEdit->setFocus();
    }

    void onColorButtonClicked()
    {
        QColor color = QColorDialog::getColor(QColor(m_selectedColor), this, "选择弹幕颜色");
        
        if (color.isValid()) {
            m_selectedColor = color.name();
            updateColorButtonStyle();
        }
    }

    void updateColorButtonStyle()
    {
        QString style = QString(
            "QPushButton {"
            "   background: %1;"
            "   border: 2px solid rgba(255, 255, 255, 0.3);"
            "   border-radius: 8px;"
            "   font-size: 16pt;"
            "}"
            "QPushButton:hover {"
            "   border: 2px solid rgba(255, 255, 255, 0.6);"
            "}"
        ).arg(m_selectedColor);
        
        m_colorButton->setStyleSheet(style);
    }

    void updateUI()
    {
        m_inputEdit->setEnabled(m_enabled);
        m_sendButton->setEnabled(m_enabled);
        m_typeCombo->setEnabled(m_enabled);
        m_colorButton->setEnabled(m_enabled);
        
        if (m_enabled) {
            m_inputEdit->setPlaceholderText(QString("发个弹幕吧，%1...").arg(m_currentUser));
        } else {
            m_inputEdit->setPlaceholderText("请先登录后发送弹幕...");
        }
    }
};

#endif // DANMAKUINPUT_H
