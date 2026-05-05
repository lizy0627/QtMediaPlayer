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
    QString playbackStartedMessage(const OnlinePlaybackRequest& request) const;

signals:
    void playbackResolved(const OnlinePlaybackRequest& request);
    void playbackResolveFailed(QString message);

private:
    OnlineVideoService* m_onlineVideoService = nullptr;
};

#endif // ONLINEVIDEOCOORDINATOR_H
