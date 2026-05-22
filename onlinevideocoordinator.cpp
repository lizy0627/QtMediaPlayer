#include "onlinevideocoordinator.h"

#include "onlinevideosearch.h"
#include "onlinevideoservice.h"

OnlineVideoCoordinator::OnlineVideoCoordinator(OnlineVideoService* onlineVideoService,
                                               QObject* parent)
    : QObject(parent)
    , m_onlineVideoService(onlineVideoService)
{
    if (m_onlineVideoService) {
        connect(m_onlineVideoService,
                &OnlineVideoService::playbackResolved,
                this,
                [this](const OnlinePlaybackRequest& request) {
                    m_resolving = false;
                    emit playbackResolved(request);
                });
        connect(m_onlineVideoService,
                qOverload<const QString&>(&OnlineVideoService::playbackResolveFailed),
                this,
                [this](const QString& message) {
                    m_resolving = false;
                    emit playbackResolveFailed(message);
                });
    }
}

void OnlineVideoCoordinator::showSearchDialog(QWidget* parent)
{
    if (!m_onlineVideoService) {
        return;
    }

    OnlineVideoSearch dialog(m_onlineVideoService, parent);
    connect(&dialog,
            &OnlineVideoSearch::videoSelected,
            this,
            &OnlineVideoCoordinator::playOnlineVideo);
    dialog.exec();
}

void OnlineVideoCoordinator::playOnlineVideo(const VideoInfo& video)
{
    m_currentVideo = video;
    m_hasCurrentVideo = true;
    resolveVideo(m_currentVideo);
}

bool OnlineVideoCoordinator::retryCurrentVideo()
{
    if (!m_hasCurrentVideo || m_resolving) {
        return false;
    }

    resolveVideo(m_currentVideo);
    return true;
}

bool OnlineVideoCoordinator::hasCurrentVideo() const
{
    return m_hasCurrentVideo;
}

bool OnlineVideoCoordinator::isResolving() const
{
    return m_resolving;
}

QString OnlineVideoCoordinator::playbackStartedMessage(const OnlinePlaybackRequest& request) const
{
    return m_onlineVideoService ? m_onlineVideoService->playbackStartedMessage(request) : QString();
}

void OnlineVideoCoordinator::resolveVideo(const VideoInfo& video)
{
    if (!m_onlineVideoService) {
        m_resolving = false;
        emit playbackResolveFailed(QStringLiteral("\u65e0\u6cd5\u89e3\u6790\u5728\u7ebf\u89c6\u9891\uff1a\u5728\u7ebf\u89c6\u9891\u670d\u52a1\u672a\u521d\u59cb\u5316\u3002"));
        return;
    }

    m_resolving = true;
    m_onlineVideoService->resolvePlaybackAsync(video);
}
