#ifndef DANMAKUREPOSITORY_H
#define DANMAKUREPOSITORY_H

#include <QString>
#include <QVector>

#include "databasecontext.h"
#include "danmakuitem.h"

class QSqlQuery;

class DanmakuRepository
{
public:
    explicit DanmakuRepository(IDatabaseProvider* provider = nullptr);

    bool ensureReady();
    bool addDanmaku(const DanmakuItem& danmaku);
    QVector<DanmakuItem> getDanmakuList(const QString& mediaId,
                                        const QString& fallbackVideoPath = QString());
    QVector<DanmakuItem> getDanmakuByTimeRange(const QString& mediaId,
                                               qint64 startTime,
                                               qint64 endTime,
                                               const QString& fallbackVideoPath = QString());
    bool deleteDanmaku(int danmakuId);
    bool clearDanmaku(const QString& mediaId, const QString& fallbackVideoPath = QString());
    QVector<DanmakuItem> getUserDanmaku(const QString& username);
    int getDanmakuCount(const QString& mediaId, const QString& fallbackVideoPath = QString());
    int getUserDanmakuCount(const QString& username);
    QString lastError() const;

private:
    void migrateLegacyRows(const QString& mediaId, const QString& videoPath);
    DanmakuItem readItem(const QSqlQuery& query) const;
    void setLastError(const QString& message);

    DatabaseContext m_dbContext;
    QString m_lastError;
};

#endif // DANMAKUREPOSITORY_H
