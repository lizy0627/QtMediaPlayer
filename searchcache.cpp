#include "searchcache.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>
#include <QDebug>

namespace {
QString safeCacheName(QString name)
{
    name = name.trimmed().toLower();
    name.replace(QRegularExpression(QStringLiteral("[^a-z0-9_]+")), QStringLiteral("_"));
    return name.isEmpty() ? QStringLiteral("default") : name;
}

QString cacheRootPath()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (path.trimmed().isEmpty()) {
        path = QCoreApplication::applicationDirPath();
    }
    return path;
}
}

SearchCache::SearchCache(const QString& cacheName)
    : m_cacheName(safeCacheName(cacheName))
    , m_connectionName(QStringLiteral("SearchCache_%1_%2")
                           .arg(m_cacheName)
                           .arg(static_cast<qulonglong>(reinterpret_cast<quintptr>(this))))
{
}

SearchCache::~SearchCache()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName);
        if (db.isOpen()) {
            db.close();
        }
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool SearchCache::init()
{
    if (m_initialized) {
        return true;
    }

    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        qWarning() << "Search cache disabled: QSQLITE driver is not available";
        return false;
    }

    const QString rootPath = cacheRootPath();
    QDir dir(rootPath);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        qWarning() << "Search cache disabled: failed to create cache directory" << rootPath;
        return false;
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(dir.filePath(QStringLiteral("%1_search_cache.sqlite").arg(m_cacheName)));

    if (!db.open()) {
        const QString errorText = db.lastError().text();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
        qWarning() << "Search cache disabled: failed to open sqlite database"
                   << errorText;
        return false;
    }

    QString createTableError;
    {
        QSqlQuery query(db);
        // 搜索缓存表：keyword 为唯一键，result_json 保存接口原始响应。
        if (!query.exec(QStringLiteral(
                "CREATE TABLE IF NOT EXISTS search_cache ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "keyword TEXT UNIQUE NOT NULL,"
                "result_json TEXT NOT NULL,"
                "update_time DATETIME NOT NULL"
                ")"))) {
            createTableError = query.lastError().text();
        }
    }

    if (!createTableError.isEmpty()) {
        qWarning() << "Search cache disabled: failed to create table"
                   << createTableError;
        db.close();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
        return false;
    }

    m_initialized = true;
    clearExpiredCache();
    return true;
}

bool SearchCache::hasValidCache(const QString& keyword)
{
    const QString key = normalizedKeyword(keyword);
    if (key.isEmpty() || !ensureInitialized()) {
        return false;
    }

    QSqlQuery query(database());
    query.prepare(QStringLiteral("SELECT update_time FROM search_cache WHERE keyword = ?"));
    query.addBindValue(key);
    if (!query.exec()) {
        qWarning() << "Search cache lookup failed:" << query.lastError().text();
        return false;
    }

    if (!query.next()) {
        return false;
    }

    return isCacheTimeValid(query.value(0).toString());
}

QString SearchCache::getCache(const QString& keyword)
{
    const QString key = normalizedKeyword(keyword);
    if (key.isEmpty() || !ensureInitialized()) {
        return QString();
    }

    QSqlQuery query(database());
    query.prepare(QStringLiteral("SELECT result_json FROM search_cache WHERE keyword = ?"));
    query.addBindValue(key);
    if (!query.exec()) {
        qWarning() << "Search cache read failed:" << query.lastError().text();
        return QString();
    }

    return query.next() ? query.value(0).toString() : QString();
}

void SearchCache::saveCache(const QString& keyword, const QString& json)
{
    const QString key = normalizedKeyword(keyword);
    if (key.isEmpty() || json.trimmed().isEmpty() || !ensureInitialized()) {
        return;
    }

    QSqlQuery query(database());
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO search_cache (keyword, result_json, update_time) "
        "VALUES (?, ?, ?)"));
    query.addBindValue(key);
    query.addBindValue(json);
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    if (!query.exec()) {
        // 缓存写入失败只记录日志，不影响正常搜索流程。
        qWarning() << "Search cache save failed:" << query.lastError().text();
    }
}

void SearchCache::clearExpiredCache()
{
    if (!ensureInitialized()) {
        return;
    }

    const QString cutoff = QDateTime::currentDateTimeUtc()
                               .addSecs(-m_expireSeconds)
                               .toString(Qt::ISODateWithMs);

    QSqlQuery query(database());
    query.prepare(QStringLiteral("DELETE FROM search_cache WHERE update_time < ?"));
    query.addBindValue(cutoff);
    if (!query.exec()) {
        qWarning() << "Search cache cleanup failed:" << query.lastError().text();
    }
}

QString SearchCache::normalizedKeyword(const QString& keyword) const
{
    return keyword.trimmed();
}

QSqlDatabase SearchCache::database() const
{
    return QSqlDatabase::database(m_connectionName);
}

bool SearchCache::ensureInitialized()
{
    return m_initialized || init();
}

bool SearchCache::isCacheTimeValid(const QString& updateTime) const
{
    QDateTime time = QDateTime::fromString(updateTime, Qt::ISODateWithMs);
    if (!time.isValid()) {
        time = QDateTime::fromString(updateTime, Qt::ISODate);
    }
    if (!time.isValid()) {
        return false;
    }

    time.setTimeSpec(Qt::UTC);
    return time.addSecs(m_expireSeconds) > QDateTime::currentDateTimeUtc();
}
