#include "authservice.h"

#include <QDebug>
#include <utility>

#include "dbmanager.h"
#include "usersession.h"

AuthService::AuthService(UserSession* session, QObject* parent)
    : QObject(parent)
    , m_accountService(new UserAccountService(this))
    , m_session(session)
{
    if (!m_session) {
        m_session = new UserSession(this);
    }
}

bool AuthService::initialize()
{
    if (m_initialized) {
        return true;
    }

    if (!m_accountService) {
        setLastError(QStringLiteral("数据库管理器未初始化"));
        return false;
    }

    if (!m_accountService->ensureReady()) {
        setLastError(QStringLiteral("数据库连接失败: %1").arg(m_accountService->lastError()));
        qDebug() << m_lastError;
        return false;
    }

    m_accountService->seedDevelopmentUsers();

    m_lastError.clear();
    m_initialized = true;
    return true;
}

bool AuthService::isDatabaseConnected() const
{
    return m_accountService && m_accountService->isConnected();
}

bool AuthService::isLoggedIn() const
{
    return m_session && m_session->isLoggedIn();
}

QString AuthService::currentUser() const
{
    return m_session ? m_session->currentUser() : QString();
}

QString AuthService::lastError() const
{
    return m_lastError;
}

int AuthService::loginCount(const QString& username) const
{
    if (!m_accountService || !m_accountService->isConnected()) {
        return 0;
    }

    const QString targetUser = username.isEmpty() ? currentUser() : username;
    return targetUser.isEmpty() ? 0 : m_accountService->getLoginCount(targetUser);
}

int AuthService::danmakuCount(const QString& username) const
{
    const QString targetUser = username.isEmpty() ? currentUser() : username;
    return m_danmakuCountProvider && !targetUser.isEmpty()
        ? m_danmakuCountProvider(targetUser)
        : 0;
}

UserSession* AuthService::session() const
{
    return m_session;
}

void AuthService::setDanmakuCountProvider(std::function<int(const QString&)> provider)
{
    m_danmakuCountProvider = std::move(provider);
}

bool AuthService::login(const QString& username, const QString& password)
{
    if (!ensureDatabaseReady()) {
        return false;
    }

    const QString trimmedUsername = username.trimmed();
    if (trimmedUsername.isEmpty()) {
        setLastError(QStringLiteral("请输入用户名"));
        return false;
    }

    if (password.isEmpty()) {
        setLastError(QStringLiteral("请输入密码"));
        return false;
    }

    if (!m_accountService->loginUser(trimmedUsername, password)) {
        setLastError(m_accountService->lastError());
        return false;
    }

    if (m_session) {
        SessionState state;
        state.loggedIn = true;
        state.username = trimmedUsername;
        state.loginCount = loginCount(trimmedUsername);
        m_session->setState(state);
    }

    m_lastError.clear();
    return true;
}

bool AuthService::registerUser(const QString& username, const QString& password)
{
    if (!ensureDatabaseReady()) {
        return false;
    }

    const QString trimmedUsername = username.trimmed();
    if (trimmedUsername.isEmpty()) {
        setLastError(QStringLiteral("请输入用户名"));
        return false;
    }

    if (password.isEmpty()) {
        setLastError(QStringLiteral("请输入密码"));
        return false;
    }

    if (password.length() < 6) {
        setLastError(QStringLiteral("密码长度至少6位"));
        return false;
    }

    if (!m_accountService->registerUser(trimmedUsername, password)) {
        setLastError(m_accountService->lastError());
        return false;
    }

    m_lastError.clear();
    return true;
}

void AuthService::logout()
{
    if (m_session) {
        m_session->clear();
    }

    m_lastError.clear();
}

bool AuthService::changePassword(const QString& oldPassword, const QString& newPassword)
{
    if (!isLoggedIn()) {
        emit loginRequired();
        return false;
    }

    if (!ensureDatabaseReady()) {
        return false;
    }

    if (oldPassword.isEmpty() || newPassword.isEmpty()) {
        setLastError(QStringLiteral("请填写所有字段"));
        return false;
    }

    if (newPassword.length() < 6) {
        setLastError(QStringLiteral("新密码长度至少6位"));
        return false;
    }

    if (!m_accountService->changePassword(currentUser(), oldPassword, newPassword)) {
        setLastError(m_accountService->lastError());
        return false;
    }

    m_lastError.clear();
    return true;
}

void AuthService::requestLogin()
{
    if (!isLoggedIn()) {
        emit loginRequired();
    }
}

bool AuthService::ensureDatabaseReady()
{
    return m_initialized || initialize();
}

void AuthService::setLastError(const QString& message)
{
    m_lastError = message;
    emit errorOccurred(m_lastError);
}
