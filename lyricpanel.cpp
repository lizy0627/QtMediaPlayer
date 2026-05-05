#include "lyricpanel.h"

#include <QVBoxLayout>

LyricPanel::LyricPanel(QWidget* parent)
    : QGroupBox(QStringLiteral("歌词"), parent)
{
    setObjectName("audioGroup");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 18, 8, 8);

    m_lyricWidget = new LyricWidget(this);
    m_lyricWidget->setMinimumHeight(450);
    layout->addWidget(m_lyricWidget);
}

LyricWidget* LyricPanel::lyricWidget() const
{
    return m_lyricWidget;
}

void LyricPanel::setLyrics(const QList<LyricLine>& lyrics)
{
    m_lyricWidget->setLyrics(lyrics);
}

void LyricPanel::showStatus(LyricDisplayState state, const QString& detail)
{
    m_lyricWidget->showStatus(state, detail);
}

void LyricPanel::clear()
{
    m_lyricWidget->clear();
}

void LyricPanel::updatePosition(qint64 position)
{
    m_lyricWidget->updatePosition(position);
}
