#ifndef DANMAKUDISPLAY_H
#define DANMAKUDISPLAY_H

#include <QWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QScrollBar>
#include <QSignalBlocker>
#include <limits>
#include "danmakumanager.h"

// Legacy danmaku list display.
// Kept for compatibility only; new code should use danmaku/DanmakuPanel.
// 弹幕显示窗口（类似聊天窗口）
class DanmakuDisplay : public QWidget
{
    Q_OBJECT

private:
    QListWidget* m_listWidget;
    QLabel* m_titleLabel;
    int m_maxItems;
    bool m_autoScroll;
    QVector<DanmakuItem> m_allDanmaku;
    int m_currentHighlightIndex;

public:
    explicit DanmakuDisplay(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_maxItems(100)
        , m_autoScroll(true)
        , m_currentHighlightIndex(-1)
    {
        setMinimumWidth(300);
        setMaximumWidth(400);
        
        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setContentsMargins(5, 5, 5, 5);
        layout->setSpacing(5);
        
        // 标题
        m_titleLabel = new QLabel("💬 弹幕列表", this);
        m_titleLabel->setStyleSheet(
            "QLabel {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
            "       stop:0 #667eea, stop:1 #764ba2);"
            "   color: white;"
            "   padding: 8px;"
            "   border-radius: 5px;"
            "   font-size: 12pt;"
            "   font-weight: bold;"
            "   font-family: 'Microsoft YaHei';"
            "}"
        );
        layout->addWidget(m_titleLabel);
        
        // 弹幕列表
        m_listWidget = new QListWidget(this);
        m_listWidget->setStyleSheet(
            "QListWidget {"
            "   background-color: #1e1e1e;"
            "   color: #ffffff;"
            "   border: 2px solid #667eea;"
            "   border-radius: 5px;"
            "   padding: 5px;"
            "   font-size: 11pt;"
            "   font-family: 'Microsoft YaHei';"
            "}"
            "QListWidget::item {"
            "   padding: 8px;"
            "   border-bottom: 1px solid #333;"
            "   border-radius: 3px;"
            "}"
            "QListWidget::item:hover {"
            "   background-color: #2d2d2d;"
            "}"
            "QListWidget::item:selected {"
            "   background-color: rgba(102, 126, 234, 0.35);"
            "   color: #ffffff;"
            "   border: 1px solid rgba(124, 143, 240, 0.8);"
            "}"
            "QScrollBar:vertical {"
            "   background: #2d2d2d;"
            "   width: 12px;"
            "   border-radius: 6px;"
            "}"
            "QScrollBar::handle:vertical {"
            "   background: #667eea;"
            "   border-radius: 6px;"
            "   min-height: 20px;"
            "}"
            "QScrollBar::handle:vertical:hover {"
            "   background: #7c8ff0;"
            "}"
        );
        m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
        m_listWidget->setFocusPolicy(Qt::NoFocus);
        layout->addWidget(m_listWidget);
        
        setLayout(layout);
    }

    void addDanmaku(const DanmakuItem &danmaku)
    {
        appendDanmakuItem(danmaku);
        m_allDanmaku.append(danmaku);
        ensureMaxItems();
        updateTitle(m_allDanmaku.size());
    }

    // 批量添加弹幕
    void addDanmakuList(const QVector<DanmakuItem> &danmakuList)
    {
        clear();

        for (const DanmakuItem &danmaku : danmakuList) {
            appendDanmakuItem(danmaku);
            m_allDanmaku.append(danmaku);
        }

        ensureMaxItems();
        updateTitle(m_allDanmaku.size());

        if (m_autoScroll && m_listWidget->count() > 0) {
            m_listWidget->scrollToBottom();
        }
    }

    void highlightByTime(qint64 currentTime)
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

    // 清空列表
    void clear()
    {
        m_listWidget->clear();
        m_allDanmaku.clear();
        m_currentHighlightIndex = -1;
        updateTitle(0);
    }

    // 设置自动滚动
    void setAutoScroll(bool enabled)
    {
        m_autoScroll = enabled;
    }

    // 设置最大显示数量
    void setMaxItems(int max)
    {
        m_maxItems = qMax(10, max);
    }

    // 更新标题（显示弹幕数量）
    void updateTitle(int count)
    {
        m_titleLabel->setText(QString("💬 弹幕列表 (%1)").arg(count));
    }

private:
    QString formatDanmakuText(const DanmakuItem &danmaku) const
    {
        qint64 seconds = danmaku.timestamp / 1000;
        QString timeStr = QString("%1:%2")
            .arg(seconds / 60, 2, 10, QLatin1Char('0'))
            .arg(seconds % 60, 2, 10, QLatin1Char('0'));

        return QString("[%1] %2: %3")
            .arg(timeStr)
            .arg(danmaku.username)
            .arg(danmaku.content);
    }

    void appendDanmakuItem(const DanmakuItem &danmaku)
    {
        QListWidgetItem* item = new QListWidgetItem(formatDanmakuText(danmaku));
        item->setForeground(QBrush(QColor(danmaku.color)));
        item->setData(Qt::UserRole, danmaku.timestamp);
        item->setToolTip(QString("发送时间：%1").arg(danmaku.createTime.isValid() ? danmaku.createTime.toString("yyyy-MM-dd hh:mm:ss") : "未知"));
        m_listWidget->addItem(item);
    }

    void ensureMaxItems()
    {
        while (m_listWidget->count() > m_maxItems) {
            delete m_listWidget->takeItem(0);
        }
    }

    void clearHighlight()
    {
        if (m_currentHighlightIndex < 0) {
            return;
        }

        QSignalBlocker blocker(m_listWidget);
        m_listWidget->clearSelection();
        m_listWidget->setCurrentRow(-1);
        m_currentHighlightIndex = -1;
    }
};

#endif // DANMAKUDISPLAY_H
