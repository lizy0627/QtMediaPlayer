#ifndef VIDEOCONTROLBAR_H
#define VIDEOCONTROLBAR_H

#include <QWidget>
#include <QString>
#include <QtGlobal>

class QLabel;
class QPushButton;
class QSlider;
class QComboBox;
class QVBoxLayout;

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
    void setProgress(qint64 position);
    void setDuration(qint64 duration);
    void setVolumeValue(int volume);
    void setSpeedValue(double speed);

signals:
    void playPauseRequested();
    void searchOnlineRequested();
    void historyRequested();
    void screenshotRequested();
    void recordRequested();
    void loginRequested();
    void danmakuToggleRequested();
    void aiPanelToggled(bool visible);
    void myDanmakuRequested();
    void volumeChanged(int volume);
    void speedChanged(double speed);
    void progressJumpRequested(qint64 position);

private:
    QPushButton* createRoundButton(const QString& text, const QString& tooltip, const char* role);
    QVBoxLayout* createButtonGroup(QPushButton* button, const QString& labelText);
    void setButtonRole(QPushButton* button, const char* role);
    void setUserLabelState(const char* state);
    QString formatTime(qint64 milliseconds) const;
    void updateTimeLabel();

    QPushButton* m_btnCtr = nullptr;
    QPushButton* m_btnPlayPause = nullptr;
    QPushButton* m_btnSearchOnline = nullptr;
    QPushButton* m_btnHistory = nullptr;
    QPushButton* m_btnScreenshot = nullptr;
    QPushButton* m_btnRecord = nullptr;
    QPushButton* m_btnLogin = nullptr;
    QPushButton* m_btnDanmaku = nullptr;
    QPushButton* m_btnAiChat = nullptr;
    QPushButton* m_btnMyDanmaku = nullptr;
    QLabel* m_userLabel = nullptr;
    QLabel* m_timeLabel = nullptr;
    QSlider* m_volSlider = nullptr;
    QSlider* m_slider = nullptr;
    QComboBox* m_cbRate = nullptr;
    qint64 m_duration = 0;
    qint64 m_position = 0;
};

#endif // VIDEOCONTROLBAR_H
