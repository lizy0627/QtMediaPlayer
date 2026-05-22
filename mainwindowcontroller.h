#ifndef MAINWINDOWCONTROLLER_H
#define MAINWINDOWCONTROLLER_H

#include "mediafileprobe.h"

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

class AudioPlayer;
class MediaPlaybackRouter;
class MediaHistoryService;
class QProgressDialog;
class QStackedWidget;
class QWidget;
class VideoPlayerWidget;
template <typename T>
class QFutureWatcher;
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
    ~MainWindowController() override;

    void showVideoPage();
    void showAudioPage();
    void openLocalMediaFiles(QWidget* dialogParent);
    void playFromHistory(const MediaHistoryRecord& record);
    void playAudioFromHistory(const QString& filePath);
    void playVideoFromHistory(const QString& filePath, qint64 position);

private:
    QStringList videoExtensions() const;
    QStringList audioExtensions() const;
    void handleLocalMediaProbeFinished();
    void closeProbeProgressDialog(bool deleteImmediately = false);

    QStackedWidget* m_pages = nullptr;
    QWidget* m_videoPage = nullptr;
    QWidget* m_audioPage = nullptr;
    VideoPlayerWidget* m_videoPlayer = nullptr;
    AudioPlayer* m_audioPlayer = nullptr;
    MediaHistoryService* m_historyService = nullptr;
    MediaPlaybackRouter* m_playbackRouter = nullptr;
    QFutureWatcher<QList<ProbedMediaFile>>* m_probeWatcher = nullptr;
    QPointer<QWidget> m_probeDialogParent;
    QPointer<QProgressDialog> m_probeProgressDialog;
    bool m_probeRunning = false;
};

#endif // MAINWINDOWCONTROLLER_H
