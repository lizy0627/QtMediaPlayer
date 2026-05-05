#include "migrationrunner.h"

#include <QDateTime>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
QDateTime parseStoredDateTime(const QString& value)
{
    QDateTime dateTime = QDateTime::fromString(value, Qt::ISODate);
    if (!dateTime.isValid()) {
        dateTime = QDateTime::fromString(value, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }
    return dateTime;
}
}

MigrationRunner::MigrationRunner(const QSqlDatabase& database)
    : m_db(database)
{
}

int MigrationRunner::latestVersion()
{
    return 5;
}

bool MigrationRunner::runMigrations(int targetVersion)
{
    if (!m_db.isValid() || !m_db.isOpen()) {
        setLastError(QStringLiteral("database is not open"));
        return false;
    }
    if (!isMysql()) {
        setLastError(QStringLiteral("database driver is not MySQL-compatible: %1").arg(m_db.driverName()));
        return false;
    }

    if (!m_db.transaction()) {
        setLastError(m_db.lastError().text());
        return false;
    }

    if (!ensureSchemaVersionTable()) {
        m_db.rollback();
        return false;
    }

    const int version = currentVersion();
    if (!m_lastError.isEmpty()) {
        m_db.rollback();
        return false;
    }

    for (int nextVersion = version + 1; nextVersion <= targetVersion; ++nextVersion) {
        if (!migrateToVersion(nextVersion) || !writeVersion(nextVersion)) {
            m_db.rollback();
            return false;
        }
    }

    if (!m_db.commit()) {
        setLastError(m_db.lastError().text());
        return false;
    }

    m_lastError.clear();
    return true;
}

QString MigrationRunner::lastError() const
{
    return m_lastError;
}

bool MigrationRunner::ensureSchemaVersionTable()
{
    return execStatement(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS schema_version ("
        "id INTEGER PRIMARY KEY, "
        "version INTEGER NOT NULL, "
        "updated_at VARCHAR(32) NOT NULL)"));
}

int MigrationRunner::currentVersion()
{
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT version FROM schema_version WHERE id = 1"))) {
        setLastError(query.lastError().text());
        return 0;
    }

    if (!query.next()) {
        m_lastError.clear();
        return 0;
    }

    m_lastError.clear();
    return query.value(0).toInt();
}

bool MigrationRunner::migrateToVersion(int version)
{
    switch (version) {
    case 1:
        return migrateToVersion1();
    case 2:
        return migrateToVersion2();
    case 3:
        return migrateToVersion3();
    case 4:
        return migrateToVersion4();
    case 5:
        return migrateToVersion5();
    default:
        setLastError(QStringLiteral("unsupported schema version: %1").arg(version));
        return false;
    }
}

bool MigrationRunner::migrateToVersion1()
{
    return execStatement(QStringLiteral(
               "CREATE TABLE IF NOT EXISTS users ("
               "id INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY, "
               "username VARCHAR(191) UNIQUE NOT NULL, "
               "password VARCHAR(128) NOT NULL, "
               "email VARCHAR(255), "
               "created_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
               "last_login VARCHAR(32), "
               "login_count INTEGER DEFAULT 0)"))
        && execStatement(QStringLiteral(
               "CREATE TABLE IF NOT EXISTS danmaku ("
               "id INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY, "
               "media_id VARCHAR(128) NOT NULL DEFAULT '', "
               "video_path VARCHAR(768) NOT NULL, "
               "username VARCHAR(191) NOT NULL, "
               "content TEXT NOT NULL, "
               "timestamp BIGINT NOT NULL, "
               "color VARCHAR(32) DEFAULT '#FFFFFF', "
               "font_size INTEGER DEFAULT 25, "
               "type INTEGER DEFAULT 0, "
               "create_time DATETIME DEFAULT CURRENT_TIMESTAMP)"))
        && execStatement(QStringLiteral(
               "CREATE TABLE IF NOT EXISTS play_history ("
               "id INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY, "
               "file_path VARCHAR(768) UNIQUE NOT NULL, "
               "file_name VARCHAR(512) NOT NULL, "
               "file_type VARCHAR(32) NOT NULL, "
               "last_play_time VARCHAR(32) NOT NULL, "
               "play_count INTEGER DEFAULT 0, "
               "last_position BIGINT DEFAULT 0, "
               "duration BIGINT DEFAULT 0, "
               "is_completed TINYINT DEFAULT 0)"))
        && execStatement(QStringLiteral(
               "CREATE TABLE IF NOT EXISTS video_history ("
               "id INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY, "
               "file_path VARCHAR(768) UNIQUE NOT NULL, "
               "file_name VARCHAR(512) NOT NULL, "
               "position BIGINT DEFAULT 0, "
               "duration BIGINT DEFAULT 0, "
               "last_play_time VARCHAR(32) NOT NULL, "
               "play_count INTEGER DEFAULT 0)"));
}

bool MigrationRunner::migrateToVersion2()
{
    return addColumnIfMissing(QStringLiteral("users"),
                              QStringLiteral("salt"),
                              QStringLiteral("salt VARCHAR(128)"))
        && addColumnIfMissing(QStringLiteral("users"),
                              QStringLiteral("password_algorithm"),
                              QStringLiteral("password_algorithm VARCHAR(32) DEFAULT 'plain'"))
        && createIndexIfMissing(QStringLiteral("play_history"),
                                QStringLiteral("idx_play_history_last_time"),
                                QStringLiteral("last_play_time"))
        && createIndexIfMissing(QStringLiteral("play_history"),
                                QStringLiteral("idx_play_history_type"),
                                QStringLiteral("file_type"));
}

bool MigrationRunner::migrateToVersion3()
{
    if (!createIndexIfMissing(QStringLiteral("danmaku"),
                              QStringLiteral("idx_danmaku_video_timestamp"),
                              QStringLiteral("video_path(255), timestamp"))
        || !createIndexIfMissing(QStringLiteral("danmaku"),
                                 QStringLiteral("idx_danmaku_username"),
                                 QStringLiteral("username"))
        || !mergeVideoHistoryIntoPlayHistory()) {
        return false;
    }

    return execStatement(QStringLiteral("DROP TABLE IF EXISTS video_history"));
}

bool MigrationRunner::migrateToVersion4()
{
    return addColumnIfMissing(QStringLiteral("play_history"),
                              QStringLiteral("is_completed"),
                              QStringLiteral("is_completed TINYINT DEFAULT 0"))
        && execStatement(QStringLiteral(
               "UPDATE play_history "
               "SET is_completed = 1 "
               "WHERE duration > 0 AND last_position >= duration"));
}

bool MigrationRunner::migrateToVersion5()
{
    return addColumnIfMissing(QStringLiteral("danmaku"),
                              QStringLiteral("media_id"),
                              QStringLiteral("media_id VARCHAR(128) NOT NULL DEFAULT ''"))
        && createIndexIfMissing(QStringLiteral("danmaku"),
                                QStringLiteral("idx_danmaku_media_timestamp"),
                                QStringLiteral("media_id, timestamp"));
}

bool MigrationRunner::writeVersion(int version)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO schema_version (id, version, updated_at) "
        "VALUES (1, :version, :updated_at) "
        "ON DUPLICATE KEY UPDATE version = VALUES(version), updated_at = VALUES(updated_at)"));
    query.bindValue(QStringLiteral(":version"), version);
    query.bindValue(QStringLiteral(":updated_at"), QDateTime::currentDateTime().toString(Qt::ISODate));

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    m_lastError.clear();
    return true;
}

bool MigrationRunner::addColumnIfMissing(const QString& tableName,
                                         const QString& columnName,
                                         const QString& columnDefinition)
{
    if (columnExists(tableName, columnName)) {
        return true;
    }

    return execStatement(QStringLiteral("ALTER TABLE %1 ADD COLUMN %2").arg(tableName, columnDefinition));
}

bool MigrationRunner::columnExists(const QString& tableName, const QString& columnName) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT 1 FROM information_schema.columns "
        "WHERE table_schema = DATABASE() AND table_name = :table_name AND column_name = :column_name "
        "LIMIT 1"));
    query.bindValue(QStringLiteral(":table_name"), tableName);
    query.bindValue(QStringLiteral(":column_name"), columnName);
    return query.exec() && query.next();
}

bool MigrationRunner::tableExists(const QString& tableName) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT 1 FROM information_schema.tables "
        "WHERE table_schema = DATABASE() AND table_name = :table_name "
        "LIMIT 1"));
    query.bindValue(QStringLiteral(":table_name"), tableName);

    return query.exec() && query.next();
}

bool MigrationRunner::createIndexIfMissing(const QString& tableName,
                                           const QString& indexName,
                                           const QString& columnsSql)
{
    if (indexExists(tableName, indexName)) {
        return true;
    }

    return execStatement(QStringLiteral("CREATE INDEX %1 ON %2(%3)")
                             .arg(indexName, tableName, columnsSql));
}

bool MigrationRunner::indexExists(const QString& tableName, const QString& indexName) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT 1 FROM information_schema.statistics "
        "WHERE table_schema = DATABASE() AND table_name = :table_name AND index_name = :index_name "
        "LIMIT 1"));
    query.bindValue(QStringLiteral(":table_name"), tableName);
    query.bindValue(QStringLiteral(":index_name"), indexName);
    return query.exec() && query.next();
}

bool MigrationRunner::mergeVideoHistoryIntoPlayHistory()
{
    if (!tableExists(QStringLiteral("video_history"))) {
        return true;
    }

    QSqlQuery select(m_db);
    if (!select.exec(QStringLiteral(
            "SELECT file_path, file_name, position, duration, last_play_time, play_count "
            "FROM video_history ORDER BY last_play_time DESC"))) {
        setLastError(select.lastError().text());
        return false;
    }

    while (select.next()) {
        const QString filePath = select.value(0).toString();
        const QString fileName = select.value(1).toString();
        const qint64 position = select.value(2).toLongLong();
        const qint64 duration = select.value(3).toLongLong();
        const QString lastPlayTime = select.value(4).toString();
        const int playCount = select.value(5).toInt();

        QSqlQuery existing(m_db);
        existing.prepare(QStringLiteral(
            "SELECT file_name, last_play_time, play_count, last_position, duration, 0 "
            "FROM play_history WHERE file_path = :file_path"));
        existing.bindValue(QStringLiteral(":file_path"), filePath);

        if (!existing.exec()) {
            setLastError(existing.lastError().text());
            return false;
        }

        if (existing.next()) {
            const QString existingName = existing.value(0).toString();
            const QString existingLastPlayTime = existing.value(1).toString();
            const int existingPlayCount = existing.value(2).toInt();
            const qint64 existingPosition = existing.value(3).toLongLong();
            const qint64 existingDuration = existing.value(4).toLongLong();
            const bool existingCompleted = existing.value(5).toInt() != 0;
            const bool migratedCompleted = duration > 0 && position >= duration;

            const QDateTime migratedTime = parseStoredDateTime(lastPlayTime);
            const QDateTime storedTime = parseStoredDateTime(existingLastPlayTime);
            const bool preferMigrated = !storedTime.isValid()
                || (migratedTime.isValid() && migratedTime >= storedTime);

            QSqlQuery update(m_db);
            update.prepare(QStringLiteral(
                "UPDATE play_history "
                "SET file_name = :file_name, file_type = 'video', last_play_time = :last_play_time, "
                "play_count = :play_count, last_position = :last_position, duration = :duration, "
                "is_completed = :is_completed "
                "WHERE file_path = :file_path"));
            update.bindValue(QStringLiteral(":file_name"),
                             fileName.isEmpty() ? existingName : fileName);
            update.bindValue(QStringLiteral(":last_play_time"),
                             preferMigrated && !lastPlayTime.isEmpty() ? lastPlayTime : existingLastPlayTime);
            update.bindValue(QStringLiteral(":play_count"), qMax(existingPlayCount, playCount));
            update.bindValue(QStringLiteral(":last_position"),
                             preferMigrated ? position : existingPosition);
            update.bindValue(QStringLiteral(":duration"),
                             preferMigrated ? duration : existingDuration);
            update.bindValue(QStringLiteral(":is_completed"),
                             (preferMigrated ? migratedCompleted : existingCompleted) ? 1 : 0);
            update.bindValue(QStringLiteral(":file_path"), filePath);

            if (!update.exec()) {
                setLastError(update.lastError().text());
                return false;
            }
            continue;
        }

        QSqlQuery insert(m_db);
        insert.prepare(QStringLiteral(
            "INSERT INTO play_history "
            "(file_path, file_name, file_type, last_play_time, play_count, last_position, duration, is_completed) "
            "VALUES (:file_path, :file_name, 'video', :last_play_time, :play_count, :last_position, :duration, :is_completed)"));
        insert.bindValue(QStringLiteral(":file_path"), filePath);
        insert.bindValue(QStringLiteral(":file_name"),
                         fileName.isEmpty() ? QFileInfo(filePath).fileName() : fileName);
        insert.bindValue(QStringLiteral(":last_play_time"),
                         lastPlayTime.isEmpty()
                             ? QDateTime::currentDateTime().toString(Qt::ISODate)
                             : lastPlayTime);
        insert.bindValue(QStringLiteral(":play_count"), playCount);
        insert.bindValue(QStringLiteral(":last_position"), position);
        insert.bindValue(QStringLiteral(":duration"), duration);
        insert.bindValue(QStringLiteral(":is_completed"), (duration > 0 && position >= duration) ? 1 : 0);

        if (!insert.exec()) {
            setLastError(insert.lastError().text());
            return false;
        }
    }

    m_lastError.clear();
    return true;
}

bool MigrationRunner::execStatement(const QString& sql)
{
    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        setLastError(query.lastError().text());
        return false;
    }

    m_lastError.clear();
    return true;
}

void MigrationRunner::setLastError(const QString& message)
{
    m_lastError = message;
}

bool MigrationRunner::isMysql() const
{
    const QString driverName = m_db.driverName().toUpper();
    return driverName == QStringLiteral("QMYSQL");
}
