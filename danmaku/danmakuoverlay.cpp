#include "danmakuoverlay.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QTimer>
#include <QtGlobal>
#include <limits>

DanmakuOverlay::DanmakuOverlay(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setStyleSheet(QStringLiteral("background: transparent;"));
    setVisible(true);

    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(30);
    connect(m_updateTimer, &QTimer::timeout, this, &DanmakuOverlay::updateDanmaku);
    m_updateTimer->start();
}

DanmakuOverlay::~DanmakuOverlay()
{
    clear();
}

void DanmakuOverlay::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled) {
        clear();
    }
}

bool DanmakuOverlay::isEnabled() const
{
    return m_enabled;
}

void DanmakuOverlay::setPaused(bool paused)
{
    if (m_paused == paused) {
        return;
    }

    m_paused = paused;
}

bool DanmakuOverlay::isPaused() const
{
    return m_paused;
}

void DanmakuOverlay::setSpeed(int speedValue)
{
    m_speed = qBound(50, speedValue, 300);
}

int DanmakuOverlay::speed() const
{
    return m_speed;
}

void DanmakuOverlay::setMaxDanmakuOnScreen(int max)
{
    m_maxDanmakuOnScreen = qBound(10, max, 100);
}

int DanmakuOverlay::maxDanmakuOnScreen() const
{
    return m_maxDanmakuOnScreen;
}

void DanmakuOverlay::showDanmaku(const DanmakuItem& item)
{
    if (!m_enabled || m_activeDanmaku.size() >= m_maxDanmakuOnScreen) {
        return;
    }

    RenderItem renderItem;
    renderItem.data = item;
    renderItem.speed = m_speed;

    const QFont font(QStringLiteral("Microsoft YaHei"), item.fontSize, QFont::Bold);
    const QFontMetrics fm(font);
    const QString text = QStringLiteral("%1: %2").arg(item.username, item.content);
    renderItem.textSize = fm.size(Qt::TextSingleLine, text);

    updateTrackCount();
    int track = 0;

    if (item.type == 0) {
        track = findAvailableTrack();
        if (track < 0) {
            return;
        }
        renderItem.xPos = width();
        renderItem.yPos = track * m_trackHeight + 10;
    } else {
        track = findSpecialTrack(item.type);
        renderItem.speed = 0;
        renderItem.xPos = qMax(20, (width() - renderItem.textSize.width()) / 2);
        if (item.type == 1) {
            renderItem.yPos = track * m_trackHeight + 10;
        } else {
            renderItem.yPos = height() - (track + 1) * m_trackHeight - 20;
        }
        m_specialLineOccupied.insert(specialTrackKey(item.type, track), 80);
    }

    m_activeDanmaku.append(renderItem);

    if (item.type == 0 && track < m_trackOccupied.size()) {
        m_trackOccupied[track] = renderItem.xPos + renderItem.textSize.width();
    }

    update();
}

void DanmakuOverlay::clear()
{
    m_danmakuQueue.clear();
    m_activeDanmaku.clear();
    m_trackOccupied.clear();
    m_specialLineOccupied.clear();
    update();
}

void DanmakuOverlay::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    if (!m_enabled || m_activeDanmaku.isEmpty()) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    for (const RenderItem& item : m_activeDanmaku) {
        const QString text = QStringLiteral("%1: %2").arg(item.data.username, item.data.content);
        const QFont font(QStringLiteral("Microsoft YaHei"), item.data.fontSize, QFont::Bold);
        painter.setFont(font);

        QPainterPath path;
        path.addText(item.xPos, item.yPos + item.data.fontSize, font, text);

        painter.strokePath(path, QPen(Qt::black, 3));
        painter.fillPath(path, QBrush(QColor(item.data.color)));
    }
}

void DanmakuOverlay::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateTrackCount();
}

void DanmakuOverlay::updateDanmaku()
{
    if (!m_enabled || m_paused) {
        return;
    }

    while (!m_danmakuQueue.isEmpty() && m_activeDanmaku.size() < m_maxDanmakuOnScreen) {
        showDanmaku(m_danmakuQueue.takeFirst());
    }

    bool needUpdate = false;
    QVector<int> toRemove;
    const QVector<int> specialKeys = m_specialLineOccupied.keys().toVector();
    for (int key : specialKeys) {
        const int remain = m_specialLineOccupied.value(key) - 1;
        if (remain <= 0) {
            m_specialLineOccupied.remove(key);
        } else {
            m_specialLineOccupied[key] = remain;
        }
    }

    for (int i = 0; i < m_activeDanmaku.size(); ++i) {
        RenderItem& item = m_activeDanmaku[i];

        if (item.data.type == 0) {
            const int moveDistance = item.speed * m_updateTimer->interval() / 1000;
            item.xPos -= moveDistance;
        }

        needUpdate = true;

        if ((item.data.type == 0 && item.xPos + item.textSize.width() < 0)
            || (item.data.type != 0
                && !m_specialLineOccupied.contains(specialTrackKey(item.data.type,
                                                                   specialLineIndex(item))))) {
            toRemove.append(i);
        }
    }

    for (int i = toRemove.size() - 1; i >= 0; --i) {
        m_activeDanmaku.removeAt(toRemove[i]);
    }

    updateTrackOccupation();

    if (needUpdate) {
        update();
    }
}

void DanmakuOverlay::updateTrackCount()
{
    const int newTrackCount = (height() - 100) / m_trackHeight;
    if (newTrackCount == m_trackCount) {
        return;
    }

    m_trackCount = qMax(1, newTrackCount);
    m_trackOccupied.resize(m_trackCount);
    m_trackOccupied.fill(0);
}

int DanmakuOverlay::findAvailableTrack()
{
    if (m_trackOccupied.isEmpty()) {
        updateTrackCount();
    }

    for (int i = 0; i < m_trackOccupied.size(); ++i) {
        if (m_trackOccupied[i] <= width() - 200) {
            return i;
        }
    }

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

void DanmakuOverlay::updateTrackOccupation()
{
    m_trackOccupied.fill(0);

    for (const RenderItem& item : m_activeDanmaku) {
        if (item.data.type != 0) {
            continue;
        }

        const int track = item.yPos / m_trackHeight;
        if (track >= 0 && track < m_trackOccupied.size()) {
            const int rightEdge = item.xPos + item.textSize.width();
            if (rightEdge > m_trackOccupied[track]) {
                m_trackOccupied[track] = rightEdge;
            }
        }
    }
}

int DanmakuOverlay::findSpecialTrack(int type)
{
    const int maxLines = qMax(1, qMin(3, m_trackCount / 3 + 1));
    for (int i = 0; i < maxLines; ++i) {
        if (!m_specialLineOccupied.contains(specialTrackKey(type, i))) {
            return i;
        }
    }
    return QRandomGenerator::global()->bounded(maxLines);
}

int DanmakuOverlay::specialTrackKey(int type, int track) const
{
    return type * 1000 + track;
}

int DanmakuOverlay::specialLineIndex(const RenderItem& item) const
{
    if (item.data.type == 1) {
        return qMax(0, item.yPos / m_trackHeight);
    }

    const int distanceFromBottom = qMax(0, height() - item.yPos - 20);
    return qMax(0, distanceFromBottom / m_trackHeight - 1);
}
