#include "danmakumanager.h"

// Legacy compatibility facade kept for older danmaku callers.
// New code should prefer DanmakuController/DanmakuRepository boundaries.
DanmakuManager::DanmakuManager(QObject* parent, IDatabaseProvider* provider)
    : QObject(parent)
    , m_dbContext(provider)
    , m_repository(provider)
{
}

bool DanmakuManager::connectToDatabase()
{
    if (!m_dbContext.initialize()) {
        m_lastError = m_dbContext.lastError();
        return false;
    }

    m_lastError.clear();
    return true;
}

bool DanmakuManager::ensureReady()
{
    return connectToDatabase();
}

bool DanmakuManager::createDanmakuTable()
{
    return ensureReady();
}

bool DanmakuManager::addDanmaku(const DanmakuItem& danmaku)
{
    const bool ok = m_repository.addDanmaku(danmaku);
    syncRepositoryError();
    return ok;
}

QVector<DanmakuItem> DanmakuManager::getDanmakuList(const QString& videoPath)
{
    QVector<DanmakuItem> items = m_repository.getDanmakuList(videoPath);
    syncRepositoryError();
    return items;
}

QVector<DanmakuItem> DanmakuManager::getDanmakuByTimeRange(const QString& videoPath,
                                                           qint64 startTime,
                                                           qint64 endTime)
{
    QVector<DanmakuItem> items = m_repository.getDanmakuByTimeRange(videoPath, startTime, endTime);
    syncRepositoryError();
    return items;
}

bool DanmakuManager::deleteDanmaku(int danmakuId)
{
    const bool ok = m_repository.deleteDanmaku(danmakuId);
    syncRepositoryError();
    return ok;
}

bool DanmakuManager::clearDanmaku(const QString& videoPath)
{
    const bool ok = m_repository.clearDanmaku(videoPath);
    syncRepositoryError();
    return ok;
}

QVector<DanmakuItem> DanmakuManager::getUserDanmaku(const QString& username)
{
    QVector<DanmakuItem> items = m_repository.getUserDanmaku(username);
    syncRepositoryError();
    return items;
}

int DanmakuManager::getDanmakuCount(const QString& videoPath)
{
    const int count = m_repository.getDanmakuCount(videoPath);
    syncRepositoryError();
    return count;
}

int DanmakuManager::getUserDanmakuCount(const QString& username)
{
    const int count = m_repository.getUserDanmakuCount(username);
    syncRepositoryError();
    return count;
}

QString DanmakuManager::lastError() const
{
    return m_lastError;
}

bool DanmakuManager::isConnected() const
{
    return m_dbContext.isInitialized();
}

void DanmakuManager::syncRepositoryError()
{
    m_lastError = m_repository.lastError();
}
