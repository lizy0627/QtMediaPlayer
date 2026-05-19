#ifndef SEARCHCACHE_H
#define SEARCHCACHE_H

#include <QSqlDatabase>
#include <QString>

class SearchCache
{
public:
    explicit SearchCache(const QString& cacheName = QStringLiteral("default"));
    ~SearchCache();

    SearchCache(const SearchCache&) = delete;
    SearchCache& operator=(const SearchCache&) = delete;

    bool init();
    bool hasValidCache(const QString& keyword);
    QString getCache(const QString& keyword);
    void saveCache(const QString& keyword, const QString& json);
    void clearExpiredCache();

private:
    QString normalizedKeyword(const QString& keyword) const;
    QSqlDatabase database() const;
    bool ensureInitialized();
    bool isCacheTimeValid(const QString& updateTime) const;

    QString m_cacheName;
    QString m_connectionName;
    bool m_initialized = false;
    int m_expireSeconds = 24 * 60 * 60;
};

#endif // SEARCHCACHE_H
