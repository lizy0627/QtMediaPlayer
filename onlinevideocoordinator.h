#ifndef ONLINEVIDEOCOORDINATOR_H
#define ONLINEVIDEOCOORDINATOR_H

#include <QObject>
#include <QString>

#include "onlinevideoservice.h"

class QWidget;

class OnlineVideoCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit OnlineVideoCoordinator(OnlineVideoService* onlineVideoService,
                                    QObject* parent = nullptr);

    void showSearchDialog(QWidget* parent);
    void playOnlineVideo(const VideoInfo& video);
    bool retryCurrentVideo();
    bool hasCurrentVideo() const;
    bool isResolving() const;
    QString playbackStartedMessage(const OnlinePlaybackRequest& request) const;

signals:
    void playbackResolved(const OnlinePlaybackRequest& request);
    void playbackResolveFailed(QString message);

private:
    void resolveVideo(const VideoInfo& video);

    OnlineVideoService* m_onlineVideoService = nullptr;
    VideoInfo m_currentVideo;
    bool m_hasCurrentVideo = false;
    bool m_resolving = false;
};

#endif // ONLINEVIDEOCOORDINATOR_H
