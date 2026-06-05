#ifndef DANMAKUOVERLAY_H
#define DANMAKUOVERLAY_H

#include <QHash>
#include <QSize>
#include <QVector>
#include <QWidget>

#include "danmakuitem.h"

class QTimer;

class DanmakuOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit DanmakuOverlay(QWidget* parent = nullptr);
    ~DanmakuOverlay() override;

    void setEnabled(bool enabled);
    bool isEnabled() const;
    void setSpeed(int speed);
    int speed() const;
    void setPaused(bool paused);
    bool isPaused() const;
    void setMaxDanmakuOnScreen(int max);
    int maxDanmakuOnScreen() const;

public slots:
    void showDanmaku(const DanmakuItem& item);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct RenderItem
    {
        DanmakuItem data;
        int xPos = 0;
        int yPos = 0;
        int speed = 150;
        QSize textSize;
    };

    void updateDanmaku();
    void updateTrackCount();
    int findAvailableTrack();
    void updateTrackOccupation();
    int findSpecialTrack(int type);
    int specialTrackKey(int type, int track) const;
    int specialLineIndex(const RenderItem& item) const;

    QVector<RenderItem> m_activeDanmaku;
    QVector<DanmakuItem> m_danmakuQueue;
    QTimer* m_updateTimer = nullptr;
    bool m_enabled = true;
    bool m_paused = false;
    int m_maxDanmakuOnScreen = 50;
    int m_speed = 150;
    QVector<int> m_trackOccupied;
    int m_trackHeight = 40;
    int m_trackCount = 0;
    QHash<int, int> m_specialLineOccupied;
};

#endif // DANMAKUOVERLAY_H
