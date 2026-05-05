#ifndef AUDIOPLAYBACKCONTROLLER_H
#define AUDIOPLAYBACKCONTROLLER_H

#include <QObject>
#include <QUrl>

class QAudioOutput;
class QMediaPlayer;

class AudioPlaybackController : public QObject
{
    Q_OBJECT

public:
    explicit AudioPlaybackController(QObject* parent = nullptr);

    QMediaPlayer* player() const;
    QAudioOutput* audioOutput() const;

    void open(const QUrl& url);
    void play();
    void pause();
    void stop();
    void toggle();
    bool isPlaying() const;

    void setPosition(qint64 position);
    void setVolume(int volume);
    int volume() const;
    void ensureAudioOutput();

private:
    QMediaPlayer* m_player = nullptr;
    QAudioOutput* m_audioOutput = nullptr;
};

#endif // AUDIOPLAYBACKCONTROLLER_H
