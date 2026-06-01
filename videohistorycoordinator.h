#ifndef VIDEOHISTORYCOORDINATOR_H
#define VIDEOHISTORYCOORDINATOR_H

#include <QObject>
#include <QString>
#include <functional>

class MediaHistoryService;
class QWidget;

class VideoHistoryCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit VideoHistoryCoordinator(MediaHistoryService* historyService,
                                     QWidget* viewParent,
                                     QObject* parent = nullptr);

    void setCurrentVideo(const QString& filePath);
    void clearCurrentVideo();
    void showHistoryDialog(QWidget* parent);
    void restoreProgressIfNeeded(QObject* context,
                                 const QString& filePath,
                                 const std::function<void(qint64)>& restorePosition);
    bool restoreProgressSilentlyIfUseful(const QString& filePath,
                                         const std::function<void(qint64)>& restorePosition);
    void maybeMarkPlaybackStarted(qint64 duration);
    void saveCurrentProgress(qint64 position, qint64 duration);
    void saveCompletedProgress(qint64 position, qint64 duration);

    bool completionSaved() const;
    void setCompletionSaved(bool saved);

signals:
    void playHistoryRequested(QString filePath, qint64 savedPosition);

private:
    MediaHistoryService* m_historyService = nullptr;
    QWidget* m_viewParent = nullptr;
    QString m_currentVideoPath;
    bool m_historyStartRecorded = false;
    bool m_completionSaved = false;
};

#endif // VIDEOHISTORYCOORDINATOR_H
