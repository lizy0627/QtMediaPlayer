#include "videocontrolbar.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyle>
#include <QVBoxLayout>

namespace {
void refreshStyle(QWidget* widget)
{
    if (!widget) {
        return;
    }

    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}
}

VideoControlBar::VideoControlBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("controlPannel");
    setProperty("component", "videoControl");
    setMaximumHeight(90);

    m_btnCtr = new QPushButton(this);
    m_btnCtr->setFixedSize(31, 31);
    setButtonRole(m_btnCtr, "videoPlainIcon");
    m_btnCtr->setIconSize(QSize(26, 26));
    connect(m_btnCtr, &QPushButton::clicked, this, &VideoControlBar::playPauseRequested);

    m_btnPlayPause = new QPushButton(this);
    m_btnPlayPause->setIconSize(QSize(28, 28));
    m_btnPlayPause->setFixedSize(56, 56);
    m_btnPlayPause->setToolTip("播放/暂停 (Space)");
    setButtonRole(m_btnPlayPause, "videoTool");
    connect(m_btnPlayPause, &QPushButton::clicked, this, &VideoControlBar::playPauseRequested);

    m_btnSearchOnline = createRoundButton("Search", "在线搜索视频", "videoToolSearch");
    connect(m_btnSearchOnline, &QPushButton::clicked, this, &VideoControlBar::searchOnlineRequested);

    m_btnHistory = createRoundButton("Hist", "播放历史记录", "videoToolHistory");
    connect(m_btnHistory, &QPushButton::clicked, this, &VideoControlBar::historyRequested);

    m_btnScreenshot = createRoundButton("Shot", "截图保存", "videoToolScreenshot");
    connect(m_btnScreenshot, &QPushButton::clicked, this, &VideoControlBar::screenshotRequested);

    m_btnRecord = createRoundButton("REC", "\u65e0\u58f0\u753b\u9762\u5f55\u5236", "videoToolRecord");
    connect(m_btnRecord, &QPushButton::clicked, this, &VideoControlBar::recordRequested);

    m_btnLogin = createRoundButton("Login", "用户登录", "videoToolLogin");
    connect(m_btnLogin, &QPushButton::clicked, this, &VideoControlBar::loginRequested);

    m_userLabel = new QLabel("未登录", this);
    m_userLabel->setAlignment(Qt::AlignCenter);
    m_userLabel->setProperty("role", "videoUser");
    setUserLabelState("loggedOut");

    m_btnDanmaku = createRoundButton("DM", "弹幕开关", "videoToolDanmaku");
    connect(m_btnDanmaku, &QPushButton::clicked, this, &VideoControlBar::danmakuToggleRequested);

    m_btnAiChat = createRoundButton("AI", QString::fromUtf8("AI 电影助手"), "videoToolAi");
    m_btnAiChat->setCheckable(true);
    connect(m_btnAiChat, &QPushButton::clicked, this, &VideoControlBar::aiPanelToggled);

    m_btnMyDanmaku = createRoundButton("Mine", "我的弹幕记录", "videoToolMine");
    connect(m_btnMyDanmaku, &QPushButton::clicked, this, &VideoControlBar::myDanmakuRequested);

    m_volSlider = new QSlider(Qt::Horizontal, this);
    m_volSlider->setRange(0, 100);
    m_volSlider->setValue(50);
    m_volSlider->setFixedWidth(100);
    connect(m_volSlider, &QSlider::valueChanged, this, &VideoControlBar::volumeChanged);

    m_cbRate = new QComboBox(this);
    m_cbRate->addItems({"0.5x", "0.75x", "1.0x", "1.25x", "1.5x", "2.0x"});
    m_cbRate->setCurrentText("1.0x");
    m_cbRate->setFixedWidth(80);
    connect(m_cbRate, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        QString numStr = text;
        numStr.remove("x");
        emit speedChanged(numStr.toDouble());
    });

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(0, 100);
    m_slider->setFixedHeight(24);
    connect(m_slider, &QSlider::sliderMoved, this, [this](int position) {
        emit progressJumpRequested(position);
    });

    m_timeLabel = new QLabel("00:00 / 00:00", this);
    m_timeLabel->setProperty("role", "videoTime");
    m_timeLabel->setFixedWidth(130);
    m_timeLabel->setAlignment(Qt::AlignCenter);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(m_btnCtr);
    layout->addSpacing(5);
    layout->addLayout(createButtonGroup(m_btnPlayPause, "播放/暂停"));
    layout->addSpacing(10);
    layout->addLayout(createButtonGroup(m_btnSearchOnline, "在线搜索"));
    layout->addSpacing(5);
    layout->addLayout(createButtonGroup(m_btnHistory, "播放历史"));
    layout->addSpacing(5);
    layout->addLayout(createButtonGroup(m_btnScreenshot, "截图"));
    layout->addSpacing(5);
    layout->addLayout(createButtonGroup(m_btnRecord, "\u753b\u9762\u5f55\u5236"));
    layout->addSpacing(15);

    QVBoxLayout* loginLayout = new QVBoxLayout();
    loginLayout->setAlignment(Qt::AlignCenter);
    loginLayout->setSpacing(5);
    loginLayout->addWidget(m_btnLogin);
    loginLayout->addWidget(m_userLabel);
    layout->addLayout(loginLayout);

    layout->addSpacing(10);
    layout->addLayout(createButtonGroup(m_btnDanmaku, "弹幕"));
    layout->addSpacing(8);
    layout->addLayout(createButtonGroup(m_btnAiChat, "AI 助手"));
    layout->addSpacing(8);
    layout->addLayout(createButtonGroup(m_btnMyDanmaku, "我的弹幕"));
    layout->addSpacing(15);
    layout->addWidget(m_volSlider);
    layout->addSpacing(10);
    layout->addWidget(m_cbRate);
    layout->addSpacing(10);
    layout->addWidget(m_slider, 9);
    layout->addSpacing(10);
    layout->addWidget(m_timeLabel, 1);
    layout->setContentsMargins(15, 8, 15, 8);

    setPlaying(false);
}

void VideoControlBar::setPlaying(bool playing)
{
    const QIcon icon(playing ? ":/assets/pause.png" : ":/assets/play.png");
    m_btnCtr->setIcon(icon);
    m_btnPlayPause->setIcon(icon);
}

void VideoControlBar::setRecording(bool recording)
{
    m_btnRecord->setEnabled(true);
    m_btnRecord->setText(recording ? "STOP" : "REC");
    setButtonRole(m_btnRecord, recording ? "videoToolRecordActive" : "videoToolRecord");
}

void VideoControlBar::setRecordProcessing(bool processing)
{
    m_btnRecord->setEnabled(!processing);
    if (processing) {
        m_btnRecord->setText("WAIT");
        setButtonRole(m_btnRecord, "videoToolProcessing");
    }
}

void VideoControlBar::setLoggedInUser(const QString& username)
{
    if (username.isEmpty()) {
        m_userLabel->setText("未登录");
        setUserLabelState("loggedOut");
        m_btnLogin->setText("Login");
        return;
    }

    m_userLabel->setText(username);
    setUserLabelState("loggedIn");
    m_btnLogin->setText("User");
}

void VideoControlBar::setDanmakuEnabled(bool enabled)
{
    setButtonRole(m_btnDanmaku, enabled ? "videoToolDanmaku" : "videoToolDanmakuOff");
}

void VideoControlBar::setProgress(qint64 position)
{
    m_position = qBound<qint64>(0, position, qMax<qint64>(0, m_duration));
    QSignalBlocker blocker(m_slider);
    m_slider->setValue(static_cast<int>(m_position));
    updateTimeLabel();
}

void VideoControlBar::setDuration(qint64 duration)
{
    m_duration = qMax<qint64>(0, duration);
    m_slider->setRange(0, static_cast<int>(m_duration));
    if (m_position > m_duration) {
        m_position = m_duration;
    }
    updateTimeLabel();
}

void VideoControlBar::setVolumeValue(int volume)
{
    QSignalBlocker blocker(m_volSlider);
    m_volSlider->setValue(qBound(0, volume, 100));
}

void VideoControlBar::setSpeedValue(double speed)
{
    for (int i = 0; i < m_cbRate->count(); ++i) {
        QString text = m_cbRate->itemText(i);
        text.remove("x");
        if (qAbs(text.toDouble() - speed) < 0.001) {
            QSignalBlocker blocker(m_cbRate);
            m_cbRate->setCurrentIndex(i);
            return;
        }
    }
}

QPushButton* VideoControlBar::createRoundButton(const QString& text, const QString& tooltip, const char* role)
{
    QPushButton* button = new QPushButton(this);
    button->setText(text);
    button->setFixedSize(56, 56);
    button->setToolTip(tooltip);
    setButtonRole(button, role);
    return button;
}

QVBoxLayout* VideoControlBar::createButtonGroup(QPushButton* button, const QString& labelText)
{
    QVBoxLayout* layout = new QVBoxLayout();
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(5);

    QLabel* label = new QLabel(labelText, this);
    label->setAlignment(Qt::AlignCenter);
    label->setProperty("role", "videoControlLabel");

    layout->addWidget(button);
    layout->addWidget(label);
    return layout;
}

void VideoControlBar::setButtonRole(QPushButton* button, const char* role)
{
    if (!button) {
        return;
    }

    button->setProperty("role", role);
    refreshStyle(button);
}

void VideoControlBar::setUserLabelState(const char* state)
{
    if (!m_userLabel) {
        return;
    }

    m_userLabel->setProperty("state", state);
    refreshStyle(m_userLabel);
}

QString VideoControlBar::formatTime(qint64 milliseconds) const
{
    const qint64 seconds = milliseconds / 1000;
    return QString("%1:%2")
        .arg(seconds / 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

void VideoControlBar::updateTimeLabel()
{
    m_timeLabel->setText(formatTime(m_position) + " / " + formatTime(m_duration));
}
