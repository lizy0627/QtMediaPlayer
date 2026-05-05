#include "mediahistory.h"

#include <QFileInfo>
#include <QSqlError>

namespace {
QDateTime parseHistoryDateTime(const QVariant& value)
{
    QDateTime dateTime = value.toDateTime();
    if (!dateTime.isValid()) {
        dateTime = QDateTime::fromString(value.toString(), Qt::ISODate);
    }
    if (!dateTime.isValid()) {
        dateTime = QDateTime::fromString(value.toString(), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }
    return dateTime;
}

QString normalizedFileType(const QString& fileType)
{
    return fileType.trimmed().toLower();
}

QString legacySourceTypeForPath(const QString& filePath)
{
    const QString trimmed = filePath.trimmed();
    if (trimmed.startsWith(QStringLiteral("online-audio:"), Qt::CaseInsensitive)) {
        return QStringLiteral("online-audio");
    }
    if (trimmed.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        || trimmed.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        return QStringLiteral("url");
    }
    return QStringLiteral("local-file");
}

QString legacySourceIdForPath(const QString& filePath)
{
    const QString trimmed = filePath.trimmed();
    const QString onlineAudioPrefix = QStringLiteral("online-audio:");
    if (trimmed.startsWith(onlineAudioPrefix, Qt::CaseInsensitive)) {
        return trimmed.mid(onlineAudioPrefix.size());
    }
    return trimmed;
}
}

QString mediaKindToString(MediaKind kind)
{
    switch (kind) {
    case MediaKind::Audio:
        return QStringLiteral("audio");
    case MediaKind::Video:
        return QStringLiteral("video");
    case MediaKind::Unknown:
        break;
    }

    return QStringLiteral("unknown");
}

MediaKind mediaKindFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == mediaKindToString(MediaKind::Audio)) {
        return MediaKind::Audio;
    }
    if (normalized == mediaKindToString(MediaKind::Video)) {
        return MediaKind::Video;
    }

    return MediaKind::Unknown;
}

bool MediaHistoryRecord::isValid() const
{
    return !filePath.isEmpty();
}

int MediaHistoryRecord::progressPercent() const
{
    if (duration <= 0) {
        return 0;
    }

    const qint64 boundedPosition = qBound<qint64>(0, lastPosition, duration);
    return static_cast<int>((boundedPosition * 100) / duration);
}

QString MediaHistoryRecord::formatTime(qint64 ms) const
{
    qint64 seconds = qMax<qint64>(0, ms / 1000);
    const qint64 minutes = seconds / 60;
    const qint64 hours = minutes / 60;

    seconds %= 60;
    const qint64 displayMinutes = minutes % 60;

    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours, 2, 10, QLatin1Char('0'))
            .arg(displayMinutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    }

    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

QString MediaHistoryRecord::positionText() const
{
    return formatTime(lastPosition);
}

QString MediaHistoryRecord::durationText() const
{
    return formatTime(duration);
}

MediaHistoryRepository::MediaHistoryRepository(IDatabaseProvider* provider)
    : m_dbContext(provider)
{
}

bool MediaHistoryRepository::saveRecord(const MediaHistoryRecord& record,
                                        const MediaHistorySaveOptions& options)
{
    if (!record.isValid() || !ensureReady()) {
        return false;
    }

    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    const QFileInfo fileInfo(record.filePath);
    const QString requestedFileType = normalizedFileType(record.fileType);
    const QString resolvedFileName = !record.fileName.trimmed().isEmpty()
        ? record.fileName.trimmed()
        : (!fileInfo.fileName().isEmpty() ? fileInfo.fileName() : record.filePath);
    const QString legacySourceType = legacySourceTypeForPath(record.filePath);
    const QString legacySourceId = legacySourceIdForPath(record.filePath);

    // TODO(db-migration): add source_type/source_id columns, backfill them from
    // legacy file_path values, then enforce a unique key on
    // (file_type, source_type, source_id). For the current schema we first look
    // up the intended file_path+file_type identity, then fall back to file_path
    // only so existing databases and unique indexes keep working unchanged.
    Q_UNUSED(legacySourceType);
    Q_UNUSED(legacySourceId);

    QSqlQuery select(m_dbContext.database());
    QString selectSql = QStringLiteral(
        "SELECT file_path, file_name, file_type, last_play_time, play_count, last_position, duration, is_completed "
        "FROM play_history WHERE file_path = :file_path");
    if (!requestedFileType.isEmpty()) {
        selectSql += QStringLiteral(" AND file_type = :file_type");
    }
    select.prepare(selectSql);
    select.bindValue(QStringLiteral(":file_path"), record.filePath);
    if (!requestedFileType.isEmpty()) {
        select.bindValue(QStringLiteral(":file_type"), requestedFileType);
    }

    if (!select.exec()) {
        setLastError(select.lastError().text());
        return false;
    }

    bool foundExistingRecord = select.next();
    if (!foundExistingRecord) {
        select.finish();
        select.prepare(QStringLiteral(
            "SELECT file_path, file_name, file_type, last_play_time, play_count, last_position, duration, is_completed "
            "FROM play_history WHERE file_path = :file_path"));
        select.bindValue(QStringLiteral(":file_path"), record.filePath);
        if (!select.exec()) {
            setLastError(select.lastError().text());
            return false;
        }
        foundExistingRecord = select.next();
    }

    if (foundExistingRecord) {
        const MediaHistoryRecord existing = readRecord(select);
        const QString fileTypeToStore = !requestedFileType.isEmpty()
            ? requestedFileType
            : normalizedFileType(existing.fileType);
        const QString lastPlayTimeToStore = options.updateLastPlayTime
            ? now
            : (existing.lastPlayTime.isValid() ? existing.lastPlayTime.toString(Qt::ISODate) : now);
        const qint64 durationToStore = record.duration > 0 ? record.duration : existing.duration;
        const bool completedToStore = options.completed.has_value()
            ? *options.completed
            : existing.isCompleted;

        QSqlQuery update(m_dbContext.database());
        update.prepare(QStringLiteral(
            "UPDATE play_history "
            "SET file_name = :file_name, file_type = :file_type, last_play_time = :last_play_time, "
            "play_count = :play_count, last_position = :last_position, duration = :duration, "
            "is_completed = :is_completed "
            "WHERE file_path = :file_path"));
        update.bindValue(QStringLiteral(":file_name"), resolvedFileName);
        update.bindValue(QStringLiteral(":file_type"),
                         fileTypeToStore.isEmpty() ? QStringLiteral("unknown") : fileTypeToStore);
        update.bindValue(QStringLiteral(":last_play_time"), lastPlayTimeToStore);
        update.bindValue(QStringLiteral(":play_count"),
                         existing.playCount + (options.incrementPlayCount ? 1 : 0));
        update.bindValue(QStringLiteral(":last_position"), qMax<qint64>(0, record.lastPosition));
        update.bindValue(QStringLiteral(":duration"), qMax<qint64>(0, durationToStore));
        update.bindValue(QStringLiteral(":is_completed"), completedToStore ? 1 : 0);
        update.bindValue(QStringLiteral(":file_path"), record.filePath);

        if (!update.exec()) {
            setLastError(update.lastError().text());
            return false;
        }
    } else {
        QSqlQuery insert(m_dbContext.database());
        insert.prepare(QStringLiteral(
            "INSERT INTO play_history "
            "(file_path, file_name, file_type, last_play_time, play_count, last_position, duration, is_completed) "
            "VALUES (:file_path, :file_name, :file_type, :last_play_time, :play_count, :last_position, :duration, :is_completed)"));
        insert.bindValue(QStringLiteral(":file_path"), record.filePath);
        insert.bindValue(QStringLiteral(":file_name"), resolvedFileName);
        insert.bindValue(QStringLiteral(":file_type"),
                         requestedFileType.isEmpty() ? QStringLiteral("unknown") : requestedFileType);
        insert.bindValue(QStringLiteral(":last_play_time"), now);
        insert.bindValue(QStringLiteral(":play_count"), options.incrementPlayCount ? 1 : 0);
        insert.bindValue(QStringLiteral(":last_position"), qMax<qint64>(0, record.lastPosition));
        insert.bindValue(QStringLiteral(":duration"), qMax<qint64>(0, record.duration));
        insert.bindValue(QStringLiteral(":is_completed"),
                         options.completed.value_or(record.isCompleted) ? 1 : 0);

        if (!insert.exec()) {
            setLastError(insert.lastError().text());
            return false;
        }
    }

    if (!trimHistory(options.maxHistoryCount)) {
        return false;
    }

    m_lastError.clear();
    return true;
}

QVector<MediaHistoryRecord> MediaHistoryRepository::getRecords(const QString& fileType, int limit) const
{
    const QString normalizedType = normalizedFileType(fileType);
    if (normalizedType.isEmpty()) {
        return fetchRecords(QString(), {}, limit);
    }

    return fetchRecords(QStringLiteral("file_type = ?"), {normalizedType}, limit);
}

std::optional<MediaHistoryRecord> MediaHistoryRepository::getRecord(const QString& filePath,
                                                                    const QString& fileType) const
{
    if (filePath.isEmpty() || !ensureReady()) {
        return std::nullopt;
    }

    QString sql = QStringLiteral(
        "SELECT file_path, file_name, file_type, last_play_time, play_count, last_position, duration, is_completed "
        "FROM play_history WHERE file_path = :file_path");

    const QString normalizedType = normalizedFileType(fileType);
    if (!normalizedType.isEmpty()) {
        sql += QStringLiteral(" AND file_type = :file_type");
    }

    QSqlQuery query(m_dbContext.database());
    query.prepare(sql);
    query.bindValue(QStringLiteral(":file_path"), filePath);
    if (!normalizedType.isEmpty()) {
        query.bindValue(QStringLiteral(":file_type"), normalizedType);
    }

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return std::nullopt;
    }

    if (!query.next()) {
        m_lastError.clear();
        return std::nullopt;
    }

    m_lastError.clear();
    return readRecord(query);
}

bool MediaHistoryRepository::hasRecord(const QString& filePath, const QString& fileType) const
{
    return getRecord(filePath, fileType).has_value();
}

bool MediaHistoryRepository::removeRecord(const QString& filePath, const QString& fileType)
{
    if (filePath.isEmpty() || !ensureReady()) {
        return false;
    }

    QString sql = QStringLiteral("DELETE FROM play_history WHERE file_path = :file_path");
    const QString normalizedType = normalizedFileType(fileType);
    if (!normalizedType.isEmpty()) {
        sql += QStringLiteral(" AND file_type = :file_type");
    }

    QSqlQuery query(m_dbContext.database());
    query.prepare(sql);
    query.bindValue(QStringLiteral(":file_path"), filePath);
    if (!normalizedType.isEmpty()) {
        query.bindValue(QStringLiteral(":file_type"), normalizedType);
    }

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    m_lastError.clear();
    return true;
}

bool MediaHistoryRepository::clear(const QString& fileType)
{
    if (!ensureReady()) {
        return false;
    }

    const QString normalizedType = normalizedFileType(fileType);
    QSqlQuery query(m_dbContext.database());
    if (normalizedType.isEmpty()) {
        if (!query.exec(QStringLiteral("DELETE FROM play_history"))) {
            setLastError(query.lastError().text());
            return false;
        }
    } else {
        query.prepare(QStringLiteral("DELETE FROM play_history WHERE file_type = :file_type"));
        query.bindValue(QStringLiteral(":file_type"), normalizedType);
        if (!query.exec()) {
            setLastError(query.lastError().text());
            return false;
        }
    }

    m_lastError.clear();
    return true;
}

int MediaHistoryRepository::count(const QString& fileType) const
{
    if (!ensureReady()) {
        return 0;
    }

    const QString normalizedType = normalizedFileType(fileType);
    QSqlQuery query(m_dbContext.database());
    if (normalizedType.isEmpty()) {
        if (query.exec(QStringLiteral("SELECT COUNT(*) FROM play_history")) && query.next()) {
            return query.value(0).toInt();
        }
    } else {
        query.prepare(QStringLiteral("SELECT COUNT(*) FROM play_history WHERE file_type = :file_type"));
        query.bindValue(QStringLiteral(":file_type"), normalizedType);
        if (query.exec() && query.next()) {
            return query.value(0).toInt();
        }
    }

    setLastError(query.lastError().text());
    return 0;
}

QString MediaHistoryRepository::lastError() const
{
    return m_lastError;
}

bool MediaHistoryRepository::ensureReady() const
{
    if (!m_dbContext.initialize()) {
        setLastError(m_dbContext.lastError());
        return false;
    }

    return true;
}

QVector<MediaHistoryRecord> MediaHistoryRepository::fetchRecords(const QString& whereClause,
                                                                 const QVariantList& bindValues,
                                                                 int limit) const
{
    QVector<MediaHistoryRecord> records;
    if (!ensureReady()) {
        return records;
    }

    QString sql = QStringLiteral(
        "SELECT file_path, file_name, file_type, last_play_time, play_count, last_position, duration, is_completed "
        "FROM play_history");
    if (!whereClause.isEmpty()) {
        sql += QStringLiteral(" WHERE ") + whereClause;
    }
    sql += QStringLiteral(" ORDER BY last_play_time DESC");
    if (limit > 0) {
        sql += QStringLiteral(" LIMIT ?");
    }

    QSqlQuery query(m_dbContext.database());
    query.prepare(sql);
    for (const QVariant& value : bindValues) {
        query.addBindValue(value);
    }
    if (limit > 0) {
        query.addBindValue(limit);
    }

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return records;
    }

    while (query.next()) {
        records.append(readRecord(query));
    }

    m_lastError.clear();
    return records;
}

MediaHistoryRecord MediaHistoryRepository::readRecord(const QSqlQuery& query) const
{
    MediaHistoryRecord record;
    record.filePath = query.value(0).toString();
    record.fileName = query.value(1).toString();
    record.fileType = normalizedFileType(query.value(2).toString());
    record.lastPlayTime = parseHistoryDateTime(query.value(3));
    record.playCount = query.value(4).toInt();
    record.lastPosition = query.value(5).toLongLong();
    record.duration = query.value(6).toLongLong();
    record.isCompleted = query.value(7).toInt() != 0;
    return record;
}

bool MediaHistoryRepository::trimHistory(int maxHistoryCount)
{
    if (maxHistoryCount <= 0) {
        return true;
    }

    QSqlQuery query(m_dbContext.database());
    query.prepare(QStringLiteral(
        "DELETE FROM play_history "
        "WHERE id NOT IN ("
        "SELECT id FROM ("
        "SELECT id FROM play_history ORDER BY last_play_time DESC LIMIT :limit"
        ") AS kept_history)"));
    query.bindValue(QStringLiteral(":limit"), maxHistoryCount);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    return true;
}

void MediaHistoryRepository::setLastError(const QString& message) const
{
    m_lastError = message;
}

MediaHistoryService::MediaHistoryService(QObject* parent, IDatabaseProvider* provider)
    : QObject(parent)
    , m_repository(provider)
{
}

bool MediaHistoryService::savePlaybackStart(const QString& filePath,
                                           const QString& fileType,
                                           qint64 duration)
{
    MediaHistoryRecord record;
    record.filePath = filePath;
    record.fileType = fileType;
    record.duration = qMax<qint64>(0, duration);

    const auto existing = m_repository.getRecord(filePath, fileType);
    if (existing.has_value()) {
        record.lastPosition = existing->lastPosition;
        if (record.duration <= 0) {
            record.duration = existing->duration;
        }
    }

    MediaHistorySaveOptions options;
    options.incrementPlayCount = true;
    options.maxHistoryCount = m_maxHistoryCount;
    options.completed = false;

    const bool ok = m_repository.saveRecord(record, options);
    if (ok) {
        emit historyUpdated();
    }
    return ok;
}

bool MediaHistoryService::savePlaybackStart(const QString& filePath,
                                            MediaKind kind,
                                            qint64 duration)
{
    return savePlaybackStart(filePath, mediaKindToString(kind), duration);
}

bool MediaHistoryService::savePlaybackStart(const MediaHistoryRecord& inputRecord,
                                            MediaKind kind)
{
    MediaHistoryRecord record = inputRecord;
    record.fileType = mediaKindToString(kind);
    record.duration = qMax<qint64>(0, record.duration);

    if (!record.isValid()) {
        return false;
    }

    const auto existing = m_repository.getRecord(record.filePath, record.fileType);
    if (existing.has_value()) {
        record.lastPosition = existing->lastPosition;
        if (record.duration <= 0) {
            record.duration = existing->duration;
        }
        if (record.fileName.trimmed().isEmpty()) {
            record.fileName = existing->fileName;
        }
    }

    MediaHistorySaveOptions options;
    options.incrementPlayCount = true;
    options.maxHistoryCount = m_maxHistoryCount;
    options.completed = false;

    const bool ok = m_repository.saveRecord(record, options);
    if (ok) {
        emit historyUpdated();
    }
    return ok;
}

bool MediaHistoryService::savePlaybackProgress(const QString& filePath,
                                               const QString& fileType,
                                               qint64 position,
                                               qint64 duration)
{
    MediaHistoryRecord record;
    record.filePath = filePath;
    record.fileType = fileType;
    record.lastPosition = qMax<qint64>(0, position);
    record.duration = qMax<qint64>(0, duration);

    MediaHistorySaveOptions options;
    options.maxHistoryCount = m_maxHistoryCount;
    options.completed = false;

    const bool ok = m_repository.saveRecord(record, options);
    if (ok) {
        emit historyUpdated();
    }
    return ok;
}

bool MediaHistoryService::savePlaybackProgress(const QString& filePath,
                                               MediaKind kind,
                                               qint64 position,
                                               qint64 duration)
{
    return savePlaybackProgress(filePath, mediaKindToString(kind), position, duration);
}

bool MediaHistoryService::savePlaybackCompleted(const QString& filePath,
                                                const QString& fileType,
                                                qint64 position,
                                                qint64 duration)
{
    MediaHistoryRecord record;
    record.filePath = filePath;
    record.fileType = fileType;
    record.duration = qMax<qint64>(0, duration);
    record.lastPosition = record.duration > 0 ? record.duration : qMax<qint64>(0, position);
    record.isCompleted = true;

    MediaHistorySaveOptions options;
    options.maxHistoryCount = m_maxHistoryCount;
    options.completed = true;

    const bool ok = m_repository.saveRecord(record, options);
    if (ok) {
        emit historyUpdated();
    }
    return ok;
}

bool MediaHistoryService::savePlaybackCompleted(const QString& filePath,
                                                MediaKind kind,
                                                qint64 position,
                                                qint64 duration)
{
    return savePlaybackCompleted(filePath, mediaKindToString(kind), position, duration);
}

QVector<MediaHistoryRecord> MediaHistoryService::history(const QString& fileType) const
{
    return m_repository.getRecords(fileType);
}

QVector<MediaHistoryRecord> MediaHistoryService::history(MediaKind kind) const
{
    return history(mediaKindToString(kind));
}

QVector<MediaHistoryRecord> MediaHistoryService::recentHistory(int count,
                                                               const QString& fileType) const
{
    return m_repository.getRecords(fileType, count);
}

QVector<MediaHistoryRecord> MediaHistoryService::recentHistory(int count, MediaKind kind) const
{
    return recentHistory(count, mediaKindToString(kind));
}

std::optional<MediaHistoryRecord> MediaHistoryService::recordFor(const QString& filePath,
                                                                 const QString& fileType) const
{
    return m_repository.getRecord(filePath, fileType);
}

std::optional<MediaHistoryRecord> MediaHistoryService::recordFor(const QString& filePath,
                                                                 MediaKind kind) const
{
    return recordFor(filePath, mediaKindToString(kind));
}

bool MediaHistoryService::hasRecord(const QString& filePath, const QString& fileType) const
{
    return m_repository.hasRecord(filePath, fileType);
}

bool MediaHistoryService::hasRecord(const QString& filePath, MediaKind kind) const
{
    return hasRecord(filePath, mediaKindToString(kind));
}

bool MediaHistoryService::removeRecord(const QString& filePath, const QString& fileType)
{
    const bool ok = m_repository.removeRecord(filePath, fileType);
    if (ok) {
        emit historyUpdated();
    }
    return ok;
}

bool MediaHistoryService::removeRecord(const QString& filePath, MediaKind kind)
{
    return removeRecord(filePath, mediaKindToString(kind));
}

bool MediaHistoryService::clearHistory(const QString& fileType)
{
    const bool ok = m_repository.clear(fileType);
    if (ok) {
        emit historyUpdated();
    }
    return ok;
}

bool MediaHistoryService::clearHistory(MediaKind kind)
{
    return clearHistory(mediaKindToString(kind));
}

int MediaHistoryService::historyCount(const QString& fileType) const
{
    return m_repository.count(fileType);
}

int MediaHistoryService::historyCount(MediaKind kind) const
{
    return historyCount(mediaKindToString(kind));
}

QString MediaHistoryService::lastError() const
{
    return m_repository.lastError();
}
