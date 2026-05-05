#ifndef AUDIOCONTROLBAR_H
#define AUDIOCONTROLBAR_H

#include <QGroupBox>
#include <QtGlobal>

#include "playlistmodel.h"

class QLabel;
class QPushButton;
class QSlider;
class QToolButton;

class AudioControlBar : public QGroupBox
{
    Q_OBJECT

public:
    explicit AudioControlBar(QWidget* parent = nullptr);

    void setPlaying(bool playing);
    void setPlayMode(PlaylistPlayMode mode);
    void setProgress(qint64 position);
    void setDuration(qint64 duration);
    void setVolumeValue(int value);

signals:
    void playPauseRequested();
    void previousRequested();
    void nextRequested();
    void playModeRequested(PlaylistPlayMode mode);
    void positionRequested(int position);
    void volumeChanged(int value);

private:
    QToolButton* createModeButton(const QString& text, const QString& tooltip);
    QPushButton* createTransportButton(const QIcon& icon, const QSize& size, const QString& tooltip, bool mainButton);
    QString formatTime(qint64 milliseconds) const;
    void updateVolumeIcon(int value);

    QPushButton* m_btnPlayPause = nullptr;
    QPushButton* m_btnPrev = nullptr;
    QPushButton* m_btnNext = nullptr;
    QToolButton* m_btnLoopList = nullptr;
    QToolButton* m_btnLoopSingle = nullptr;
    QToolButton* m_btnRandom = nullptr;
    QSlider* m_progressSlider = nullptr;
    QLabel* m_currentTime = nullptr;
    QLabel* m_totalTime = nullptr;
    QSlider* m_volumeSlider = nullptr;
    QLabel* m_volumeLabel = nullptr;
    QLabel* m_volumeIcon = nullptr;
};

#endif // AUDIOCONTROLBAR_H
