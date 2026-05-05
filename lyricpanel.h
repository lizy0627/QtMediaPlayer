#ifndef LYRICPANEL_H
#define LYRICPANEL_H

#include <QGroupBox>
#include <QList>

#include "lyricwidget.h"

class LyricPanel : public QGroupBox
{
    Q_OBJECT

public:
    explicit LyricPanel(QWidget* parent = nullptr);

    LyricWidget* lyricWidget() const;
    void setLyrics(const QList<LyricLine>& lyrics);
    void showStatus(LyricDisplayState state, const QString& detail = QString());
    void clear();
    void updatePosition(qint64 position);

private:
    LyricWidget* m_lyricWidget = nullptr;
};

#endif // LYRICPANEL_H
