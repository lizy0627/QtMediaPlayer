#ifndef DBMANAGER_H
#define DBMANAGER_H

#include "databasecontext.h"
#include "databaseconfigloader.h"
#include "userrepository.h"

#include <QObject>
#include <QSqlDatabase>
#include <QString>

class DatabaseManager : public QObject, public IDatabaseProvider
{
    Q_OBJECT

public:
    static DatabaseManager& instance();

    bool initialize() override;
    bool initialize(const DatabaseConfig& config);
    bool isInitialized() const override;
    QSqlDatabase database() const override;
    QString connectionName() const;
    QString databasePath() const;
    QString driverName() const;
    QString lastError() const override;

private:
    explicit DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager() override;

    bool openDatabase(const DatabaseConfig& config);
    bool runMigrations();
    void setLastError(const QString& message);

    QSqlDatabase m_db;
    QString m_connectionName;
    QString m_driverName;
    QString m_databasePath;
    QString m_lastError;
    bool m_initialized = false;
};

class UserAccountService : public QObject
{
    Q_OBJECT

public:
    explicit UserAccountService(QObject* parent = nullptr, IDatabaseProvider* provider = nullptr);

    bool ensureReady();
    [[deprecated("Use ensureReady() instead.")]]
    bool connectToDatabase();
    [[deprecated("Use ensureReady() instead; migrations own the users table schema.")]]
    bool createUserTable();
    void seedDevelopmentUsers();
    [[deprecated("Use seedDevelopmentUsers(), gated by QTMEDIAPLAYER_SEED_DEV_USERS=1.")]]
    void insertDefaultUsers();
    bool registerUser(const QString& username,
                      const QString& password,
                      const QString& email = QString());
    bool loginUser(const QString& username, const QString& password);
    bool verifyUserPassword(const QString& username, const QString& password);
    bool userExists(const QString& username);
    void updateLoginInfo(const QString& username);
    int getLoginCount(const QString& username);
    bool changePassword(const QString& username,
                        const QString& oldPassword,
                        const QString& newPassword);
    QString lastError() const;
    bool isConnected() const;

private:
    void syncRepositoryError();

    DatabaseContext m_dbContext;
    UserRepository m_users;
    QString m_lastError;
};

using DBManager [[deprecated("Use UserAccountService instead.")]] = UserAccountService;

#endif // DBMANAGER_H
