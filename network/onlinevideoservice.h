#ifndef ONLINEVIDEOSERVICE_H
#define ONLINEVIDEOSERVICE_H

#include <QObject>
#include <QList>
#include <QString>

#include "networkclient.h"
#include "onlinevideotypes.h"

class BilibiliPlaybackResolver;
class BilibiliSearchService;

class OnlineVideoService : public QObject
{
    Q_OBJECT

public:
    explicit OnlineVideoService(QObject* parent = nullptr);

    void searchVideo(const QString& keyword);
    void resolvePlaybackAsync(const VideoInfo& video);
    QString playbackStartedMessage(const OnlinePlaybackRequest& request) const;

signals:
    void searchStarted(const QString& keyword);
    void searchFinished(const QList<VideoInfo>& videos, const QString& statusMessage);
    void searchFailed(const ServiceError& error);
    void searchFailed(const QString& message);
    void playbackResolved(const OnlinePlaybackRequest& request);
    void playbackResolveFailed(const ServiceError& error);
    void playbackResolveFailed(const QString& message);

private:
    void emitSearchFailure(const ServiceError& error);
    void emitPlaybackFailure(const ServiceError& error);

    BilibiliSearchService* m_searchService = nullptr;
    BilibiliPlaybackResolver* m_playbackResolver = nullptr;
};

#endif // ONLINEVIDEOSERVICE_H
