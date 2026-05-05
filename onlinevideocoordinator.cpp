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
                &OnlineVideoCoordinator::playbackResolved);
        connect(m_onlineVideoService,
                qOverload<const QString&>(&OnlineVideoService::playbackResolveFailed),
                this,
                &OnlineVideoCoordinator::playbackResolveFailed);
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
    if (m_onlineVideoService) {
        m_onlineVideoService->resolvePlaybackAsync(video);
    }
}

QString OnlineVideoCoordinator::playbackStartedMessage(const OnlinePlaybackRequest& request) const
{
    return m_onlineVideoService ? m_onlineVideoService->playbackStartedMessage(request) : QString();
}
