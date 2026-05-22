#ifndef DATABASECONFIGLOADER_H
#define DATABASECONFIGLOADER_H

#include <QString>

struct DatabaseConfig
{
    QString driverName = QStringLiteral("QSQLITE");
    QString hostName;
    int port = 0;
    QString databaseName;
    QString userName;
    QString password;
    QString connectOptions;
    bool createDatabase = true;
    QString sourcePath;
};

class DatabaseConfigLoader
{
public:
    static DatabaseConfig load();
    static QString configPath();
    static QString defaultSqliteDatabasePath();
    static QString normalizeDriverName(const QString& driverName);
    static bool parseBool(const QString& value, bool fallback);
};

#endif // DATABASECONFIGLOADER_H
