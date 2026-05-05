#ifndef MIGRATIONRUNNER_H
#define MIGRATIONRUNNER_H

#include <QSqlDatabase>
#include <QString>

class MigrationRunner
{
public:
    explicit MigrationRunner(const QSqlDatabase& database);

    static int latestVersion();

    bool runMigrations(int targetVersion = latestVersion());
    QString lastError() const;

private:
    bool ensureSchemaVersionTable();
    int currentVersion();
    bool migrateToVersion(int version);
    bool migrateToVersion1();
    bool migrateToVersion2();
    bool migrateToVersion3();
    bool migrateToVersion4();
    bool migrateToVersion5();
    bool writeVersion(int version);
    bool addColumnIfMissing(const QString& tableName,
                            const QString& columnName,
                            const QString& columnDefinition);
    bool columnExists(const QString& tableName, const QString& columnName) const;
    bool tableExists(const QString& tableName) const;
    bool createIndexIfMissing(const QString& tableName,
                              const QString& indexName,
                              const QString& columnsSql);
    bool indexExists(const QString& tableName, const QString& indexName) const;
    bool mergeVideoHistoryIntoPlayHistory();
    bool execStatement(const QString& sql);
    bool isMysql() const;
    void setLastError(const QString& message);

    QSqlDatabase m_db;
    QString m_lastError;
};

#endif // MIGRATIONRUNNER_H
