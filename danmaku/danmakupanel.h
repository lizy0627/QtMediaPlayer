#ifndef DANMAKUPANEL_H
#define DANMAKUPANEL_H

#include <QVector>
#include <QWidget>

#include "danmakuitem.h"

class QLabel;
class QListWidget;

class DanmakuPanel : public QWidget
{
    Q_OBJECT

public:
    explicit DanmakuPanel(QWidget* parent = nullptr);
    void highlightByTime(qint64 currentTime);
    void setAutoScroll(bool enabled);
    void setMaxItems(int max);

public slots:
    void updateList(const QVector<DanmakuItem>& items);
    void appendDanmaku(const DanmakuItem& item);
    void clearPanel();

private:
    QString formatDanmakuText(const DanmakuItem& item) const;
    void appendDanmakuItem(const DanmakuItem& item);
    void ensureMaxItems();
    void updateTitle(int count);
    void clearHighlight();

    QListWidget* m_listWidget = nullptr;
    QLabel* m_titleLabel = nullptr;
    int m_maxItems = 100;
    bool m_autoScroll = true;
    QVector<DanmakuItem> m_allDanmaku;
    int m_currentHighlightIndex = -1;
};

#endif // DANMAKUPANEL_H
