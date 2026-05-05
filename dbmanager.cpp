#include "dbmanager.h"

#include "migrationrunner.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
bool isMysqlDriver(const QString& driverName)
{
    return DatabaseConfigLoader::normalizeDriverName(driverName) == QStringLiteral("QMYSQL");
}

QString availableDriversText()
{
    return QSqlDatabase::drivers().join(QStringLiteral(", "));
}

QString escapedMysqlIdentifier(QString identifier)
{
    return identifier.replace(QStringLiteral("`"), QStringLiteral("``"));
}

QString databaseIdentity(const DatabaseConfig& config)
{
    return QStringLiteral("%1:%2:%3/%4")
        .arg(config.driverName, config.hostName)
        .arg(config.port)
        .arg(config.databaseName);
}

bool ensureMysqlDatabaseExists(const DatabaseConfig& config, QString* errorMessage)
{
    if (config.databaseName.trimmed().isEmpty()) {
        return true;
    }

    const QString bootstrapConnection = QStringLiteral("qtmediaplayer_mysql_bootstrap_connection");
    if (QSqlDatabase::contains(bootstrapConnection)) {
        QSqlDatabase stale = QSqlDatabase::database(bootstrapConnection);
        if (stale.isOpen()) {
            stale.close();
        }
        stale = QSqlDatabase();
        QSqlDatabase::removeDatabase(bootstrapConnection);
    }

    QSqlDatabase bootstrap = QSqlDatabase::addDatabase(config.driverName, bootstrapConnection);
    bootstrap.setHostName(config.hostName);
    bootstrap.setPort(config.port);
    bootstrap.setUserName(config.userName);
    bootstrap.setPassword(config.password);
    if (!config.connectOptions.trimmed().isEmpty()) {
        bootstrap.setConnectOptions(config.connectOptions);
    }

    if (!bootstrap.open()) {
        if (errorMessage) {
            *errorMessage = bootstrap.lastError().text();
        }
        bootstrap = QSqlDatabase();
        QSqlDatabase::removeDatabase(bootstrapConnection);
        return false;
    }

    QSqlQuery query(bootstrap);
    query.exec(QStringLiteral("SET NAMES utf8mb4"));

    const QString sql = QStringLiteral(
        "CREATE DATABASE IF NOT EXISTS `%1` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci")
        .arg(escapedMysqlIdentifier(config.databaseName));
    const bool ok = query.exec(sql);
    if (!ok && errorMessage) {
        *errorMessage = query.lastError().text();
    }

    bootstrap.close();
    bootstrap = QSqlDatabase();
    QSqlDatabase::removeDatabase(bootstrapConnection);
    return ok;
}

#ifndef QT_NO_DEBUG
bool shouldSeedDevelopmentUsers()
{
    return qEnvironmentVariable("QTMEDIAPLAYER_SEED_DEV_USERS").trimmed() == QStringLiteral("1");
}
#endif
}

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager manager;
    return manager;
}

DatabaseManager::DatabaseManager(QObject* parent)
    : QObject(parent)
    , m_connectionName(QStringLiteral("qtmediaplayer_main_connection"))
{
}

DatabaseManager::~DatabaseManager()
{
    if (m_db.isOpen()) {
        m_db.close();
    }

    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool DatabaseManager::initialize()
{
    return initialize(DatabaseConfigLoader::load());
}

bool DatabaseManager::initialize(const DatabaseConfig& config)
{
    const QString targetIdentity = databaseIdentity(config);
    if (m_initialized && m_driverName == config.driverName && m_databasePath == targetIdentity && m_db.isOpen()) {
        return true;
    }

    m_initialized = false;
    if (!openDatabase(config)) {
        return false;
    }

    QSqlQuery charset(m_db);
    charset.exec(QStringLiteral("SET NAMES utf8mb4"));

    if (!runMigrations()) {
        m_initialized = false;
        return false;
    }

    m_initialized = true;
    m_lastError.clear();
    return true;
}

bool DatabaseManager::isInitialized() const
{
    return m_initialized && m_db.isOpen();
}

QSqlDatabase DatabaseManager::database() const
{
    return QSqlDatabase::database(m_connectionName);
}

QString DatabaseManager::connectionName() const
{
    return m_connectionName;
}

QString DatabaseManager::databasePath() const
{
    return m_databasePath;
}

QString DatabaseManager::driverName() const
{
    return m_driverName;
}

QString DatabaseManager::lastError() const
{
    return m_lastError;
}

bool DatabaseManager::openDatabase(const DatabaseConfig& config)
{
    if (!isMysqlDriver(config.driverName)) {
        setLastError(QStringLiteral("unsupported database driver: %1. This project requires QMYSQL direct connection.")
                         .arg(config.driverName));
        return false;
    }

    if (!QSqlDatabase::isDriverAvailable(config.driverName)) {
        setLastError(QStringLiteral("%1 driver is not available. Available drivers: %2")
                         .arg(config.driverName, availableDriversText()));
        return false;
    }

    if (m_db.isOpen()) {
        m_db.close();
    }

    if (QSqlDatabase::contains(m_connectionName)) {
        m_db = QSqlDatabase::database(m_connectionName);
        if (m_db.isOpen()) {
            m_db.close();
        }
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }

    m_db = QSqlDatabase::addDatabase(config.driverName, m_connectionName);

    if (config.driverName == QStringLiteral("QMYSQL")) {
        m_db.setHostName(config.hostName);
        m_db.setPort(config.port);
        m_db.setUserName(config.userName);
        m_db.setPassword(config.password);
        m_db.setDatabaseName(config.databaseName);
        if (!config.connectOptions.trimmed().isEmpty()) {
            m_db.setConnectOptions(config.connectOptions);
        }
    } else {
        setLastError(QStringLiteral("unsupported database driver: %1").arg(config.driverName));
        return false;
    }

    if (!m_db.open()) {
        const QString firstOpenError = m_db.lastError().text();
        if (config.driverName == QStringLiteral("QMYSQL") && config.createDatabase) {
            QString createError;
            if (ensureMysqlDatabaseExists(config, &createError) && m_db.open()) {
                m_driverName = config.driverName;
                m_databasePath = databaseIdentity(config);
                m_lastError.clear();
                return true;
            }

            setLastError(QStringLiteral("failed to open MySQL database: %1; create database attempt: %2")
                             .arg(firstOpenError, createError));
            return false;
        }

        setLastError(firstOpenError);
        return false;
    }

    m_driverName = config.driverName;
    m_databasePath = databaseIdentity(config);
    return true;
}

bool DatabaseManager::runMigrations()
{
    MigrationRunner runner(m_db);
    if (!runner.runMigrations()) {
        setLastError(runner.lastError());
        return false;
    }

    m_lastError.clear();
    return true;
}

void DatabaseManager::setLastError(const QString& message)
{
    m_lastError = message;
}

UserAccountService::UserAccountService(QObject* parent, IDatabaseProvider* provider)
    : QObject(parent)
    , m_dbContext(provider)
    , m_users(provider)
{
}

bool UserAccountService::ensureReady()
{
    if (!m_dbContext.initialize()) {
        m_lastError = m_dbContext.lastError();
        return false;
    }

    m_lastError.clear();
    return true;
}

bool UserAccountService::connectToDatabase()
{
    return ensureReady();
}

bool UserAccountService::createUserTable()
{
    if (!ensureReady()) {
        return false;
    }

    seedDevelopmentUsers();
    return true;
}

void UserAccountService::seedDevelopmentUsers()
{
#ifndef QT_NO_DEBUG
    if (!shouldSeedDevelopmentUsers()) {
        return;
    }
#else
    return;
#endif

    if (!ensureReady()) {
        return;
    }

    QSqlQuery query(m_dbContext.database());
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM users"))) {
        m_lastError = query.lastError().text();
        return;
    }

    if (query.next() && query.value(0).toInt() > 0) {
        return;
    }

    m_users.registerUser(QStringLiteral("admin"), QStringLiteral("admin123"), QStringLiteral("admin@example.com"));
    m_users.registerUser(QStringLiteral("test"), QStringLiteral("test123"), QStringLiteral("test@example.com"));
    syncRepositoryError();
}

void UserAccountService::insertDefaultUsers()
{
    seedDevelopmentUsers();
}

bool UserAccountService::registerUser(const QString& username,
                                      const QString& password,
                                      const QString& email)
{
    const bool ok = m_users.registerUser(username, password, email);
    syncRepositoryError();
    return ok;
}

bool UserAccountService::loginUser(const QString& username, const QString& password)
{
    const bool ok = m_users.loginUser(username, password);
    syncRepositoryError();
    return ok;
}

bool UserAccountService::verifyUserPassword(const QString& username, const QString& password)
{
    const bool ok = m_users.verifyUserPassword(username, password);
    syncRepositoryError();
    return ok;
}

bool UserAccountService::userExists(const QString& username)
{
    const bool ok = m_users.userExists(username);
    syncRepositoryError();
    return ok;
}

void UserAccountService::updateLoginInfo(const QString& username)
{
    if (!ensureReady()) {
        return;
    }

    QSqlQuery query(m_dbContext.database());
    query.prepare(QStringLiteral(
        "UPDATE users "
        "SET last_login = :last_login, login_count = login_count + 1 "
        "WHERE username = :username"));
    query.bindValue(QStringLiteral(":last_login"), QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(QStringLiteral(":username"), username);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
    } else {
        m_lastError.clear();
    }
}

int UserAccountService::getLoginCount(const QString& username)
{
    const int count = m_users.getLoginCount(username);
    syncRepositoryError();
    return count;
}

bool UserAccountService::changePassword(const QString& username,
                                        const QString& oldPassword,
                                        const QString& newPassword)
{
    const bool ok = m_users.changePassword(username, oldPassword, newPassword);
    syncRepositoryError();
    return ok;
}

QString UserAccountService::lastError() const
{
    return m_lastError;
}

bool UserAccountService::isConnected() const
{
    return m_dbContext.isInitialized();
}

void UserAccountService::syncRepositoryError()
{
    m_lastError = m_users.lastError();
}
