#include "userrepository.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
const char* kPasswordAlgorithm = "plain";
}

UserRepository::UserRepository(IDatabaseProvider* provider)
    : m_dbContext(provider)
{
}

bool UserRepository::registerUser(const QString& username,
                                  const QString& password,
                                  const QString& email)
{
    if (!ensureReady()) {
        return false;
    }

    if (userExists(username)) {
        setLastError(QStringLiteral("username already exists"));
        return false;
    }
    if (!m_lastError.isEmpty()) {
        return false;
    }

    QSqlQuery query(m_dbContext.database());
    query.prepare(QStringLiteral(
        "INSERT INTO users (username, password, salt, password_algorithm, email) "
        "VALUES (:username, :password, :salt, :algorithm, :email)"));
    query.bindValue(QStringLiteral(":username"), username);
    query.bindValue(QStringLiteral(":password"), password);
    query.bindValue(QStringLiteral(":salt"), QString());
    query.bindValue(QStringLiteral(":algorithm"), QString::fromLatin1(kPasswordAlgorithm));
    query.bindValue(QStringLiteral(":email"), email);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    m_lastError.clear();
    return true;
}

bool UserRepository::loginUser(const QString& username, const QString& password)
{
    if (!verifyStoredPassword(username, password)) {
        return false;
    }

    updateLoginInfo(username);
    m_lastError.clear();
    return true;
}

bool UserRepository::verifyUserPassword(const QString& username, const QString& password)
{
    return verifyStoredPassword(username, password);
}

bool UserRepository::userExists(const QString& username)
{
    if (!ensureReady()) {
        return false;
    }

    QSqlQuery query(m_dbContext.database());
    query.prepare(QStringLiteral("SELECT id FROM users WHERE username = :username"));
    query.bindValue(QStringLiteral(":username"), username);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    const bool exists = query.next();
    m_lastError.clear();
    return exists;
}

int UserRepository::getLoginCount(const QString& username)
{
    if (!ensureReady()) {
        return 0;
    }

    QSqlQuery query(m_dbContext.database());
    query.prepare(QStringLiteral("SELECT login_count FROM users WHERE username = :username"));
    query.bindValue(QStringLiteral(":username"), username);

    if (query.exec() && query.next()) {
        m_lastError.clear();
        return query.value(0).toInt();
    }

    setLastError(query.lastError().text());
    return 0;
}

bool UserRepository::changePassword(const QString& username,
                                    const QString& oldPassword,
                                    const QString& newPassword)
{
    if (!verifyStoredPassword(username, oldPassword)) {
        setLastError(QStringLiteral("old password is incorrect"));
        return false;
    }

    return updatePassword(username, newPassword);
}

QString UserRepository::lastError() const
{
    return m_lastError;
}

bool UserRepository::ensureReady()
{
    if (!m_dbContext.initialize()) {
        setLastError(m_dbContext.lastError());
        return false;
    }

    return true;
}

void UserRepository::updateLoginInfo(const QString& username)
{
    QSqlQuery query(m_dbContext.database());
    query.prepare(QStringLiteral(
        "UPDATE users "
        "SET last_login = :last_login, login_count = login_count + 1 "
        "WHERE username = :username"));
    query.bindValue(QStringLiteral(":last_login"), QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(QStringLiteral(":username"), username);

    if (!query.exec()) {
        setLastError(query.lastError().text());
    }
}

void UserRepository::setLastError(const QString& message)
{
    m_lastError = message;
}

bool UserRepository::verifyStoredPassword(const QString& username, const QString& password)
{
    if (!ensureReady()) {
        return false;
    }

    QSqlQuery query(m_dbContext.database());
    query.prepare(QStringLiteral(
        "SELECT id FROM users WHERE username = :username AND password = :password"));
    query.bindValue(QStringLiteral(":username"), username);
    query.bindValue(QStringLiteral(":password"), password);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    if (!query.next()) {
        setLastError(QStringLiteral("username or password is incorrect"));
        return false;
    }

    m_lastError.clear();
    return true;
}

bool UserRepository::updatePassword(const QString& username, const QString& password)
{
    if (!ensureReady()) {
        return false;
    }

    QSqlQuery query(m_dbContext.database());
    query.prepare(QStringLiteral(
        "UPDATE users "
        "SET password = :password, salt = :salt, password_algorithm = :algorithm "
        "WHERE username = :username"));
    query.bindValue(QStringLiteral(":password"), password);
    query.bindValue(QStringLiteral(":salt"), QString());
    query.bindValue(QStringLiteral(":algorithm"), QString::fromLatin1(kPasswordAlgorithm));
    query.bindValue(QStringLiteral(":username"), username);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    m_lastError.clear();
    return true;
}
