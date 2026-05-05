#include "spectrumpanel.h"

#include <QVBoxLayout>

#include "spectrumwidget.h"

SpectrumPanel::SpectrumPanel(QWidget* parent)
    : QGroupBox(QStringLiteral("模拟频谱视觉效果"), parent)
{
    setObjectName("audioGroup");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 18, 8, 8);

    m_spectrumWidget = new SpectrumWidget(this);
    m_spectrumWidget->setMinimumHeight(150);
    layout->addWidget(m_spectrumWidget);
}

void SpectrumPanel::setMediaPlayer(QMediaPlayer* player)
{
    m_spectrumWidget->setMediaPlayer(player);
}

void SpectrumPanel::setPlaying(bool playing)
{
    m_spectrumWidget->setPlaying(playing);
}
