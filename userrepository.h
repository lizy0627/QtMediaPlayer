#ifndef USERREPOSITORY_H
#define USERREPOSITORY_H

#include <QString>

#include "databasecontext.h"

class UserRepository
{
public:
    explicit UserRepository(IDatabaseProvider* provider = nullptr);

    bool registerUser(const QString& username,
                      const QString& password,
                      const QString& email = QString());
    bool loginUser(const QString& username, const QString& password);
    bool verifyUserPassword(const QString& username, const QString& password);
    bool userExists(const QString& username);
    int getLoginCount(const QString& username);
    bool changePassword(const QString& username,
                        const QString& oldPassword,
                        const QString& newPassword);
    QString lastError() const;

private:
    bool ensureReady();
    void updateLoginInfo(const QString& username);
    void setLastError(const QString& message);
    bool verifyStoredPassword(const QString& username, const QString& password);
    bool updatePassword(const QString& username, const QString& password);

    DatabaseContext m_dbContext;
    QString m_lastError;
};

#endif // USERREPOSITORY_H
