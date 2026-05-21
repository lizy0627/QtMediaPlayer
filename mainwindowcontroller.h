#ifndef MAINWINDOWCONTROLLER_H
#define MAINWINDOWCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>

class AudioPlayer;
class MediaPlaybackRouter;
class MediaHistoryService;
class QStackedWidget;
class QWidget;
class VideoPlayerWidget;
struct MediaHistoryRecord;

class MainWindowController : public QObject
{
    Q_OBJECT

public:
    explicit MainWindowController(QStackedWidget* pages,
                                  QWidget* videoPage,
                                  QWidget* audioPage,
                                  VideoPlayerWidget* videoPlayer,
                                  AudioPlayer* audioPlayer,
                                  MediaHistoryService* historyService,
                                  QObject* parent = nullptr);

    void showVideoPage();
    void showAudioPage();
    void openLocalMediaFiles(QWidget* dialogParent);
    void playFromHistory(const MediaHistoryRecord& record);
    void playAudioFromHistory(const QString& filePath);
    void playVideoFromHistory(const QString& filePath, qint64 position);

private:
    QStringList videoExtensions() const;
    QStringList audioExtensions() const;

    QStackedWidget* m_pages = nullptr;
    QWidget* m_videoPage = nullptr;
    QWidget* m_audioPage = nullptr;
    VideoPlayerWidget* m_videoPlayer = nullptr;
    AudioPlayer* m_audioPlayer = nullptr;
    MediaHistoryService* m_historyService = nullptr;
    MediaPlaybackRouter* m_playbackRouter = nullptr;
};

#endif // MAINWINDOWCONTROLLER_H
