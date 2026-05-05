#ifndef DANMAKUCONTROLLER_H
#define DANMAKUCONTROLLER_H

#include <QObject>
#include <QRect>
#include <QVector>

#include "danmakuitem.h"

class DanmakuOverlay;
class DanmakuPanel;
class DanmakuRepository;

class DanmakuController : public QObject
{
    Q_OBJECT

public:
    explicit DanmakuController(DanmakuRepository* repository,
                               DanmakuOverlay* overlayWidget,
                               DanmakuPanel* listDisplay,
                               QObject* parent = nullptr);

    void setOverlayGeometry(const QRect& geometry);
    void startSync();
    void stopSync();
    void resetSyncPosition(qint64 position = -1);

    void loadVideo(const QString& videoPath);
    void clearVideo();
    bool toggleEnabled(qint64 currentPosition);
    void setEnabled(bool enabled, qint64 currentPosition = 0);
    bool isEnabled() const;

    bool sendDanmaku(const QString& content,
                     const QString& color,
                     int type,
                     const QString& username,
                     qint64 position);
    void addDanmaku(const DanmakuItem& item);
    void clear();
    void syncToTime(int ms);
    void syncToPosition(qint64 currentPosition);
    void syncFromPlaybackPosition(qint64 currentPosition);
    void highlight(qint64 position);

    QVector<DanmakuItem> userDanmaku(const QString& username) const;
    int userDanmakuCount(const QString& username) const;
    QString lastError() const;

signals:
    void enabledChanged(bool enabled);
    void danmakuLoaded(int count);
    void danmakuAdded(DanmakuItem item);
    void danmakuCleared();

private:
    QString mediaIdForVideo(const QString& videoPath) const;
    int findFirstIndexAfter(qint64 timestamp) const;
    int insertDanmakuSorted(const DanmakuItem& item);
    void showOverlayIfNeeded();

    DanmakuRepository* m_repository = nullptr;
    DanmakuOverlay* m_overlayWidget = nullptr;
    DanmakuPanel* m_listDisplay = nullptr;
    QString m_currentMediaId;
    QString m_currentVideoPath;
    QVector<DanmakuItem> m_currentDanmakuList;
    bool m_enabled = true;
    bool m_syncActive = false;
    qint64 m_lastSyncPosition = -1;
    int m_nextDanmakuIndex = 0;
    QString m_lastError;
};

#endif // DANMAKUCONTROLLER_H
