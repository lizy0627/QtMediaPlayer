#include "authdialogcontroller.h"

#include <QAction>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPoint>
#include <QPushButton>
#include <QVBoxLayout>

#include "authservice.h"
#include "logindialog.h"

AuthDialogController::AuthDialogController(AuthService* authService, QObject* parent)
    : QObject(parent)
    , m_authService(authService)
{
}

AuthService* AuthDialogController::authService() const
{
    return m_authService;
}

void AuthDialogController::showLoginDialog(QWidget* parent)
{
    if (!m_authService || m_authService->isLoggedIn()) {
        return;
    }

    if (!m_authService->initialize()) {
        QMessageBox::warning(parent,
                             QStringLiteral("错误"),
                             QStringLiteral("数据库未连接！\n\n请检查数据库配置。"));
        return;
    }

    LoginDialog loginDialog(parent);
    connect(&loginDialog, &LoginDialog::loginRequested, &loginDialog,
            [this, &loginDialog](const QString& username, const QString& password) {
                if (!m_authService->login(username, password)) {
                    loginDialog.setStatus(m_authService->lastError());
                    loginDialog.clearPassword();
                    return;
                }

                QMessageBox::information(&loginDialog,
                                         QStringLiteral("登录成功"),
                                         QStringLiteral("欢迎回来，%1！\n\n这是您第 %2 次登录。")
                                             .arg(m_authService->currentUser())
                                             .arg(m_authService->loginCount()));
                loginDialog.accept();
            });

    connect(&loginDialog, &LoginDialog::registerRequested, &loginDialog,
            [this, &loginDialog](const QString& username, const QString& password) {
                if (!m_authService->registerUser(username, password)) {
                    loginDialog.setStatus(m_authService->lastError());
                    loginDialog.clearPassword();
                    return;
                }

                QMessageBox::information(&loginDialog,
                                         QStringLiteral("注册成功"),
                                         QStringLiteral("用户 %1 注册成功！\n\n现在可以使用该账号登录了。")
                                             .arg(username.trimmed()));
                loginDialog.setStatus(QStringLiteral("注册成功，请登录"), true);
                loginDialog.clearPassword();
            });

    loginDialog.exec();
}

void AuthDialogController::showUserMenu(QWidget* parent, const QPoint& pos)
{
    if (!m_authService) {
        return;
    }

    if (!m_authService->isLoggedIn()) {
        showLoginDialog(parent);
        return;
    }

    QMenu userMenu(parent);
    userMenu.setStyleSheet(
        "QMenu { "
        "   background-color: #2b2b2b; "
        "   color: white; "
        "   border: 1px solid #444; "
        "}"
        "QMenu::item { "
        "   padding: 8px 25px; "
        "}"
        "QMenu::item:selected { "
        "   background-color: #667eea; "
        "}"
    );

    QAction* userInfoAction = userMenu.addAction(QStringLiteral("当前用户: %1").arg(m_authService->currentUser()));
    userInfoAction->setEnabled(false);

    userMenu.addSeparator();

    QAction* statsAction = userMenu.addAction(QStringLiteral("登录次数: %1").arg(m_authService->loginCount()));
    statsAction->setEnabled(false);

    QAction* danmakuStatsAction = userMenu.addAction(
        QStringLiteral("发送弹幕: %1 条").arg(m_authService->danmakuCount()));
    danmakuStatsAction->setEnabled(false);

    userMenu.addSeparator();
    QAction* changePasswordAction = userMenu.addAction(QStringLiteral("修改密码"));
    QAction* logoutAction = userMenu.addAction(QStringLiteral("退出登录"));

    QAction* selectedAction = userMenu.exec(pos);
    if (selectedAction == changePasswordAction) {
        showChangePasswordDialog(parent);
    } else if (selectedAction == logoutAction) {
        m_authService->logout();
        QMessageBox::information(parent, QStringLiteral("提示"), QStringLiteral("已退出登录"));
    }
}

void AuthDialogController::showChangePasswordDialog(QWidget* parent)
{
    if (!m_authService) {
        return;
    }

    if (!m_authService->isLoggedIn()) {
        m_authService->requestLogin();
        return;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("修改密码"));
    dialog.setFixedSize(480, 520);
    dialog.setModal(true);
    dialog.setStyleSheet(
        "QDialog { "
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, "
        "       stop:0 #1a202c, stop:0.5 #2d3748, stop:1 #1a202c); "
        "}"
        "QLabel { "
        "   color: #e2e8f0; "
        "   font-size: 11pt; "
        "}"
        "QLineEdit { "
        "   background-color: #2d3748; "
        "   color: #ffffff; "
        "   border: 2px solid #667eea; "
        "   border-radius: 8px; "
        "   padding: 10px; "
        "   font-size: 11pt; "
        "   min-height: 35px; "
        "}"
        "QPushButton { "
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "       stop:0 #667eea, stop:1 #5568d3); "
        "   color: white; "
        "   border: none; "
        "   border-radius: 8px; "
        "   padding: 12px; "
        "   font-size: 11pt; "
        "   font-weight: bold; "
        "}"
        "QPushButton:hover { "
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "       stop:0 #7c8ff0, stop:1 #667eea); "
        "}"
    );

    auto* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(15);
    layout->setContentsMargins(35, 35, 35, 35);

    auto* titleLabel = new QLabel(QStringLiteral("修改密码"), &dialog);
    titleLabel->setStyleSheet("font-size: 18pt; font-weight: bold; color: #64b5f6;");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);
    layout->addSpacing(20);

    auto* oldPwdLabel = new QLabel(QStringLiteral("原密码"), &dialog);
    oldPwdLabel->setStyleSheet("font-size: 11pt; color: #e2e8f0;");
    layout->addWidget(oldPwdLabel);
    layout->addSpacing(8);

    auto* oldPwdEdit = new QLineEdit(&dialog);
    oldPwdEdit->setEchoMode(QLineEdit::Password);
    oldPwdEdit->setPlaceholderText(QStringLiteral("请输入原密码"));
    oldPwdEdit->setMinimumHeight(45);
    layout->addWidget(oldPwdEdit);
    layout->addSpacing(20);

    auto* newPwdLabel = new QLabel(QStringLiteral("新密码"), &dialog);
    newPwdLabel->setStyleSheet("font-size: 11pt; color: #e2e8f0;");
    layout->addWidget(newPwdLabel);
    layout->addSpacing(8);

    auto* newPwdEdit = new QLineEdit(&dialog);
    newPwdEdit->setEchoMode(QLineEdit::Password);
    newPwdEdit->setPlaceholderText(QStringLiteral("请输入新密码（至少6位）"));
    newPwdEdit->setMinimumHeight(45);
    layout->addWidget(newPwdEdit);
    layout->addSpacing(20);

    auto* confirmPwdLabel = new QLabel(QStringLiteral("确认密码"), &dialog);
    confirmPwdLabel->setStyleSheet("font-size: 11pt; color: #e2e8f0;");
    layout->addWidget(confirmPwdLabel);
    layout->addSpacing(8);

    auto* confirmPwdEdit = new QLineEdit(&dialog);
    confirmPwdEdit->setEchoMode(QLineEdit::Password);
    confirmPwdEdit->setPlaceholderText(QStringLiteral("请再次输入新密码"));
    confirmPwdEdit->setMinimumHeight(45);
    layout->addWidget(confirmPwdEdit);
    layout->addSpacing(20);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(15);
    auto* confirmBtn = new QPushButton(QStringLiteral("确认修改"), &dialog);
    confirmBtn->setMinimumHeight(45);
    auto* cancelBtn = new QPushButton(QStringLiteral("取消"), &dialog);
    cancelBtn->setMinimumHeight(45);
    cancelBtn->setStyleSheet(
        "QPushButton { "
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "       stop:0 #718096, stop:1 #4a5568); "
        "}"
    );

    btnLayout->addWidget(confirmBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    connect(confirmBtn, &QPushButton::clicked, &dialog, [this, &dialog, oldPwdEdit, newPwdEdit, confirmPwdEdit]() {
        const QString oldPwd = oldPwdEdit->text();
        const QString newPwd = newPwdEdit->text();
        const QString confirmPwd = confirmPwdEdit->text();

        if (newPwd != confirmPwd) {
            QMessageBox::warning(&dialog, QStringLiteral("提示"), QStringLiteral("两次输入的新密码不一致！"));
            return;
        }

        if (m_authService->changePassword(oldPwd, newPwd)) {
            QMessageBox::information(&dialog, QStringLiteral("成功"), QStringLiteral("密码修改成功！"));
            dialog.accept();
        } else {
            QMessageBox::warning(&dialog, QStringLiteral("失败"), m_authService->lastError());
        }
    });

    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    dialog.exec();
}
