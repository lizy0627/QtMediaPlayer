#include "audiocontrolbar.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QToolButton>
#include <QTransform>
#include <QVBoxLayout>

#include "audiostyle.h"

AudioControlBar::AudioControlBar(QWidget* parent)
    : QGroupBox(QStringLiteral("播放控制"), parent)
{
    setObjectName("audioGroup");

    auto* controlLayout = new QVBoxLayout(this);
    controlLayout->setContentsMargins(10, 18, 10, 10);
    controlLayout->setSpacing(12);

    auto* modeLayout = new QHBoxLayout();
    m_btnLoopList = createModeButton(QStringLiteral("列表循环"), QStringLiteral("列表循环"));
    m_btnLoopSingle = createModeButton(QStringLiteral("单曲循环"), QStringLiteral("单曲循环"));
    m_btnRandom = createModeButton(QStringLiteral("随机播放"), QStringLiteral("随机播放"));
    modeLayout->addWidget(m_btnLoopList);
    modeLayout->addWidget(m_btnLoopSingle);
    modeLayout->addWidget(m_btnRandom);
    modeLayout->addStretch();
    controlLayout->addLayout(modeLayout);

    auto* progressLayout = new QHBoxLayout();
    m_currentTime = new QLabel(QStringLiteral("00:00"), this);
    m_currentTime->setFixedWidth(50);
    m_currentTime->setAlignment(Qt::AlignCenter);

    m_progressSlider = new QSlider(Qt::Horizontal, this);
    m_progressSlider->setRange(0, 100);

    m_totalTime = new QLabel(QStringLiteral("00:00"), this);
    m_totalTime->setFixedWidth(50);
    m_totalTime->setAlignment(Qt::AlignCenter);

    progressLayout->addWidget(m_currentTime);
    progressLayout->addWidget(m_progressSlider);
    progressLayout->addWidget(m_totalTime);
    controlLayout->addLayout(progressLayout);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setAlignment(Qt::AlignCenter);
    buttonLayout->setSpacing(24);

    m_btnPrev = createTransportButton(QIcon(":/assets/pre.png"), QSize(40, 40), QStringLiteral("上一首"), false);

    m_btnPlayPause = createTransportButton(QIcon(":/assets/play.png"), QSize(48, 48), QStringLiteral("播放/暂停"), true);

    QPixmap nextPixmap(":/assets/pre.png");
    if (!nextPixmap.isNull()) {
        QTransform transform;
        transform.scale(-1, 1);
        transform.translate(-nextPixmap.width(), 0);
        nextPixmap = nextPixmap.transformed(transform);
    }
    m_btnNext = createTransportButton(QIcon(nextPixmap), QSize(40, 40), QStringLiteral("下一首"), false);

    auto addButtonGroup = [buttonLayout, this](QPushButton* button, const QString& text) {
        auto* group = new QVBoxLayout();
        group->setAlignment(Qt::AlignCenter);
        group->setSpacing(5);

        auto* label = new QLabel(text, this);
        label->setObjectName("accentLabel");
        label->setAlignment(Qt::AlignCenter);

        group->addWidget(button);
        group->addWidget(label);
        buttonLayout->addLayout(group);
    };

    addButtonGroup(m_btnPrev, QStringLiteral("上一首"));
    addButtonGroup(m_btnPlayPause, QStringLiteral("播放/暂停"));
    addButtonGroup(m_btnNext, QStringLiteral("下一首"));
    controlLayout->addLayout(buttonLayout);

    auto* volumeLayout = new QHBoxLayout();
    volumeLayout->setSpacing(10);

    m_volumeIcon = new QLabel(QStringLiteral("音量"), this);
    m_volumeIcon->setFixedWidth(36);
    m_volumeIcon->setAlignment(Qt::AlignCenter);

    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeSlider->setToolTip(QStringLiteral("音量控制"));

    m_volumeLabel = new QLabel(QStringLiteral("80%"), this);
    m_volumeLabel->setObjectName("accentLabel");
    m_volumeLabel->setFixedWidth(45);
    m_volumeLabel->setAlignment(Qt::AlignCenter);

    volumeLayout->addWidget(m_volumeIcon);
    volumeLayout->addWidget(m_volumeSlider);
    volumeLayout->addWidget(m_volumeLabel);
    controlLayout->addLayout(volumeLayout);

    connect(m_btnPlayPause, &QPushButton::clicked, this, &AudioControlBar::playPauseRequested);
    connect(m_btnPrev, &QPushButton::clicked, this, &AudioControlBar::previousRequested);
    connect(m_btnNext, &QPushButton::clicked, this, &AudioControlBar::nextRequested);
    connect(m_btnLoopList, &QToolButton::clicked, this, [this]() { emit playModeRequested(PlaylistListLoop); });
    connect(m_btnLoopSingle, &QToolButton::clicked, this, [this]() { emit playModeRequested(PlaylistSingleLoop); });
    connect(m_btnRandom, &QToolButton::clicked, this, [this]() { emit playModeRequested(PlaylistRandom); });
    connect(m_progressSlider, &QSlider::sliderMoved, this, &AudioControlBar::positionRequested);
    connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int value) {
        m_volumeLabel->setText(QStringLiteral("%1%").arg(value));
        updateVolumeIcon(value);
        emit volumeChanged(value);
    });
}

void AudioControlBar::setPlaying(bool playing)
{
    m_btnPlayPause->setIcon(QIcon(playing ? ":/assets/pause.png" : ":/assets/play.png"));
    m_btnPlayPause->setIconSize(QSize(48, 48));
    m_btnPlayPause->setToolTip(playing ? QStringLiteral("暂停") : QStringLiteral("播放"));
}

void AudioControlBar::setPlayMode(PlaylistPlayMode mode)
{
    m_btnLoopList->setChecked(mode == PlaylistListLoop);
    m_btnLoopSingle->setChecked(mode == PlaylistSingleLoop);
    m_btnRandom->setChecked(mode == PlaylistRandom);
}

void AudioControlBar::setProgress(qint64 position)
{
    m_currentTime->setText(formatTime(position));
    if (!m_progressSlider->isSliderDown()) {
        QSignalBlocker blocker(m_progressSlider);
        m_progressSlider->setValue(static_cast<int>(position));
    }
}

void AudioControlBar::setDuration(qint64 duration)
{
    m_totalTime->setText(formatTime(duration));
    m_progressSlider->setRange(0, static_cast<int>(duration));
}

void AudioControlBar::setVolumeValue(int value)
{
    QSignalBlocker blocker(m_volumeSlider);
    m_volumeSlider->setValue(value);
    m_volumeLabel->setText(QStringLiteral("%1%").arg(value));
    updateVolumeIcon(value);
}

QToolButton* AudioControlBar::createModeButton(const QString& text, const QString& tooltip)
{
    auto* button = new QToolButton(this);
    button->setText(text);
    button->setCheckable(true);
    button->setToolTip(tooltip);
    AudioStyle::setModeButton(button);
    return button;
}

QPushButton* AudioControlBar::createTransportButton(const QIcon& icon, const QSize& size, const QString& tooltip, bool mainButton)
{
    auto* button = new QPushButton(this);
    button->setIcon(icon);
    button->setIconSize(size);
    button->setFixedSize(mainButton ? QSize(70, 70) : QSize(60, 60));
    button->setToolTip(tooltip);
    AudioStyle::setButtonRole(button, mainButton ? "roundMain" : "round");
    return button;
}

QString AudioControlBar::formatTime(qint64 milliseconds) const
{
    int seconds = static_cast<int>(milliseconds / 1000);
    const int minutes = seconds / 60;
    seconds %= 60;
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

void AudioControlBar::updateVolumeIcon(int value)
{
    if (value == 0) {
        m_volumeIcon->setText(QStringLiteral("静音"));
    } else if (value < 30) {
        m_volumeIcon->setText(QStringLiteral("低"));
    } else if (value < 70) {
        m_volumeIcon->setText(QStringLiteral("中"));
    } else {
        m_volumeIcon->setText(QStringLiteral("高"));
    }
}
