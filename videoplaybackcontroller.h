#ifndef VIDEOPLAYBACKCONTROLLER_H
#define VIDEOPLAYBACKCONTROLLER_H

#include <QObject>
#include <QString>

class QAudioOutput;
class QMediaPlayer;
class QUrl;
class QVideoWidget;

class VideoPlaybackController : public QObject
{
    Q_OBJECT

public:
    explicit VideoPlaybackController(QObject* parent = nullptr);

    QMediaPlayer* player() const;
    QAudioOutput* audioOutput() const;

    void setVideoOutput(QVideoWidget* videoOutput);
    void openLocalFile(const QString& filePath);
    void openUrl(const QUrl& url);
    void play();
    void pause();
    void stop();
    void toggle();
    void jump(bool forward, int ms = 5000);

    void setVolume(int volume);
    int volume() const;

    void setSpeed(double speed = 1.0);
    double speed() const;
    bool isPlaying() const;

private:
    QMediaPlayer* m_player = nullptr;
    QAudioOutput* m_audioOutput = nullptr;
    double m_playbackRate = 1.0;
};

#endif // VIDEOPLAYBACKCONTROLLER_H
