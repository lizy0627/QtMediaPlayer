#include "danmakupanel.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QtGlobal>
#include <limits>

DanmakuPanel::DanmakuPanel(QWidget* parent)
    : QWidget(parent)
{
    setMinimumWidth(300);
    setMaximumWidth(400);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(5);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setStyleSheet(
        QStringLiteral("QLabel {"
                       " background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
                       " stop:0 #667eea, stop:1 #764ba2);"
                       " color: white;"
                       " padding: 8px;"
                       " border-radius: 5px;"
                       " font-size: 12pt;"
                       " font-weight: bold;"
                       " font-family: 'Microsoft YaHei';"
                       "}"));
    layout->addWidget(m_titleLabel);

    m_listWidget = new QListWidget(this);
    m_listWidget->setStyleSheet(
        QStringLiteral("QListWidget {"
                       " background-color: #1e1e1e;"
                       " color: #ffffff;"
                       " border: 2px solid #667eea;"
                       " border-radius: 5px;"
                       " padding: 5px;"
                       " font-size: 11pt;"
                       " font-family: 'Microsoft YaHei';"
                       "}"
                       "QListWidget::item {"
                       " padding: 8px;"
                       " border-bottom: 1px solid #333;"
                       " border-radius: 3px;"
                       "}"
                       "QListWidget::item:hover {"
                       " background-color: #2d2d2d;"
                       "}"
                       "QListWidget::item:selected {"
                       " background-color: rgba(102, 126, 234, 0.35);"
                       " color: #ffffff;"
                       " border: 1px solid rgba(124, 143, 240, 0.8);"
                       "}"
                       "QScrollBar:vertical {"
                       " background: #2d2d2d;"
                       " width: 12px;"
                       " border-radius: 6px;"
                       "}"
                       "QScrollBar::handle:vertical {"
                       " background: #667eea;"
                       " border-radius: 6px;"
                       " min-height: 20px;"
                       "}"
                       "QScrollBar::handle:vertical:hover {"
                       " background: #7c8ff0;"
                       "}"));
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(m_listWidget);

    updateTitle(0);
}

void DanmakuPanel::updateList(const QVector<DanmakuItem>& items)
{
    m_listWidget->clear();
    m_allDanmaku.clear();
    m_currentHighlightIndex = -1;

    for (const DanmakuItem& item : items) {
        appendDanmakuItem(item);
        m_allDanmaku.append(item);
    }

    ensureMaxItems();
    updateTitle(m_allDanmaku.size());

    if (m_autoScroll && m_listWidget->count() > 0) {
        m_listWidget->scrollToBottom();
    }
}

void DanmakuPanel::appendDanmaku(const DanmakuItem& item)
{
    appendDanmakuItem(item);
    m_allDanmaku.append(item);
    ensureMaxItems();
    updateTitle(m_allDanmaku.size());

    if (m_autoScroll && m_listWidget->count() > 0) {
        m_listWidget->scrollToBottom();
    }
}

void DanmakuPanel::clearPanel()
{
    m_listWidget->clear();
    m_allDanmaku.clear();
    m_currentHighlightIndex = -1;
    updateTitle(0);
}

void DanmakuPanel::highlightByTime(qint64 currentTime)
{
    if (m_allDanmaku.isEmpty()) {
        clearHighlight();
        return;
    }

    int targetIndex = -1;
    qint64 minDelta = std::numeric_limits<qint64>::max();

    for (int i = 0; i < m_allDanmaku.size(); ++i) {
        const qint64 delta = qAbs(m_allDanmaku[i].timestamp - currentTime);
        if (delta <= 1000 && delta < minDelta) {
            minDelta = delta;
            targetIndex = i;
        }
        if (m_allDanmaku[i].timestamp > currentTime + 1000) {
            break;
        }
    }

    const int visibleStart = qMax(0, m_allDanmaku.size() - m_listWidget->count());
    const int widgetIndex = targetIndex >= visibleStart ? targetIndex - visibleStart : -1;

    if (widgetIndex < 0 || widgetIndex >= m_listWidget->count()) {
        clearHighlight();
        return;
    }

    if (m_currentHighlightIndex == widgetIndex) {
        return;
    }

    m_currentHighlightIndex = widgetIndex;
    QSignalBlocker blocker(m_listWidget);
    m_listWidget->setCurrentRow(widgetIndex);
    QListWidgetItem* item = m_listWidget->item(widgetIndex);
    if (item) {
        m_listWidget->scrollToItem(item, QAbstractItemView::PositionAtCenter);
    }
}

void DanmakuPanel::setAutoScroll(bool enabled)
{
    m_autoScroll = enabled;
}

void DanmakuPanel::setMaxItems(int max)
{
    m_maxItems = qMax(10, max);
    ensureMaxItems();
}

QString DanmakuPanel::formatDanmakuText(const DanmakuItem& item) const
{
    const qint64 seconds = item.timestamp / 1000;
    const QString timeText = QStringLiteral("%1:%2")
                                 .arg(seconds / 60, 2, 10, QLatin1Char('0'))
                                 .arg(seconds % 60, 2, 10, QLatin1Char('0'));

    return QStringLiteral("[%1] %2: %3").arg(timeText, item.username, item.content);
}

void DanmakuPanel::appendDanmakuItem(const DanmakuItem& item)
{
    QListWidgetItem* listItem = new QListWidgetItem(formatDanmakuText(item));
    listItem->setForeground(QBrush(QColor(item.color)));
    listItem->setData(Qt::UserRole, item.timestamp);
    listItem->setToolTip(QStringLiteral("\u53d1\u9001\u65f6\u95f4: %1")
                             .arg(item.createTime.isValid()
                                      ? item.createTime.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"))
                                      : QStringLiteral("\u672a\u77e5")));
    m_listWidget->addItem(listItem);
}

void DanmakuPanel::ensureMaxItems()
{
    while (m_listWidget->count() > m_maxItems) {
        delete m_listWidget->takeItem(0);
    }
}

void DanmakuPanel::updateTitle(int count)
{
    m_titleLabel->setText(QStringLiteral("\u5f39\u5e55\u5217\u8868 (%1)").arg(count));
}

void DanmakuPanel::clearHighlight()
{
    if (m_currentHighlightIndex < 0) {
        return;
    }

    QSignalBlocker blocker(m_listWidget);
    m_listWidget->clearSelection();
    m_listWidget->setCurrentRow(-1);
    m_currentHighlightIndex = -1;
}
