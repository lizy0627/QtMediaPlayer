#include "danmakurepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

DanmakuRepository::DanmakuRepository(IDatabaseProvider* provider)
    : m_dbContext(provider)
{
}

bool DanmakuRepository::addDanmaku(const DanmakuItem& danmaku)
{
    if (!ensureReady()) {
        return false;
    }

    const QString mediaId = danmaku.mediaId.trimmed().isEmpty() ? danmaku.videoPath : danmaku.mediaId;
    if (mediaId.trimmed().isEmpty()) {
        setLastError(QStringLiteral("media_id is empty"));
        return false;
    }

    QSqlQuery query(m_dbContext.database());
    query.prepare(QStringLiteral(
        "INSERT INTO danmaku (media_id, video_path, username, content, timestamp, color, font_size, type) "
        "VALUES (:media_id, :video_path, :username, :content, :timestamp, :color, :font_size, :type)"));
    query.bindValue(QStringLiteral(":media_id"), mediaId);
    query.bindValue(QStringLiteral(":video_path"), danmaku.videoPath);
    query.bindValue(QStringLiteral(":username"), danmaku.username);
    query.bindValue(QStringLiteral(":content"), danmaku.content);
    query.bindValue(QStringLiteral(":timestamp"), danmaku.timestamp);
    query.bindValue(QStringLiteral(":color"), danmaku.color);
    query.bindValue(QStringLiteral(":font_size"), danmaku.fontSize);
    query.bindValue(QStringLiteral(":type"), danmaku.type);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    m_lastError.clear();
    return true;
}

QVector<DanmakuItem> DanmakuRepository::getDanmakuList(const QString& mediaId,
                                                       const QString& fallbackVideoPath)
{
    QVector<DanmakuItem> danmakuList;
    if (!ensureReady()) {
        return danmakuList;
    }

    migrateLegacyRows(mediaId, fallbackVideoPath);

    QSqlQuery query(m_dbContext.database());
    query.prepare(QStringLiteral(
        "SELECT id, media_id, video_path, username, content, timestamp, color, font_size, type, create_time "
        "FROM danmaku "
        "WHERE media_id = :media_id "
        "OR ((media_id IS NULL OR media_id = '') AND video_path = :video_path) "
        "ORDER BY timestamp ASC"));
    query.bindValue(QStringLiteral(":media_id"), mediaId);
    query.bindValue(QStringLiteral(":video_path"), fallbackVideoPath);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return danmakuList;
    }

    while (query.next()) {
        danmakuList.append(readItem(query));
    }

    m_lastError.clear();
    return danmakuList;
}

QVector<DanmakuItem> DanmakuRepository::getDanmakuByTimeRange(const QString& mediaId,
                                                              qint64 startTime,
                                                              qint64 endTime,
                                                              const QString& fallbackVideoPath)
{
    QVector<DanmakuItem> danmakuList;
    if (!ensureReady()) {
        return danmakuList;
    }

    migrateLegacyRows(mediaId, fallbackVideoPath);

    QSqlQuery query(m_dbContext.database());
    query.prepare(QStringLiteral(
        "SELECT id, media_id, video_path, username, content, timestamp, color, font_size, type, create_time "
        "FROM danmaku "
        "WHERE (media_id = :media_id "
        "OR ((media_id IS NULL OR media_id = '') AND video_path = :video_path)) "
        "AND timestamp >= :start_time AND timestamp <= :end_time "
        "ORDER BY timestamp ASC"));
    query.bindValue(QStringLiteral(":media_id"), mediaId);
    query.bindValue(QStringLiteral(":video_path"), fallbackVideoPath);
    query.bindValue(QStringLiteral(":start_time"), startTime);
    query.bindValue(QStringLiteral(":end_time"), endTime);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return danmakuList;
    }

    while (query.next()) {
        danmakuList.append(readItem(query));
    }

    m_lastError.clear();
    return danmakuList;
}

bool DanmakuRepository::deleteDanmaku(int danmakuId)
{
    if (!ensureReady()) {
        return false;
    }

    QSqlQuery query(m_dbContext.database());
    query.prepare(QStringLiteral("DELETE FROM danmaku WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), danmakuId);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    m_lastError.clear();
    return true;
}

bool DanmakuRepository::clearDanmaku(const QString& mediaId, const QString& fallbackVideoPath)
{
    if (!ensureReady()) {
        return false;
    }

    migrateLegacyRows(mediaId, fallbackVideoPath);

    QSqlQuery query(m_dbContext.database());
    query.prepare(QStringLiteral(
        "DELETE FROM danmaku "
        "WHERE media_id = :media_id "
        "OR ((media_id IS NULL OR media_id = '') AND video_path = :video_path)"));
    query.bindValue(QStringLiteral(":media_id"), mediaId);
    query.bindValue(QStringLiteral(":video_path"), fallbackVideoPath);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    m_lastError.clear();
    return true;
}

QVector<DanmakuItem> DanmakuRepository::getUserDanmaku(const QString& username)
{
    QVector<DanmakuItem> danmakuList;
    if (!ensureReady()) {
        return danmakuList;
    }

    QSqlQuery query(m_dbContext.database());
    query.prepare(QStringLiteral(
        "SELECT id, media_id, video_path, username, content, timestamp, color, font_size, type, create_time "
        "FROM danmaku WHERE username = :username ORDER BY create_time DESC"));
    query.bindValue(QStringLiteral(":username"), username);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return danmakuList;
    }

    while (query.next()) {
        danmakuList.append(readItem(query));
    }

    m_lastError.clear();
    return danmakuList;
}

int DanmakuRepository::getDanmakuCount(const QString& mediaId, const QString& fallbackVideoPath)
{
    if (!ensureReady()) {
        return 0;
    }

    migrateLegacyRows(mediaId, fallbackVideoPath);

    QSqlQuery query(m_dbContext.database());
    query.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM danmaku "
        "WHERE media_id = :media_id "
        "OR ((media_id IS NULL OR media_id = '') AND video_path = :video_path)"));
    query.bindValue(QStringLiteral(":media_id"), mediaId);
    query.bindValue(QStringLiteral(":video_path"), fallbackVideoPath);

    if (query.exec() && query.next()) {
        m_lastError.clear();
        return query.value(0).toInt();
    }

    setLastError(query.lastError().text());
    return 0;
}

int DanmakuRepository::getUserDanmakuCount(const QString& username)
{
    if (!ensureReady()) {
        return 0;
    }

    QSqlQuery query(m_dbContext.database());
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM danmaku WHERE username = :username"));
    query.bindValue(QStringLiteral(":username"), username);

    if (query.exec() && query.next()) {
        m_lastError.clear();
        return query.value(0).toInt();
    }

    setLastError(query.lastError().text());
    return 0;
}

QString DanmakuRepository::lastError() const
{
    return m_lastError;
}

bool DanmakuRepository::ensureReady()
{
    if (!m_dbContext.initialize()) {
        setLastError(m_dbContext.lastError());
        return false;
    }

    return true;
}

DanmakuItem DanmakuRepository::readItem(const QSqlQuery& query) const
{
    DanmakuItem item;
    item.id = query.value(0).toInt();
    item.mediaId = query.value(1).toString();
    item.videoPath = query.value(2).toString();
    item.username = query.value(3).toString();
    item.content = query.value(4).toString();
    item.timestamp = query.value(5).toLongLong();
    item.color = query.value(6).toString();
    item.fontSize = query.value(7).toInt();
    item.type = query.value(8).toInt();
    item.createTime = query.value(9).toDateTime();
    return item;
}

void DanmakuRepository::migrateLegacyRows(const QString& mediaId, const QString& videoPath)
{
    if (mediaId.trimmed().isEmpty() || videoPath.trimmed().isEmpty()) {
        return;
    }

    QSqlQuery query(m_dbContext.database());
    query.prepare(QStringLiteral(
        "UPDATE danmaku SET media_id = :media_id "
        "WHERE video_path = :video_path AND (media_id IS NULL OR media_id = '')"));
    query.bindValue(QStringLiteral(":media_id"), mediaId);
    query.bindValue(QStringLiteral(":video_path"), videoPath);
    if (!query.exec()) {
        setLastError(query.lastError().text());
    }
}

void DanmakuRepository::setLastError(const QString& message)
{
    m_lastError = message;
}
