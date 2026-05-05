#ifndef VIDEODANMAKUCOORDINATOR_H
#define VIDEODANMAKUCOORDINATOR_H

#include <QObject>
#include <QString>

class DanmakuController;
class UserSession;
class QWidget;

class VideoDanmakuCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit VideoDanmakuCoordinator(DanmakuController* danmakuController,
                                     UserSession* userSession,
                                     QObject* parent = nullptr);

    void loadVideo(const QString& filePath);
    void clearVideo();
    void syncAfterSeek(qint64 position);
    void onPlaybackStarted(qint64 position);
    void onPlaybackStopped();
    void onPlaybackPaused();
    void syncToPosition(qint64 position);
    void toggleDanmaku(qint64 position);
    void sendDanmaku(const QString& content,
                     const QString& color,
                     int type,
                     qint64 position);
    void showMyDanmakuRecords(QWidget* parent, const QString& currentVideoPath);
    void highlight(qint64 position);

signals:
    void danmakuEnabledChanged(bool enabled);
    void warningRequested(QString title, QString message);
    void infoRequested(QString title, QString message);
    void locateRequested(QString videoPath, qint64 timestamp);

private:
    DanmakuController* m_danmakuController = nullptr;
    UserSession* m_userSession = nullptr;
};

#endif // VIDEODANMAKUCOORDINATOR_H
