#ifndef VIDEOCONTROLBAR_H
#define VIDEOCONTROLBAR_H

#include <QWidget>
#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QString>
#include <QtGlobal>

class QLabel;
class QPushButton;
class QSlider;
class QComboBox;
class QVBoxLayout;
template <typename T>
class QFutureWatcher;

class VideoControlBar : public QWidget
{
    Q_OBJECT

public:
    explicit VideoControlBar(QWidget* parent = nullptr);

    void setPlaying(bool playing);
    void setRecording(bool recording);
    void setRecordProcessing(bool processing);
    void setLoggedInUser(const QString& username);
    void setDanmakuEnabled(bool enabled);
    void setRetryResolving(bool resolving);
    void setProgress(qint64 position);
    void setDuration(qint64 duration);
    void setVolumeValue(int volume);
    void setSpeedValue(double speed);
    void setPreviewVideoPath(const QString& filePath);

signals:
    void playPauseRequested();
    void searchOnlineRequested();
    void retryOnlineRequested();
    void historyRequested();
    void queueRequested();
    void screenshotRequested();
    void recordRequested();
    void loginRequested();
    void danmakuToggleRequested();
    void aiPanelToggled(bool visible);
    void myDanmakuRequested();
    void volumeChanged(int volume);
    void speedChanged(double speed);
    void progressJumpRequested(qint64 position);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct PreviewFrameResult
    {
        QString filePath;
        qint64 second = -1;
        QImage image;
    };

    QPushButton* createRoundButton(const QString& text, const QString& tooltip, const char* role);
    QVBoxLayout* createButtonGroup(QPushButton* button, const QString& labelText);
    void setButtonRole(QPushButton* button, const char* role);
    void setUserLabelState(const char* state);
    QString formatTime(qint64 milliseconds) const;
    void updateTimeLabel();
    qint64 previewPositionFromSlider(int x) const;
    void requestPreview(qint64 positionMs);
    void startPreviewExtraction(qint64 second);
    void onPreviewExtractionFinished();
    void showPreview(const QPixmap& pixmap);
    void movePreviewToCursor();
    void hidePreview();
    void clearPreviewCache();

    QPushButton* m_btnCtr = nullptr;
    QPushButton* m_btnPlayPause = nullptr;
    QPushButton* m_btnSearchOnline = nullptr;
    QPushButton* m_btnRetryOnline = nullptr;
    QPushButton* m_btnHistory = nullptr;
    QPushButton* m_btnQueue = nullptr;
    QPushButton* m_btnScreenshot = nullptr;
    QPushButton* m_btnRecord = nullptr;
    QPushButton* m_btnLogin = nullptr;
    QPushButton* m_btnDanmaku = nullptr;
    QPushButton* m_btnAiChat = nullptr;
    QPushButton* m_btnMyDanmaku = nullptr;
    QLabel* m_userLabel = nullptr;
    QLabel* m_timeLabel = nullptr;
    QLabel* m_previewLabel = nullptr;
    QSlider* m_volSlider = nullptr;
    QSlider* m_slider = nullptr;
    QComboBox* m_cbRate = nullptr;
    qint64 m_duration = 0;
    qint64 m_position = 0;
    QString m_previewVideoPath;
    QHash<qint64, QPixmap> m_previewCache;
    QFutureWatcher<PreviewFrameResult>* m_previewWatcher = nullptr;
    QPoint m_lastPreviewGlobalPos;
    qint64 m_extractingPreviewSecond = -1;
    qint64 m_queuedPreviewSecond = -1;
    qint64 m_currentPreviewSecond = -1;
    bool m_previewHoverActive = false;
};

#endif // VIDEOCONTROLBAR_H
