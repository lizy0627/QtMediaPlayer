#ifndef DATABASECONFIGLOADER_H
#define DATABASECONFIGLOADER_H

#include <QString>

struct DatabaseConfig
{
    QString driverName = QStringLiteral("QMYSQL");
    QString hostName = QStringLiteral("127.0.0.1");
    int port = 3306;
    QString databaseName = QStringLiteral("qtmediaplayer");
    QString userName = QStringLiteral("root");
    QString password = QStringLiteral("123456");
    QString connectOptions = QStringLiteral("MYSQL_OPT_RECONNECT=1");
    bool createDatabase = true;
    QString sourcePath;
};

class DatabaseConfigLoader
{
public:
    static DatabaseConfig load();
    static QString configPath();
    static QString normalizeDriverName(const QString& driverName);
    static bool parseBool(const QString& value, bool fallback);
};

#endif // DATABASECONFIGLOADER_H
