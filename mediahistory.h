#ifndef MEDIAHISTORY_H
#define MEDIAHISTORY_H

#include <QDateTime>
#include <QObject>
#include <QSqlQuery>
#include <QString>
#include <QVariant>
#include <QVector>

#include <optional>

#include "databasecontext.h"

enum class MediaKind
{
    Audio,
    Video,
    Unknown
};

QString mediaKindToString(MediaKind kind);
MediaKind mediaKindFromString(const QString& value);

struct MediaHistoryRecord
{
    QString filePath;
    QString fileName;
    QString fileType;
    QDateTime lastPlayTime;
    int playCount = 0;
    qint64 lastPosition = 0;
    qint64 duration = 0;
    bool isCompleted = false;

    bool isValid() const;
    int progressPercent() const;
    QString formatTime(qint64 ms) const;
    QString positionText() const;
    QString durationText() const;
};

struct MediaHistorySaveOptions
{
    bool incrementPlayCount = false;
    bool updateLastPlayTime = true;
    int maxHistoryCount = -1;
    std::optional<bool> completed;
};

class MediaHistoryRepository
{
public:
    explicit MediaHistoryRepository(IDatabaseProvider* provider = nullptr);

    bool saveRecord(const MediaHistoryRecord& record,
                    const MediaHistorySaveOptions& options = MediaHistorySaveOptions());
    QVector<MediaHistoryRecord> getRecords(const QString& fileType = QString(),
                                           int limit = -1) const;
    std::optional<MediaHistoryRecord> getRecord(const QString& filePath,
                                                const QString& fileType = QString()) const;
    bool hasRecord(const QString& filePath, const QString& fileType = QString()) const;
    bool removeRecord(const QString& filePath, const QString& fileType = QString());
    bool clear(const QString& fileType = QString());
    int count(const QString& fileType = QString()) const;
    QString lastError() const;

private:
    bool ensureReady() const;
    QVector<MediaHistoryRecord> fetchRecords(const QString& whereClause,
                                             const QVariantList& bindValues,
                                             int limit) const;
    MediaHistoryRecord readRecord(const QSqlQuery& query) const;
    bool trimHistory(int maxHistoryCount);
    void setLastError(const QString& message) const;

    DatabaseContext m_dbContext;
    mutable QString m_lastError;
};

class MediaHistoryService : public QObject
{
    Q_OBJECT

public:
    explicit MediaHistoryService(QObject* parent = nullptr, IDatabaseProvider* provider = nullptr);

    bool savePlaybackStart(const QString& filePath,
                           const QString& fileType,
                           qint64 duration = 0);
    bool savePlaybackStart(const QString& filePath,
                           MediaKind kind,
                           qint64 duration = 0);
    bool savePlaybackStart(const MediaHistoryRecord& record,
                           MediaKind kind);
    bool savePlaybackProgress(const QString& filePath,
                              const QString& fileType,
                              qint64 position,
                              qint64 duration);
    bool savePlaybackProgress(const QString& filePath,
                              MediaKind kind,
                              qint64 position,
                              qint64 duration);
    bool savePlaybackCompleted(const QString& filePath,
                               const QString& fileType,
                               qint64 position,
                               qint64 duration);
    bool savePlaybackCompleted(const QString& filePath,
                               MediaKind kind,
                               qint64 position,
                               qint64 duration);

    QVector<MediaHistoryRecord> history(const QString& fileType = QString()) const;
    QVector<MediaHistoryRecord> history(MediaKind kind) const;
    QVector<MediaHistoryRecord> recentHistory(int count,
                                              const QString& fileType = QString()) const;
    QVector<MediaHistoryRecord> recentHistory(int count,
                                              MediaKind kind) const;
    std::optional<MediaHistoryRecord> recordFor(const QString& filePath,
                                                const QString& fileType = QString()) const;
    std::optional<MediaHistoryRecord> recordFor(const QString& filePath,
                                                MediaKind kind) const;
    bool hasRecord(const QString& filePath, const QString& fileType = QString()) const;
    bool hasRecord(const QString& filePath, MediaKind kind) const;
    bool removeRecord(const QString& filePath, const QString& fileType = QString());
    bool removeRecord(const QString& filePath, MediaKind kind);
    bool clearHistory(const QString& fileType = QString());
    bool clearHistory(MediaKind kind);
    int historyCount(const QString& fileType = QString()) const;
    int historyCount(MediaKind kind) const;
    QString lastError() const;

signals:
    void historyUpdated();

private:
    MediaHistoryRepository m_repository;
    int m_maxHistoryCount = 100;
};

#endif // MEDIAHISTORY_H
