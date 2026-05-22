#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QObject>
#include <QString>
#include <QStringList>

class AiChatPanel;
class AiChatWidget;
class AuthDialogController;
class AuthService;
class CaptureService;
class DanmakuController;
class DanmakuInputBar;
class DanmakuOverlay;
class DanmakuPanel;
class DanmakuRepository;
class OnlineVideoService;
class MediaHistoryService;
class QEvent;
class QSplitter;
class QVBoxLayout;
class QVideoWidget;
class UserSession;
class VideoControlBar;
class VideoPlaybackController;
class VideoPlayerController;
class QWidget;

// TODO(video-facade-rename): This type is a QObject facade that builds and wires
// the video page; it is not itself a QWidget. Rename to VideoPlayerFacade in a
// follow-up pass after updating MainWindowController, MediaPlaybackRouter, Widget,
// the VideoPlayer alias, and any Qt meta-object references together.
class VideoPlayerWidget : public QObject
{
    Q_OBJECT

public:
    explicit VideoPlayerWidget(QWidget* parent,
                               UserSession* userSession = nullptr,
                               AuthService* authService = nullptr,
                               AuthDialogController* authDialogController = nullptr,
                               MediaHistoryService* historyService = nullptr);
    ~VideoPlayerWidget() override;

    void open(const QString& filePath, bool localFile = true);
    void openQueue(const QStringList& filePaths);
    void openAtPosition(const QString& filePath, qint64 position);
    void toggle();
    void jump(bool forward, int ms = 5000);
    void setVolume(int volume);
    int volume() const;
    bool isPlaying() const;
    void setSpeed(double speed = 1.0);
    double speed() const;
    void setControlsVisible(bool visible);
    void pause();
    void play();
    void showMyDanmakuRecords();

public slots:
    void showError(const QString& message);
    void showWarning(const QString& title, const QString& message);
    void showInfo(const QString& title, const QString& message);
    void showScreenshotResult(const QString& path);
    void showRecordingStarted(const QString& directory, bool willTranscodeToVideo);
    void showRecordingResult(const QString& path);
    void showRecordingError(const QString& message);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void createLayout();
    void createServices();
    void connectSignals();
    void updateAiPanelLayout(bool visible);

    QWidget* m_parent = nullptr;
    QWidget* m_videoContainer = nullptr;
    VideoControlBar* m_controlBar = nullptr;
    QSplitter* m_splitter = nullptr;
    QVBoxLayout* m_mainLayout = nullptr;
    QVideoWidget* m_video = nullptr;

    UserSession* m_userSession = nullptr;
    AuthService* m_authService = nullptr;
    AuthDialogController* m_authDialogController = nullptr;
    VideoPlaybackController* m_playbackController = nullptr;
    MediaHistoryService* m_historyService = nullptr;
    CaptureService* m_captureService = nullptr;
    OnlineVideoService* m_onlineVideoService = nullptr;
    DanmakuRepository* m_danmakuRepository = nullptr;
    DanmakuController* m_danmakuController = nullptr;
    VideoPlayerController* m_controller = nullptr;

    DanmakuOverlay* m_danmakuWidget = nullptr;
    DanmakuPanel* m_danmakuDisplay = nullptr;
    DanmakuInputBar* m_danmakuInput = nullptr;
    AiChatPanel* m_aiChatPanel = nullptr;
    AiChatWidget* m_aiChatWidget = nullptr;
};

using VideoPlayer = VideoPlayerWidget;

#endif // VIDEOPLAYER_H
