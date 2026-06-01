#include "videocontrolbar.h"

#ifdef USE_FFMPEG
#include "ffmpegframeextractor.h"
#endif

#include <QComboBox>
#include <QCursor>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace {
constexpr int kPreviewWidth = 160;
constexpr int kPreviewHeight = 90;
constexpr int kMaxPreviewCacheEntries = 120;

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

    m_btnRetryOnline = createRoundButton("Retry", QStringLiteral("\u91cd\u8bd5\u89e3\u6790\u5f53\u524d\u5728\u7ebf\u89c6\u9891"), "videoToolRetry");
    connect(m_btnRetryOnline, &QPushButton::clicked, this, &VideoControlBar::retryOnlineRequested);

    m_btnHistory = createRoundButton("Hist", "播放历史记录", "videoToolHistory");
    connect(m_btnHistory, &QPushButton::clicked, this, &VideoControlBar::historyRequested);

    m_btnQueue = createRoundButton("Queue", "Video queue", "videoToolQueue");
    connect(m_btnQueue, &QPushButton::clicked, this, &VideoControlBar::queueRequested);

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
    m_slider->setMouseTracking(true);
    m_slider->installEventFilter(this);
    connect(m_slider, &QSlider::sliderMoved, this, [this](int position) {
        emit progressJumpRequested(position);
    });

    m_previewLabel = new QLabel(this, Qt::ToolTip | Qt::FramelessWindowHint);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setAttribute(Qt::WA_ShowWithoutActivating, true);
    m_previewLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: rgba(16, 20, 28, 230); border: 1px solid rgba(255, 255, 255, 90); padding: 4px; }"));
    m_previewLabel->hide();

    m_previewWatcher = new QFutureWatcher<PreviewFrameResult>(this);
    connect(m_previewWatcher,
            &QFutureWatcher<PreviewFrameResult>::finished,
            this,
            &VideoControlBar::onPreviewExtractionFinished);

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
    layout->addLayout(createButtonGroup(m_btnQueue, "Queue"));
    layout->addSpacing(5);
    layout->addLayout(createButtonGroup(m_btnRetryOnline, "Retry"));
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

void VideoControlBar::setPreviewVideoPath(const QString& filePath)
{
    const QString trimmedPath = filePath.trimmed();
    if (m_previewVideoPath == trimmedPath) {
        return;
    }

    m_previewVideoPath = trimmedPath;
    clearPreviewCache();
    hidePreview();
}

bool VideoControlBar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != m_slider) {
        return QWidget::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::Enter:
        m_previewHoverActive = true;
        break;
    case QEvent::MouseMove:
    case QEvent::MouseButtonPress: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        m_previewHoverActive = true;
        m_lastPreviewGlobalPos = mouseEvent->globalPosition().toPoint();
        const qint64 previewPosition = previewPositionFromSlider(
            qBound(0, static_cast<int>(mouseEvent->position().x()), m_slider->width()));
        requestPreview(previewPosition);
        break;
    }
    case QEvent::Leave:
    case QEvent::Hide:
        hidePreview();
        break;
    default:
        break;
    }

    return QWidget::eventFilter(watched, event);
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

qint64 VideoControlBar::previewPositionFromSlider(int x) const
{
    if (!m_slider || m_duration <= 0 || m_slider->width() <= 0) {
        return 0;
    }

    QStyleOptionSlider option;
    option.initFrom(m_slider);
    option.orientation = m_slider->orientation();
    option.minimum = m_slider->minimum();
    option.maximum = m_slider->maximum();
    option.sliderPosition = m_slider->sliderPosition();
    option.sliderValue = m_slider->value();
    option.upsideDown = false;

    const QRect groove = m_slider->style()->subControlRect(QStyle::CC_Slider,
                                                           &option,
                                                           QStyle::SC_SliderGroove,
                                                           m_slider);
    const int left = groove.isValid() ? groove.left() : 0;
    const int width = groove.isValid() && groove.width() > 0 ? groove.width() : m_slider->width();
    const int relativeX = qBound(0, x - left, width);
    const int sliderPosition = QStyle::sliderValueFromPosition(0,
                                                               static_cast<int>(m_duration),
                                                               relativeX,
                                                               width);
    return qBound<qint64>(0, static_cast<qint64>(sliderPosition), m_duration);
}

void VideoControlBar::requestPreview(qint64 positionMs)
{
    if (m_previewVideoPath.isEmpty() || m_duration <= 0) {
        hidePreview();
        return;
    }

    const qint64 boundedPosition = qBound<qint64>(0, positionMs, m_duration);
    const qint64 second = boundedPosition / 1000;
    m_currentPreviewSecond = second;
    movePreviewToCursor();

    const auto cached = m_previewCache.constFind(second);
    if (cached != m_previewCache.constEnd()) {
        showPreview(*cached);
        return;
    }

    if (m_previewWatcher && m_previewWatcher->isRunning()) {
        m_queuedPreviewSecond = second;
        return;
    }

    startPreviewExtraction(second);
}

void VideoControlBar::startPreviewExtraction(qint64 second)
{
#ifdef USE_FFMPEG
    if (!m_previewWatcher || m_previewVideoPath.isEmpty() || second < 0) {
        return;
    }

    m_extractingPreviewSecond = second;
    m_queuedPreviewSecond = -1;
    const QString filePath = m_previewVideoPath;
    m_previewWatcher->setFuture(QtConcurrent::run([filePath, second]() {
        PreviewFrameResult result;
        result.filePath = filePath;
        result.second = second;

        QImage image;
        QString errorMessage;
        if (FFmpegFrameExtractor::extractFrame(filePath, second * 1000, image, &errorMessage)) {
            result.image = image.scaled(kPreviewWidth,
                                        kPreviewHeight,
                                        Qt::KeepAspectRatio,
                                        Qt::SmoothTransformation);
        }
        return result;
    }));
#else
    Q_UNUSED(second);
#endif
}

void VideoControlBar::onPreviewExtractionFinished()
{
    if (!m_previewWatcher) {
        return;
    }

    const PreviewFrameResult result = m_previewWatcher->result();
    m_extractingPreviewSecond = -1;

    if (result.filePath == m_previewVideoPath && !result.image.isNull()) {
        if (m_previewCache.size() >= kMaxPreviewCacheEntries) {
            m_previewCache.erase(m_previewCache.begin());
        }
        const QPixmap pixmap = QPixmap::fromImage(result.image);
        m_previewCache.insert(result.second, pixmap);
        if (m_previewHoverActive && m_currentPreviewSecond == result.second) {
            showPreview(pixmap);
        }
    }

    const qint64 nextSecond = m_queuedPreviewSecond;
    m_queuedPreviewSecond = -1;
    if (m_previewHoverActive
        && nextSecond >= 0
        && nextSecond != result.second
        && !m_previewCache.contains(nextSecond)) {
        startPreviewExtraction(nextSecond);
    } else if (m_previewHoverActive
               && nextSecond >= 0
               && m_previewCache.contains(nextSecond)) {
        m_currentPreviewSecond = nextSecond;
        showPreview(m_previewCache.value(nextSecond));
    }
}

void VideoControlBar::showPreview(const QPixmap& pixmap)
{
    if (!m_previewLabel || pixmap.isNull()) {
        return;
    }

    m_previewLabel->setPixmap(pixmap);
    m_previewLabel->setFixedSize(pixmap.size() + QSize(10, 10));
    movePreviewToCursor();
    m_previewLabel->show();
}

void VideoControlBar::movePreviewToCursor()
{
    if (!m_previewLabel || !m_slider || !m_previewHoverActive) {
        return;
    }

    const QSize previewSize = m_previewLabel->sizeHint().isValid()
        ? m_previewLabel->sizeHint()
        : QSize(kPreviewWidth + 10, kPreviewHeight + 10);
    QPoint target(m_lastPreviewGlobalPos.x() - previewSize.width() / 2,
                  m_slider->mapToGlobal(QPoint(0, 0)).y() - previewSize.height() - 10);

    if (QScreen* screen = QGuiApplication::screenAt(m_lastPreviewGlobalPos)) {
        const QRect available = screen->availableGeometry();
        target.setX(qBound(available.left(),
                           target.x(),
                           available.right() - previewSize.width()));
        target.setY(qBound(available.top(),
                           target.y(),
                           available.bottom() - previewSize.height()));
    }

    m_previewLabel->move(target);
}

void VideoControlBar::hidePreview()
{
    m_previewHoverActive = false;
    m_currentPreviewSecond = -1;
    m_queuedPreviewSecond = -1;
    if (m_previewLabel) {
        m_previewLabel->hide();
    }
}

void VideoControlBar::clearPreviewCache()
{
    m_previewCache.clear();
    m_extractingPreviewSecond = -1;
    m_queuedPreviewSecond = -1;
    m_currentPreviewSecond = -1;
}
