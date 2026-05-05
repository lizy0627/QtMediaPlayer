#include "databaseconfigloader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

namespace {
void appendCandidate(QStringList& candidates, const QString& path)
{
    const QString trimmedPath = path.trimmed();
    if (!trimmedPath.isEmpty()) {
        candidates.append(trimmedPath);
    }
}

QString sanitizedConnectOptions(const QString& connectOptions)
{
    QStringList options;
    for (const QString& option : connectOptions.split(QStringLiteral(";"), Qt::SkipEmptyParts)) {
        const QString trimmed = option.trimmed();
        if (!trimmed.startsWith(QStringLiteral("MYSQL_SET_CHARSET_NAME"), Qt::CaseInsensitive)) {
            options.append(trimmed);
        }
    }
    return options.join(QStringLiteral(";"));
}

void applyDefaultRootPassword(DatabaseConfig& config)
{
    if (config.userName == QStringLiteral("root") && config.password.isEmpty()) {
        config.password = QStringLiteral("123456");
    }
}

void applyEnvironmentOverrides(DatabaseConfig& config)
{
    if (qEnvironmentVariableIsSet("QTMEDIAPLAYER_DB_DRIVER")) {
        config.driverName = DatabaseConfigLoader::normalizeDriverName(
            qEnvironmentVariable("QTMEDIAPLAYER_DB_DRIVER"));
    }
    if (qEnvironmentVariableIsSet("QTMEDIAPLAYER_DB_HOST")) {
        config.hostName = qEnvironmentVariable("QTMEDIAPLAYER_DB_HOST");
    }
    if (qEnvironmentVariableIsSet("QTMEDIAPLAYER_DB_PORT")) {
        bool ok = false;
        const int port = qEnvironmentVariable("QTMEDIAPLAYER_DB_PORT").toInt(&ok);
        if (ok) {
            config.port = port;
        }
    }
    if (qEnvironmentVariableIsSet("QTMEDIAPLAYER_DB_NAME")) {
        config.databaseName = qEnvironmentVariable("QTMEDIAPLAYER_DB_NAME");
    }
    if (qEnvironmentVariableIsSet("QTMEDIAPLAYER_DB_USER")) {
        config.userName = qEnvironmentVariable("QTMEDIAPLAYER_DB_USER");
    }
    if (qEnvironmentVariableIsSet("QTMEDIAPLAYER_DB_PASSWORD")) {
        config.password = qEnvironmentVariable("QTMEDIAPLAYER_DB_PASSWORD");
    }
    if (qEnvironmentVariableIsSet("QTMEDIAPLAYER_DB_OPTIONS")) {
        config.connectOptions = qEnvironmentVariable("QTMEDIAPLAYER_DB_OPTIONS");
    }
    if (qEnvironmentVariableIsSet("QTMEDIAPLAYER_DB_CREATE")) {
        config.createDatabase = DatabaseConfigLoader::parseBool(
            qEnvironmentVariable("QTMEDIAPLAYER_DB_CREATE"),
            config.createDatabase);
    }
}
}

QString DatabaseConfigLoader::normalizeDriverName(const QString& driverName)
{
    const QString normalized = driverName.trimmed().toUpper();
    if (normalized == QStringLiteral("MYSQL")) {
        return QStringLiteral("QMYSQL");
    }
    return normalized.isEmpty() ? QStringLiteral("QMYSQL") : normalized;
}

bool DatabaseConfigLoader::parseBool(const QString& value, bool fallback)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("1")
        || normalized == QStringLiteral("true")
        || normalized == QStringLiteral("yes")
        || normalized == QStringLiteral("on")) {
        return true;
    }
    if (normalized == QStringLiteral("0")
        || normalized == QStringLiteral("false")
        || normalized == QStringLiteral("no")
        || normalized == QStringLiteral("off")) {
        return false;
    }
    return fallback;
}

QString DatabaseConfigLoader::configPath()
{
    QStringList candidates;
    appendCandidate(candidates,
                    QDir(QCoreApplication::applicationDirPath())
                        .filePath(QStringLiteral("database.ini")));
    appendCandidate(candidates, qEnvironmentVariable("QTMEDIAPLAYER_DB_CONFIG"));

    const QString appConfigLocation =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (!appConfigLocation.isEmpty()) {
        appendCandidate(candidates, QDir(appConfigLocation).filePath(QStringLiteral("database.ini")));
    }

    const QString genericConfigLocation =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (!genericConfigLocation.isEmpty()) {
        appendCandidate(candidates,
                        QDir(genericConfigLocation)
                            .filePath(QStringLiteral("QtMediaPlayer/database.ini")));
    }

    QStringList checkedPaths;
    for (const QString& candidate : candidates) {
        const QString absolutePath = QFileInfo(candidate).absoluteFilePath();
        if (checkedPaths.contains(absolutePath)) {
            continue;
        }
        checkedPaths.append(absolutePath);
        if (QFileInfo::exists(absolutePath)) {
            return absolutePath;
        }
    }

    return QString();
}

DatabaseConfig DatabaseConfigLoader::load()
{
    DatabaseConfig config;

    const QString path = configPath();
    if (!path.isEmpty()) {
        QSettings settings(path, QSettings::IniFormat);
        settings.beginGroup(QStringLiteral("Database"));
        config.driverName = normalizeDriverName(settings.value(QStringLiteral("driver"),
                                                               config.driverName).toString());
        config.hostName = settings.value(QStringLiteral("host"), config.hostName).toString();
        config.port = settings.value(QStringLiteral("port"), config.port).toInt();
        config.databaseName = settings.value(QStringLiteral("name"), config.databaseName).toString();
        config.userName = settings.value(QStringLiteral("user"), config.userName).toString();
        config.password = settings.value(QStringLiteral("password"), config.password).toString();
        config.connectOptions = settings.value(QStringLiteral("connectOptions"),
                                               config.connectOptions).toString();
        config.createDatabase = settings.value(QStringLiteral("createDatabase"),
                                               config.createDatabase).toBool();
        settings.endGroup();
        config.sourcePath = path;
    }

    applyEnvironmentOverrides(config);
    config.driverName = normalizeDriverName(config.driverName);
    applyDefaultRootPassword(config);
    config.connectOptions = sanitizedConnectOptions(config.connectOptions);
    return config;
}
