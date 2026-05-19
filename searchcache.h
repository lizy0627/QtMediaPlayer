#ifndef SEARCHCACHE_H
#define SEARCHCACHE_H

#include <QMap>
#include <QSqlDatabase>
#include <QString>

class SearchCache
{
public:
    static constexpr int DefaultExpireSeconds = 24 * 60 * 60;

    explicit SearchCache(const QString& cacheName = QStringLiteral("default"),
                         int expireSeconds = DefaultExpireSeconds);
    ~SearchCache();

    SearchCache(const SearchCache&) = delete;
    SearchCache& operator=(const SearchCache&) = delete;

    bool init();
    void setExpireSeconds(int seconds);
    int expireSeconds() const;
    void setMaxEntries(int maxEntries);
    int maxEntries() const;

    static QString buildKey(const QString& source,
                            const QString& keyword,
                            const QMap<QString, QString>& parameters = {});

    bool hasValidCache(const QString& key);
    QString getCache(const QString& key);
    bool saveCache(const QString& key, const QString& json);
    void clearExpiredCache();

private:
    QString normalizedKey(const QString& key) const;
    QSqlDatabase database() const;
    bool ensureInitialized();
    bool isCacheTimeValid(const QString& updateTime) const;
    void enforceMaxEntries();

    QString m_cacheName;
    QString m_connectionName;
    bool m_initialized = false;
    int m_expireSeconds = DefaultExpireSeconds;
    int m_maxEntries = 500;
};

#endif // SEARCHCACHE_H
