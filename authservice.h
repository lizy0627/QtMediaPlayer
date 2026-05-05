#ifndef AUTHSERVICE_H
#define AUTHSERVICE_H

#include <QObject>
#include <QString>
#include <functional>

class UserAccountService;
class UserSession;

class AuthService : public QObject
{
    Q_OBJECT

public:
    explicit AuthService(UserSession* session, QObject* parent = nullptr);

    bool initialize();
    bool isDatabaseConnected() const;
    bool isLoggedIn() const;
    QString currentUser() const;
    QString lastError() const;
    int loginCount(const QString& username = QString()) const;
    int danmakuCount(const QString& username = QString()) const;
    UserSession* session() const;

    void setDanmakuCountProvider(std::function<int(const QString&)> provider);

public slots:
    bool login(const QString& username, const QString& password);
    bool registerUser(const QString& username, const QString& password);
    void logout();
    bool changePassword(const QString& oldPassword, const QString& newPassword);
    void requestLogin();

signals:
    void loginRequired();
    void errorOccurred(const QString& message);

private:
    bool ensureDatabaseReady();
    void setLastError(const QString& message);

    UserAccountService* m_accountService = nullptr;
    UserSession* m_session = nullptr;
    bool m_initialized = false;
    QString m_lastError;
    std::function<int(const QString&)> m_danmakuCountProvider;
};

#endif // AUTHSERVICE_H
