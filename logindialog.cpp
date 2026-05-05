#include "logindialog.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QStyle>
#include <QVBoxLayout>

LoginDialog::LoginDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("用户登录"));
    setObjectName("LoginDialog");
    setFixedSize(520, 580);
    setModal(true);

    createUI();
    connectSignals();
    loadRememberedUsername();
}

QString LoginDialog::username() const
{
    return m_usernameEdit->text().trimmed();
}

QString LoginDialog::password() const
{
    return m_passwordEdit->text();
}

void LoginDialog::setStatus(const QString& message, bool success)
{
    m_statusLabel->setText(message);
    m_statusLabel->setProperty("state", success ? "success" : "error");
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
}

void LoginDialog::clearPassword()
{
    m_passwordEdit->clear();
    m_passwordEdit->setFocus();
}

void LoginDialog::createUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(35, 30, 35, 30);

    auto* titleLabel = new QLabel(QStringLiteral("Qt 影音系统"), this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setProperty("role", "title");
    mainLayout->addWidget(titleLabel);
    mainLayout->addSpacing(10);

    auto* formGroup = new QGroupBox(QStringLiteral("用户登录"), this);
    auto* formLayout = new QVBoxLayout(formGroup);
    formLayout->setSpacing(15);
    formLayout->setContentsMargins(30, 35, 30, 25);

    auto* usernameLabel = new QLabel(QStringLiteral("用户名"), this);
    formLayout->addWidget(usernameLabel);
    formLayout->addSpacing(10);

    m_usernameEdit = new QLineEdit(this);
    m_usernameEdit->setPlaceholderText(QStringLiteral("请输入用户名"));
    m_usernameEdit->setMaxLength(50);
    m_usernameEdit->setMinimumHeight(45);
    formLayout->addWidget(m_usernameEdit);
    formLayout->addSpacing(35);

    auto* passwordLabel = new QLabel(QStringLiteral("密码"), this);
    formLayout->addWidget(passwordLabel);
    formLayout->addSpacing(10);

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setPlaceholderText(QStringLiteral("请输入密码"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setMaxLength(50);
    m_passwordEdit->setMinimumHeight(45);
    formLayout->addWidget(m_passwordEdit);
    formLayout->addSpacing(20);

    m_rememberCheck = new QCheckBox(QStringLiteral("记住用户名"), this);
    formLayout->addWidget(m_rememberCheck);

    mainLayout->addWidget(formGroup);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setProperty("role", "status");
    m_statusLabel->setProperty("state", "error");
    m_statusLabel->setWordWrap(true);
    mainLayout->addWidget(m_statusLabel);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(15);

    m_loginButton = new QPushButton(QStringLiteral("登录"), this);
    m_loginButton->setCursor(Qt::PointingHandCursor);
    m_loginButton->setMinimumHeight(50);

    m_registerButton = new QPushButton(QStringLiteral("注册"), this);
    m_registerButton->setProperty("role", "primaryAlt");
    m_registerButton->setCursor(Qt::PointingHandCursor);
    m_registerButton->setMinimumHeight(50);

    m_cancelButton = new QPushButton(QStringLiteral("取消"), this);
    m_cancelButton->setProperty("role", "secondary");
    m_cancelButton->setCursor(Qt::PointingHandCursor);
    m_cancelButton->setMinimumHeight(50);

    buttonLayout->addWidget(m_loginButton);
    buttonLayout->addWidget(m_registerButton);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(buttonLayout);
    mainLayout->addStretch();
}

void LoginDialog::connectSignals()
{
    connect(m_loginButton, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(m_registerButton, &QPushButton::clicked, this, &LoginDialog::onRegisterClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLoginClicked);
    connect(m_usernameEdit, &QLineEdit::returnPressed, m_passwordEdit, qOverload<>(&QLineEdit::setFocus));
}

bool LoginDialog::validateBasicInput()
{
    if (username().isEmpty()) {
        setStatus(QStringLiteral("请输入用户名"));
        m_usernameEdit->setFocus();
        return false;
    }

    if (password().isEmpty()) {
        setStatus(QStringLiteral("请输入密码"));
        m_passwordEdit->setFocus();
        return false;
    }

    return true;
}

void LoginDialog::loadRememberedUsername()
{
    QSettings settings;
    const QString rememberedUsername = settings.value(QStringLiteral("auth/rememberedUsername")).toString();
    if (rememberedUsername.isEmpty()) {
        return;
    }

    m_usernameEdit->setText(rememberedUsername);
    m_rememberCheck->setChecked(true);
    m_passwordEdit->setFocus();
}

void LoginDialog::persistRememberedUsername()
{
    QSettings settings;
    if (m_rememberCheck->isChecked()) {
        settings.setValue(QStringLiteral("auth/rememberedUsername"), username());
    } else {
        settings.remove(QStringLiteral("auth/rememberedUsername"));
    }
}

void LoginDialog::onLoginClicked()
{
    if (validateBasicInput()) {
        persistRememberedUsername();
        emit loginRequested(username(), password());
    }
}

void LoginDialog::onRegisterClicked()
{
    if (validateBasicInput()) {
        persistRememberedUsername();
        emit registerRequested(username(), password());
    }
}
