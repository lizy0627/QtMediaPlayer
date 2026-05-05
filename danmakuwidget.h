#ifndef DANMAKUWIDGET_H
#define DANMAKUWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QVector>
#include <QRandomGenerator>
#include <QPainter>
#include <QFont>
#include <QPainterPath>
#include <QHash>
#include <limits>
#include "danmakumanager.h"

// Legacy danmaku overlay implementation.
// Kept for compatibility only; new code should use danmaku/DanmakuOverlay.
// 单条弹幕数据（用于绘制）
struct DanmakuRenderItem
{
    DanmakuItem data;
    int xPos;
    int yPos;
    int speed;
    QSize textSize;
    
    DanmakuRenderItem() : xPos(0), yPos(0), speed(150) {}
};

// Legacy danmaku drawing widget.
// Kept for compatibility only; new code should use danmaku/DanmakuOverlay.
// 弹幕显示组件 - 使用绘制方式
class DanmakuWidget : public QWidget
{
    Q_OBJECT

private:
    QVector<DanmakuRenderItem> m_activeDanmaku;  // 活动的弹幕
    QVector<DanmakuItem> m_danmakuQueue;         // 弹幕队列
    QTimer* m_updateTimer;                        // 更新定时器
    bool m_enabled;                               // 是否启用弹幕
    int m_maxDanmakuOnScreen;                     // 屏幕上最大弹幕数
    double m_opacity;                             // 弹幕透明度
    int m_speed;                                  // 弹幕速度（像素/秒）
    QVector<int> m_trackOccupied;                 // 轨道占用情况
    int m_trackHeight;                            // 每条轨道的高度
    int m_trackCount;                             // 轨道数量
    QFont m_font;                                 // 弹幕字体
    QHash<int, int> m_specialLineOccupied;        // 顶部/底部轨道占用

public:
    explicit DanmakuWidget(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_enabled(true)
        , m_maxDanmakuOnScreen(50)
        , m_opacity(1.0)
        , m_speed(150)
        , m_trackHeight(40)
        , m_trackCount(0)
    {
        // 设置半透明背景用于测试（可以看到弹幕组件的位置）
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setStyleSheet("background: transparent;");
        
        // 确保组件可见
        setVisible(true);
        
        // 设置字体
        m_font = QFont("Microsoft YaHei", 12, QFont::Bold);
        
        // 创建更新定时器
        m_updateTimer = new QTimer(this);
        m_updateTimer->setInterval(30);  // 约33fps
        connect(m_updateTimer, &QTimer::timeout, this, &DanmakuWidget::updateDanmaku);
        m_updateTimer->start();
    }

    ~DanmakuWidget()
    {
        clearAllDanmaku();
    }

    // 添加弹幕到队列
    void addDanmaku(const DanmakuItem &danmaku)
    {
        if (!m_enabled) {
            return;
        }
        
        m_danmakuQueue.append(danmaku);
    }

    // 批量添加弹幕
    void addDanmakuList(const QVector<DanmakuItem> &danmakuList)
    {
        if (!m_enabled) {
            return;
        }
        
        m_danmakuQueue.append(danmakuList);
    }

    // 立即显示弹幕
    void showDanmaku(const DanmakuItem &danmaku)
    {
        if (!m_enabled || m_activeDanmaku.size() >= m_maxDanmakuOnScreen) {
            return;
        }

        // 创建渲染项
        DanmakuRenderItem item;
        item.data = danmaku;
        item.speed = m_speed;
        
        // 计算文字大小
        QFont font("Microsoft YaHei", danmaku.fontSize, QFont::Bold);
        QFontMetrics fm(font);
        QString text = QString("%1: %2").arg(danmaku.username).arg(danmaku.content);
        item.textSize = fm.size(Qt::TextSingleLine, text);
        
        // 计算轨道
        updateTrackCount();
        int track = 0;

        if (danmaku.type == 0) {
            track = findAvailableTrack();
            if (track < 0) {
                return;
            }
            item.xPos = width();
            item.yPos = track * m_trackHeight + 10;
        } else {
            track = findSpecialTrack(danmaku.type);
            item.speed = 0;
            item.xPos = qMax(20, (width() - item.textSize.width()) / 2);
            if (danmaku.type == 1) {
                item.yPos = track * m_trackHeight + 10;
            } else {
                item.yPos = height() - (track + 1) * m_trackHeight - 20;
            }
            m_specialLineOccupied.insert(specialTrackKey(danmaku.type, track), 80);
        }
        
        // 添加到活动列表
        m_activeDanmaku.append(item);
        
        // 标记轨道被占用
        if (danmaku.type == 0 && track < m_trackOccupied.size()) {
            m_trackOccupied[track] = item.xPos + item.textSize.width();
        }
        
        // 触发重绘
        update();
    }

    // 更新弹幕位置
    void updateDanmaku()
    {
        if (!m_enabled) {
            return;
        }

        // 从队列中取出弹幕显示
        while (!m_danmakuQueue.isEmpty() && m_activeDanmaku.size() < m_maxDanmakuOnScreen) {
            DanmakuItem item = m_danmakuQueue.takeFirst();
            showDanmaku(item);
        }

        // 更新所有活动弹幕的位置
        bool needUpdate = false;
        QVector<int> toRemove;
        QVector<int> specialKeys = m_specialLineOccupied.keys().toVector();
        for (int key : specialKeys) {
            int remain = m_specialLineOccupied.value(key) - 1;
            if (remain <= 0) {
                m_specialLineOccupied.remove(key);
            } else {
                m_specialLineOccupied[key] = remain;
            }
        }
        
        for (int i = 0; i < m_activeDanmaku.size(); ++i) {
            DanmakuRenderItem &item = m_activeDanmaku[i];
            
            if (item.data.type == 0) {
                // 计算新位置
                int moveDistance = item.speed * m_updateTimer->interval() / 1000;
                item.xPos -= moveDistance;
            }
            
            needUpdate = true;
            
            // 如果弹幕完全移出屏幕，标记为删除
            if ((item.data.type == 0 && item.xPos + item.textSize.width() < 0)
                || (item.data.type != 0 && !m_specialLineOccupied.contains(specialTrackKey(item.data.type, specialLineIndex(item))))) {
                toRemove.append(i);
            }
        }

        // 删除已移出屏幕的弹幕（从后往前删除）
        for (int i = toRemove.size() - 1; i >= 0; --i) {
            m_activeDanmaku.removeAt(toRemove[i]);
        }
        
        // 更新轨道占用情况
        updateTrackOccupation();
        
        // 触发重绘
        if (needUpdate) {
            update();
        }
    }

    // 清空所有弹幕
    void clearAllDanmaku()
    {
        m_danmakuQueue.clear();
        m_activeDanmaku.clear();
        m_trackOccupied.clear();
        m_specialLineOccupied.clear();
        update();
    }

    // 设置是否启用弹幕
    void setEnabled(bool enabled)
    {
        m_enabled = enabled;
        if (!enabled) {
            clearAllDanmaku();
        }
    }

    bool isEnabled() const { return m_enabled; }

    // 设置弹幕速度
    void setSpeed(int speed)
    {
        m_speed = qBound(50, speed, 300);
    }

    int speed() const { return m_speed; }

    // 设置最大弹幕数
    void setMaxDanmakuOnScreen(int max)
    {
        m_maxDanmakuOnScreen = qBound(10, max, 100);
    }

    int maxDanmakuOnScreen() const { return m_maxDanmakuOnScreen; }

protected:
    // 绘制弹幕
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        
        if (!m_enabled || m_activeDanmaku.isEmpty()) {
            return;
        }
        
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        
        // 绘制每条弹幕
        for (const DanmakuRenderItem &item : m_activeDanmaku) {
            QString text = QString("%1: %2").arg(item.data.username).arg(item.data.content);
            
            // 设置字体
            QFont font("Microsoft YaHei", item.data.fontSize, QFont::Bold);
            painter.setFont(font);
            
            // 绘制文字描边（黑色）
            QPainterPath path;
            path.addText(item.xPos, item.yPos + item.data.fontSize, font, text);
            
            painter.strokePath(path, QPen(Qt::black, 3));
            painter.fillPath(path, QBrush(QColor(item.data.color)));
        }
    }

    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        updateTrackCount();
    }

private:
    // 更新轨道数量
    void updateTrackCount()
    {
        int newTrackCount = (height() - 100) / m_trackHeight;
        if (newTrackCount != m_trackCount) {
            m_trackCount = qMax(1, newTrackCount);
            m_trackOccupied.resize(m_trackCount);
            m_trackOccupied.fill(0);
        }
    }

    // 查找可用轨道
    int findAvailableTrack()
    {
        if (m_trackOccupied.isEmpty()) {
            updateTrackCount();
        }

        // 查找完全空闲的轨道
        for (int i = 0; i < m_trackOccupied.size(); ++i) {
            if (m_trackOccupied[i] <= width() - 200) {
                return i;
            }
        }

        // 没有完全空闲的轨道时，选择右边界最靠左的轨道，减少新弹幕重叠。
        int bestTrack = -1;
        int bestRightEdge = std::numeric_limits<int>::max();
        for (int i = 0; i < m_trackOccupied.size(); ++i) {
            if (m_trackOccupied[i] < bestRightEdge) {
                bestRightEdge = m_trackOccupied[i];
                bestTrack = i;
            }
        }

        return bestTrack;
    }

    // 更新轨道占用情况
    void updateTrackOccupation()
    {
        m_trackOccupied.fill(0);

        for (const DanmakuRenderItem &item : m_activeDanmaku) {
            if (item.data.type != 0) {
                continue;
            }
            int track = item.yPos / m_trackHeight;
            if (track >= 0 && track < m_trackOccupied.size()) {
                int rightEdge = item.xPos + item.textSize.width();
                if (rightEdge > m_trackOccupied[track]) {
                    m_trackOccupied[track] = rightEdge;
                }
            }
        }
    }

    int findSpecialTrack(int type)
    {
        const int maxLines = qMax(1, qMin(3, m_trackCount / 3 + 1));
        for (int i = 0; i < maxLines; ++i) {
            if (!m_specialLineOccupied.contains(specialTrackKey(type, i))) {
                return i;
            }
        }
        return QRandomGenerator::global()->bounded(maxLines);
    }

    int specialTrackKey(int type, int track) const
    {
        return type * 1000 + track;
    }

    int specialLineIndex(const DanmakuRenderItem &item) const
    {
        if (item.data.type == 1) {
            return qMax(0, item.yPos / m_trackHeight);
        }

        const int distanceFromBottom = qMax(0, height() - item.yPos - 20);
        return qMax(0, distanceFromBottom / m_trackHeight - 1);
    }
};

#endif // DANMAKUWIDGET_H
