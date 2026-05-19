#include "searchcache.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>
#include <QVariant>

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

QString encodedKeyPart(const QString& value)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(value.trimmed()));
}
}

SearchCache::SearchCache(const QString& cacheName, int expireSeconds)
    : m_cacheName(safeCacheName(cacheName))
    , m_connectionName(QStringLiteral("SearchCache_%1_%2")
                           .arg(m_cacheName)
                           .arg(static_cast<qulonglong>(reinterpret_cast<quintptr>(this))))
    , m_expireSeconds(expireSeconds > 0 ? expireSeconds : DefaultExpireSeconds)
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

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS search_cache ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "keyword TEXT UNIQUE NOT NULL,"
            "result_json TEXT NOT NULL,"
            "update_time DATETIME NOT NULL"
            ")"))) {
        qWarning() << "Search cache disabled: failed to create table"
                   << query.lastError().text();
        db.close();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
        return false;
    }

    m_initialized = true;
    clearExpiredCache();
    return true;
}

void SearchCache::setExpireSeconds(int seconds)
{
    if (seconds <= 0) {
        qWarning() << "Search cache ignored invalid TTL" << seconds << "for" << m_cacheName;
        return;
    }

    m_expireSeconds = seconds;
}

int SearchCache::expireSeconds() const
{
    return m_expireSeconds;
}

void SearchCache::setMaxEntries(int maxEntries)
{
    m_maxEntries = maxEntries;
    enforceMaxEntries();
}

int SearchCache::maxEntries() const
{
    return m_maxEntries;
}

QString SearchCache::buildKey(const QString& source,
                              const QString& keyword,
                              const QMap<QString, QString>& parameters)
{
    const QString normalizedSource = source.trimmed().toLower();
    const QString normalizedKeyword = keyword.trimmed();
    if (normalizedSource.isEmpty() || normalizedKeyword.isEmpty()) {
        return QString();
    }

    QStringList parts;
    parts << QStringLiteral("source=%1").arg(encodedKeyPart(normalizedSource));
    parts << QStringLiteral("keyword=%1").arg(encodedKeyPart(normalizedKeyword));

    for (auto it = parameters.cbegin(); it != parameters.cend(); ++it) {
        const QString name = it.key().trimmed();
        if (name.isEmpty()) {
            continue;
        }
        parts << QStringLiteral("%1=%2").arg(encodedKeyPart(name), encodedKeyPart(it.value()));
    }

    return parts.join(QLatin1Char('&'));
}

bool SearchCache::hasValidCache(const QString& key)
{
    const QString normalized = normalizedKey(key);
    if (normalized.isEmpty() || !ensureInitialized()) {
        return false;
    }

    QSqlQuery query(database());
    query.prepare(QStringLiteral("SELECT update_time FROM search_cache WHERE keyword = ?"));
    query.addBindValue(normalized);
    if (!query.exec()) {
        qWarning() << "Search cache lookup failed for" << normalized << ":"
                   << query.lastError().text();
        return false;
    }

    if (!query.next()) {
        qDebug() << "Search cache miss:" << normalized;
        return false;
    }

    const QString updateTime = query.value(0).toString();
    const bool valid = isCacheTimeValid(updateTime);
    if (valid) {
        qDebug() << "Search cache hit:" << normalized;
    } else {
        qDebug() << "Search cache expired:" << normalized << "updated at" << updateTime;
    }
    return valid;
}

QString SearchCache::getCache(const QString& key)
{
    const QString normalized = normalizedKey(key);
    if (normalized.isEmpty() || !ensureInitialized()) {
        return QString();
    }

    QSqlQuery query(database());
    query.prepare(QStringLiteral("SELECT result_json FROM search_cache WHERE keyword = ?"));
    query.addBindValue(normalized);
    if (!query.exec()) {
        qWarning() << "Search cache read failed for" << normalized << ":"
                   << query.lastError().text();
        return QString();
    }

    if (!query.next()) {
        return QString();
    }

    return query.value(0).toString();
}

bool SearchCache::saveCache(const QString& key, const QString& json)
{
    const QString normalized = normalizedKey(key);
    if (normalized.isEmpty() || json.trimmed().isEmpty() || !ensureInitialized()) {
        return false;
    }

    QSqlQuery query(database());
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO search_cache (keyword, result_json, update_time) "
        "VALUES (?, ?, ?)"));
    query.addBindValue(normalized);
    query.addBindValue(json);
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    if (!query.exec()) {
        qWarning() << "Search cache save failed for" << normalized << ":"
                   << query.lastError().text();
        return false;
    }

    qDebug() << "Search cache saved:" << normalized;
    enforceMaxEntries();
    return true;
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
        return;
    }

    if (query.numRowsAffected() > 0) {
        qDebug() << "Search cache cleanup removed expired entries:"
                 << query.numRowsAffected()
                 << "from" << m_cacheName;
    }
    enforceMaxEntries();
}

QString SearchCache::normalizedKey(const QString& key) const
{
    return key.trimmed();
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

void SearchCache::enforceMaxEntries()
{
    if (m_maxEntries <= 0 || !m_initialized) {
        return;
    }

    QSqlQuery query(database());
    query.prepare(QStringLiteral(
        "DELETE FROM search_cache "
        "WHERE id NOT IN ("
        "SELECT id FROM search_cache ORDER BY update_time DESC, id DESC LIMIT ?"
        ")"));
    query.addBindValue(m_maxEntries);
    if (!query.exec()) {
        qWarning() << "Search cache max-entry cleanup failed:" << query.lastError().text();
        return;
    }

    if (query.numRowsAffected() > 0) {
        qDebug() << "Search cache cleanup trimmed old entries:"
                 << query.numRowsAffected()
                 << "from" << m_cacheName;
    }
}
