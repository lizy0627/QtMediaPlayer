#ifndef DANMAKUMANAGER_H
#define DANMAKUMANAGER_H

#include <QObject>
#include <QString>
#include <QVector>

#include "databasecontext.h"
#include "danmakurepository.h"

// Legacy compatibility facade for the danmaku database layer.
// New danmaku code should prefer DanmakuController/DanmakuRepository directly.
class DanmakuManager : public QObject
{
    Q_OBJECT

public:
    explicit DanmakuManager(QObject* parent = nullptr, IDatabaseProvider* provider = nullptr);

    bool connectToDatabase();
    bool ensureReady();
    [[deprecated("Use ensureReady() instead.")]]
    bool createDanmakuTable();
    bool addDanmaku(const DanmakuItem& danmaku);
    QVector<DanmakuItem> getDanmakuList(const QString& videoPath);
    QVector<DanmakuItem> getDanmakuByTimeRange(const QString& videoPath,
                                               qint64 startTime,
                                               qint64 endTime);
    bool deleteDanmaku(int danmakuId);
    bool clearDanmaku(const QString& videoPath);
    QVector<DanmakuItem> getUserDanmaku(const QString& username);
    int getDanmakuCount(const QString& videoPath);
    int getUserDanmakuCount(const QString& username);
    QString lastError() const;
    bool isConnected() const;

private:
    void syncRepositoryError();

    DatabaseContext m_dbContext;
    DanmakuRepository m_repository;
    QString m_lastError;
};

#endif // DANMAKUMANAGER_H
