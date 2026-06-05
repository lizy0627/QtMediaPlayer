#include "videoplayer.h"

#include <QCursor>
#include <QEvent>
#include <QFileInfo>
#include <QMessageBox>
#include <QStackedLayout>
#include <QSplitter>
#include <QVBoxLayout>
#include <QVideoWidget>

#ifdef USE_FFMPEG
#include "ffmpeg/ffmpegvideowidget.h"
#endif

#include "aichatpanel.h"
#include "aichatwidget.h"
#include "authdialogcontroller.h"
#include "authservice.h"
#include "captureservice.h"
#include "danmakucontroller.h"
#include "danmakuinputbar.h"
#include "danmakuoverlay.h"
#include "danmakupanel.h"
#include "danmakurepository.h"
#include "mediahistory.h"
#include "mediainfodialog.h"
#include "onlinevideoservice.h"
#include "usersession.h"
#include "videocontrolbar.h"
#include "videoplaybackcontroller.h"
#include "videoplayercontroller.h"
#include "videoqueuedialog.h"

class VideoRenderContainer : public QWidget
{
public:
    enum class Renderer {
        QtVideo,
        FFmpegVideo
    };

    explicit VideoRenderContainer(QWidget* parent = nullptr)
        : QWidget(parent)
        , m_stack(new QStackedLayout(this))
        , m_qtVideoWidget(new QVideoWidget(this))
    {
        setStyleSheet("background: black;");
        m_stack->setContentsMargins(0, 0, 0, 0);
        m_stack->setSpacing(0);

        m_qtVideoWidget->setStyleSheet(
            "background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
            " stop:0 rgba(26, 32, 44, 1),"
            " stop:0.5 rgba(45, 55, 72, 1),"
            " stop:1 rgba(26, 32, 44, 1));");
        m_stack->addWidget(m_qtVideoWidget);

#ifdef USE_FFMPEG
        m_ffmpegVideoWidget = new FFmpegVideoWidget(this);
        m_stack->addWidget(m_ffmpegVideoWidget);
#endif

        setRenderer(Renderer::QtVideo);
    }

    QVideoWidget* qtVideoWidget() const
    {
        return m_qtVideoWidget;
    }

#ifdef USE_FFMPEG
    FFmpegVideoWidget* ffmpegVideoWidget() const
    {
        return m_ffmpegVideoWidget;
    }
#endif

    void setRenderer(Renderer renderer)
    {
        if (renderer == Renderer::FFmpegVideo) {
#ifdef USE_FFMPEG
            if (m_ffmpegVideoWidget != nullptr) {
                m_stack->setCurrentWidget(m_ffmpegVideoWidget);
                m_renderer = renderer;
                return;
            }
#endif
        }

        m_stack->setCurrentWidget(m_qtVideoWidget);
        m_renderer = Renderer::QtVideo;
    }

    Renderer renderer() const
    {
        return m_renderer;
    }

private:
    QStackedLayout* m_stack = nullptr;
    QVideoWidget* m_qtVideoWidget = nullptr;
#ifdef USE_FFMPEG
    FFmpegVideoWidget* m_ffmpegVideoWidget = nullptr;
#endif
    Renderer m_renderer = Renderer::QtVideo;
};

VideoPlayerWidget::VideoPlayerWidget(QWidget* parent,
                                     UserSession* userSession,
                                     AuthService* authService,
                                     AuthDialogController* authDialogController,
                                     MediaHistoryService* historyService)
    : QObject(parent)
    , m_parent(parent)
    , m_historyService(historyService)
{
    createLayout();

    m_userSession = userSession ? userSession : new UserSession(this);
    m_authService = authService ? authService : new AuthService(m_userSession, this);
    m_authDialogController = authDialogController
        ? authDialogController
        : new AuthDialogController(m_authService, this);

    createServices();
    connectSignals();

    if (m_authService) {
        m_authService->initialize();
    }

    if (m_controlBar && m_controller) {
        m_controlBar->setVolumeValue(m_controller->volume());
        m_controlBar->setSpeedValue(m_controller->speed());
    }
    if (m_controlBar && m_danmakuController) {
        m_controlBar->setDanmakuEnabled(m_danmakuController->isEnabled());
    }
}

VideoPlayerWidget::~VideoPlayerWidget()
{
    delete m_danmakuRepository;
}

void VideoPlayerWidget::open(const QString& filePath, bool localFile)
{
    if (m_controller) {
        m_controller->open(filePath, localFile);
    }
}

void VideoPlayerWidget::openQueue(const QStringList& filePaths)
{
    if (m_controller) {
        m_controller->openQueue(filePaths);
    }
}

void VideoPlayerWidget::openAtPosition(const QString& filePath, qint64 position)
{
    if (m_controller) {
        m_controller->openAtPosition(filePath, position);
    }
}

void VideoPlayerWidget::toggle()
{
    if (m_controller) {
        m_controller->togglePlayback();
    }
}

void VideoPlayerWidget::jump(bool forward, int ms)
{
    if (m_controller) {
        m_controller->jump(forward, ms);
    }
}

void VideoPlayerWidget::setVolume(int volumeValue)
{
    if (m_controller) {
        m_controller->setVolume(volumeValue);
    }
}

int VideoPlayerWidget::volume() const
{
    return m_controller ? m_controller->volume() : 0;
}

bool VideoPlayerWidget::isPlaying() const
{
    return m_controller && m_controller->isPlaying();
}

void VideoPlayerWidget::setSpeed(double speedValue)
{
    if (m_controller) {
        m_controller->setSpeed(speedValue);
    }
}

double VideoPlayerWidget::speed() const
{
    return m_controller ? m_controller->speed() : 1.0;
}

void VideoPlayerWidget::setControlsVisible(bool visible)
{
    if (m_controlBar) {
        m_controlBar->setVisible(visible);
    }
}

void VideoPlayerWidget::pause()
{
    if (m_controller) {
        m_controller->pause();
    }
}

void VideoPlayerWidget::play()
{
    if (m_controller) {
        m_controller->play();
    }
}

void VideoPlayerWidget::showMyDanmakuRecords()
{
    if (m_controller) {
        m_controller->showMyDanmakuRecords(m_parent);
    }
}

bool VideoPlayerWidget::showMediaInfo()
{
    if (!m_controller) {
        return false;
    }

    const QString filePath = m_controller->currentVideoPath().trimmed();
    if (filePath.isEmpty()) {
        QMessageBox::information(m_parent,
                                 QStringLiteral("\u5a92\u4f53\u4fe1\u606f"),
                                 QStringLiteral("\u8bf7\u5148\u6253\u5f00\u672c\u5730\u89c6\u9891\u6587\u4ef6\u3002"));
        return false;
    }

    MediaInfoDialog dialog(filePath, m_parent);
    dialog.exec();
    return true;
}

void VideoPlayerWidget::setUseFFmpegBackend(bool enabled)
{
#ifdef USE_FFMPEG
    if (!m_playbackController || !m_videoSurface) {
        return;
    }

    m_playbackController->setLocalFileBackendPolicy(
        enabled
            ? VideoPlaybackController::LocalFileBackendPolicy::PreferFFmpeg
            : VideoPlaybackController::LocalFileBackendPolicy::PreferQtMedia);
    m_videoSurface->setRenderer(enabled
                                    ? VideoRenderContainer::Renderer::FFmpegVideo
                                    : VideoRenderContainer::Renderer::QtVideo);
    m_playbackController->setBackendType(enabled
                                             ? VideoPlaybackController::BackendType::FFmpeg
                                             : VideoPlaybackController::BackendType::QtMedia);
    m_playbackController->setVideoOutput(m_video);
    m_playbackController->setFrameOutput(m_videoSurface->ffmpegVideoWidget());
    if (m_danmakuWidget) {
        m_danmakuWidget->raise();
    }
    if (m_danmakuController) {
        m_danmakuController->setOverlayGeometry(m_videoContainer->rect());
    }
#else
    Q_UNUSED(enabled)
#endif
}

bool VideoPlayerWidget::isUsingFFmpegBackend() const
{
#ifdef USE_FFMPEG
    return m_playbackController
        && m_playbackController->backendType() == VideoPlaybackController::BackendType::FFmpeg;
#else
    return false;
#endif
}

void VideoPlayerWidget::showError(const QString& message)
{
    QMessageBox::critical(m_parent, QStringLiteral("播放错误"), message);
}

void VideoPlayerWidget::showWarning(const QString& title, const QString& message)
{
    QMessageBox::warning(m_parent, title, message);
}

void VideoPlayerWidget::showInfo(const QString& title, const QString& message)
{
    QMessageBox::information(m_parent, title, message);
}

void VideoPlayerWidget::showScreenshotResult(const QString& path)
{
    const QFileInfo fileInfo(path);
    QMessageBox::information(
        m_parent,
        QStringLiteral("截图成功"),
        QStringLiteral("截图已保存到：\n\n%1\n\n保存目录：\n%2")
            .arg(fileInfo.fileName(), fileInfo.absolutePath()));
}

void VideoPlayerWidget::showRecordingStarted(const QString& directory, bool willTranscodeToVideo)
{
    if (m_controlBar) {
        m_controlBar->setRecordProcessing(false);
        m_controlBar->setRecording(true);
    }

    QString message =
        QStringLiteral("\u65e0\u58f0\u753b\u9762\u5f55\u5236\u5df2\u5f00\u59cb\u3002\n\n\u4fdd\u5b58\u4f4d\u7f6e\uff1a\n%1\n\n\u53ea\u6355\u83b7\u753b\u9762\uff0c\u4e0d\u5305\u542b\u97f3\u9891\u3002\n\u518d\u6b21\u70b9\u51fb\u753b\u9762\u5f55\u5236\u6309\u94ae\u53ef\u505c\u6b62\u5f55\u5236\u3002").arg(directory);
    message += willTranscodeToVideo
        ? QStringLiteral("\n\u505c\u6b62\u540e\u5c06\u5f02\u6b65\u8f6c\u6362\u4e3a\u65e0\u58f0 MP4 \u753b\u9762\u89c6\u9891\u3002")
        : QStringLiteral("\n\u5f53\u524d\u672a\u68c0\u6d4b\u5230 FFmpeg\uff0c\u5c06\u4fdd\u7559\u4e3a PNG \u753b\u9762\u5e27\u5e8f\u5217\u3002");

    showInfo(QStringLiteral("\u5f00\u59cb\u753b\u9762\u5f55\u5236"), message);
}

void VideoPlayerWidget::showRecordingResult(const QString& path)
{
    if (m_controlBar) {
        m_controlBar->setRecordProcessing(false);
        m_controlBar->setRecording(false);
    }

    const QFileInfo fileInfo(path);
    if (fileInfo.isFile()) {
        QMessageBox::information(
            m_parent,
            QStringLiteral("\u65e0\u58f0\u753b\u9762\u5f55\u5236\u5b8c\u6210"),
            QStringLiteral("\u65e0\u58f0\u753b\u9762\u89c6\u9891\u5df2\u751f\u6210\uff1a\n\n%1\n\n\u6587\u4ef6\u5927\u5c0f\uff1a%2 MB")
                .arg(fileInfo.absoluteFilePath())
                .arg(QString::number(fileInfo.size() / 1024.0 / 1024.0, 'f', 2)));
        return;
    }

    QMessageBox::information(
        m_parent,
        QStringLiteral("\u753b\u9762\u5f55\u5236\u5b8c\u6210"),
        QStringLiteral("\u753b\u9762\u5e27\u5df2\u4fdd\u5b58\u5230\uff1a\n\n%1").arg(path));
}

void VideoPlayerWidget::showRecordingError(const QString& message)
{
    if (m_controlBar) {
        m_controlBar->setRecordProcessing(false);
        m_controlBar->setRecording(false);
    }

    QMessageBox::warning(m_parent, QStringLiteral("\u5f55\u5236\u5931\u8d25"), message);
}

bool VideoPlayerWidget::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == m_video || watched == m_videoContainer)
        && m_danmakuController
        && m_videoContainer
        && (event->type() == QEvent::Resize
            || event->type() == QEvent::Show
            || event->type() == QEvent::Move)) {
        m_danmakuController->setOverlayGeometry(m_videoContainer->rect());
    }

    return QObject::eventFilter(watched, event);
}

void VideoPlayerWidget::createLayout()
{
    m_mainLayout = new QVBoxLayout(m_parent);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    m_splitter = new QSplitter(Qt::Horizontal, m_parent);
    m_splitter->setHandleWidth(1);
    m_splitter->setStyleSheet(
        "QSplitter::handle { background: rgba(102, 126, 234, 0.3); }"
        "QSplitter::handle:hover { background: rgba(102, 126, 234, 0.6); }");

    m_videoSurface = new VideoRenderContainer(m_parent);
    m_videoContainer = m_videoSurface;
    m_video = m_videoSurface->qtVideoWidget();

    m_danmakuDisplay = new DanmakuPanel(m_parent);
    m_danmakuDisplay->setMinimumWidth(300);

    m_aiChatPanel = new AiChatPanel(m_videoContainer, m_parent, this);
    m_aiChatWidget = m_aiChatPanel->widget();
    m_aiChatWidget->hide();

    m_splitter->addWidget(m_videoContainer);
    m_splitter->addWidget(m_danmakuDisplay);
    m_splitter->addWidget(m_aiChatWidget);
    m_splitter->setStretchFactor(0, 5);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setStretchFactor(2, 0);

    m_mainLayout->addWidget(m_splitter);

    m_controlBar = new VideoControlBar(m_parent);
    m_mainLayout->addWidget(m_controlBar);

    m_danmakuInput = new DanmakuInputBar(m_parent);
    m_mainLayout->addWidget(m_danmakuInput);

    m_parent->setLayout(m_mainLayout);
}

void VideoPlayerWidget::createServices()
{
    m_playbackController = new VideoPlaybackController(this);
    m_playbackController->setVideoOutput(m_video);
#ifdef USE_FFMPEG
    if (m_videoSurface) {
        m_playbackController->setFrameOutput(m_videoSurface->ffmpegVideoWidget());
    }
#endif

    if (!m_historyService) {
        m_historyService = new MediaHistoryService(this);
    }
    m_captureService = new CaptureService(m_videoContainer, this);
    if (m_aiChatPanel) {
        m_aiChatPanel->setCaptureService(m_captureService);
    }
    m_onlineVideoService = new OnlineVideoService(this);
    m_danmakuRepository = new DanmakuRepository();

    m_danmakuWidget = new DanmakuOverlay(m_videoContainer);
    m_danmakuWidget->setGeometry(m_videoContainer->rect());
    m_danmakuWidget->raise();
    m_danmakuWidget->show();
    m_danmakuWidget->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    m_danmakuController = new DanmakuController(m_danmakuRepository,
                                                m_danmakuWidget,
                                                m_danmakuDisplay,
                                                this);

    m_controller = new VideoPlayerController(m_parent,
                                             m_playbackController,
                                             m_historyService,
                                             m_captureService,
                                             m_danmakuController,
                                             m_onlineVideoService,
                                             m_userSession,
                                             this);

    if (m_danmakuInput) {
        m_danmakuInput->setUserSession(m_userSession);
    }

    m_video->installEventFilter(this);
    m_videoContainer->installEventFilter(this);
}

void VideoPlayerWidget::connectSignals()
{
    connect(m_controlBar, &VideoControlBar::playPauseRequested, m_controller, &VideoPlayerController::togglePlayback);
    connect(m_controlBar, &VideoControlBar::screenshotRequested, m_controller, &VideoPlayerController::requestScreenshot);
    connect(m_controlBar, &VideoControlBar::recordRequested, m_controller, &VideoPlayerController::toggleRecording);
    connect(m_controlBar, &VideoControlBar::danmakuToggleRequested, m_controller, &VideoPlayerController::toggleDanmaku);
    connect(m_controlBar, &VideoControlBar::volumeChanged, m_controller, &VideoPlayerController::setVolume);
    connect(m_controlBar, &VideoControlBar::speedChanged, m_controller, &VideoPlayerController::setSpeed);
    connect(m_controlBar, &VideoControlBar::progressJumpRequested, m_controller, &VideoPlayerController::seekToPosition);

    connect(m_controlBar, &VideoControlBar::searchOnlineRequested, this, [this]() {
        m_controller->showOnlineSearchDialog(m_parent);
    });
    connect(m_controlBar, &VideoControlBar::historyRequested, this, [this]() {
        m_controller->showHistoryDialog(m_parent);
    });
    connect(m_controlBar, &VideoControlBar::queueRequested, this, &VideoPlayerWidget::showVideoQueueDialog);
    connect(m_controlBar, &VideoControlBar::myDanmakuRequested, this, [this]() {
        m_controller->showMyDanmakuRecords(m_parent);
    });
    connect(m_controlBar, &VideoControlBar::loginRequested, this, [this]() {
        if (m_authDialogController) {
            m_authDialogController->showUserMenu(m_parent, QCursor::pos());
        }
    });
    connect(m_controlBar, &VideoControlBar::aiPanelToggled, this, &VideoPlayerWidget::updateAiPanelLayout);

    connect(m_controller, &VideoPlayerController::playbackStateChanged, m_controlBar, &VideoControlBar::setPlaying);
    connect(m_controller, &VideoPlayerController::positionChanged, m_controlBar, &VideoControlBar::setProgress);
    connect(m_controller, &VideoPlayerController::durationChanged, m_controlBar, &VideoControlBar::setDuration);
    connect(m_controller, &VideoPlayerController::volumeChanged, m_controlBar, &VideoControlBar::setVolumeValue);
    connect(m_controller, &VideoPlayerController::speedChanged, m_controlBar, &VideoControlBar::setSpeedValue);
    connect(m_controller,
            &VideoPlayerController::previewVideoPathChanged,
            m_controlBar,
            &VideoControlBar::setPreviewVideoPath);
    connect(m_controller, &VideoPlayerController::danmakuEnabledChanged, m_controlBar, &VideoControlBar::setDanmakuEnabled);
    connect(m_controller, &VideoPlayerController::playbackError, this, &VideoPlayerWidget::showError);
    connect(m_controller, &VideoPlayerController::warningRequested, this, &VideoPlayerWidget::showWarning);
    connect(m_controller, &VideoPlayerController::infoRequested, this, &VideoPlayerWidget::showInfo);

#ifdef USE_FFMPEG
    connect(m_playbackController,
            &VideoPlaybackController::backendTypeChanged,
            this,
            [this](VideoPlaybackController::BackendType backendType) {
                if (m_videoSurface == nullptr) {
                    return;
                }

                m_videoSurface->setRenderer(
                    backendType == VideoPlaybackController::BackendType::FFmpeg
                        ? VideoRenderContainer::Renderer::FFmpegVideo
                        : VideoRenderContainer::Renderer::QtVideo);
                if (m_danmakuWidget) {
                    m_danmakuWidget->raise();
                }
                if (m_danmakuController) {
                    m_danmakuController->setOverlayGeometry(m_videoContainer->rect());
                }
            });
#endif

    connect(m_captureService, &CaptureService::screenshotSaved, this, &VideoPlayerWidget::showScreenshotResult);
    connect(m_captureService, &CaptureService::captureFailed, this, [this](const QString& message) {
        showWarning(QStringLiteral("截图失败"), message);
    });
    connect(m_captureService, &CaptureService::recordingStarted, this, &VideoPlayerWidget::showRecordingStarted);
    connect(m_captureService,
            &CaptureService::recordingProcessingChanged,
            m_controlBar,
            &VideoControlBar::setRecordProcessing);
    connect(m_captureService, &CaptureService::recordingFinished, this, &VideoPlayerWidget::showRecordingResult);
    connect(m_captureService, &CaptureService::recordingFailed, this, &VideoPlayerWidget::showRecordingError);

    connect(m_danmakuInput, &DanmakuInputBar::danmakuSubmitted, this, [this](const QString& content, const QString& color, int type) {
        m_controller->sendDanmaku(content, color, type);
    });
    connect(m_danmakuInput, &DanmakuInputBar::loginRequired, m_authService, &AuthService::requestLogin);

    connect(m_authService, &AuthService::loginRequired, this, [this]() {
        if (m_authDialogController) {
            m_authDialogController->showLoginDialog(m_parent);
        }
    });
    connect(m_userSession, &UserSession::sessionChanged, this, [this](const SessionState& state) {
        m_controlBar->setLoggedInUser(state.username);
    });

    connect(m_danmakuController, &DanmakuController::danmakuAdded, m_danmakuWidget, &DanmakuOverlay::showDanmaku);
    connect(m_danmakuController, &DanmakuController::danmakuAdded, m_danmakuDisplay, &DanmakuPanel::appendDanmaku);

    if (m_authService) {
        m_authService->setDanmakuCountProvider([this](const QString& username) {
            return m_danmakuController ? m_danmakuController->userDanmakuCount(username) : 0;
        });
    }

    m_controlBar->setLoggedInUser(m_userSession ? m_userSession->currentUser() : QString());
}

void VideoPlayerWidget::showVideoQueueDialog()
{
    if (!m_controller) {
        return;
    }

    if (!m_queueDialog) {
        m_queueDialog = new VideoQueueDialog(m_controller, m_parent);
    }

    m_queueDialog->show();
    m_queueDialog->raise();
    m_queueDialog->activateWindow();
}

void VideoPlayerWidget::updateAiPanelLayout(bool visible)
{
    if (!m_aiChatWidget || !m_splitter) {
        return;
    }

    m_aiChatWidget->setVisible(visible);

    QList<int> sizes = m_splitter->sizes();
    int total = 0;
    for (int size : sizes) {
        total += size;
    }

    if (!visible) {
        if (m_danmakuDisplay && m_danmakuDisplay->isVisible()) {
            m_splitter->setSizes({total * 80 / 100, total * 20 / 100, 0});
        } else {
            m_splitter->setSizes({total, 0, 0});
        }
        return;
    }

    if (m_danmakuDisplay && m_danmakuDisplay->isVisible()) {
        m_splitter->setSizes({total * 65 / 100, total * 15 / 100, total * 20 / 100});
    } else {
        m_splitter->setSizes({total * 80 / 100, 0, total * 20 / 100});
    }
}
