#include "onlinevideoservice.h"

#include "bilibiliplaybackresolver.h"
#include "bilibilisearchservice.h"

OnlineVideoService::OnlineVideoService(QObject* parent)
    : QObject(parent)
    , m_searchService(new BilibiliSearchService(this))
    , m_playbackResolver(new BilibiliPlaybackResolver(this))
{
    qRegisterMetaType<PlaybackResolution>("PlaybackResolution");
    qRegisterMetaType<OnlinePlaybackRequest>("OnlinePlaybackRequest");

    connect(m_searchService,
            &BilibiliSearchService::searchStarted,
            this,
            &OnlineVideoService::searchStarted);
    connect(m_searchService,
            &BilibiliSearchService::searchFinished,
            this,
            &OnlineVideoService::searchFinished);
    connect(m_searchService,
            &BilibiliSearchService::searchFailed,
            this,
            &OnlineVideoService::emitSearchFailure);

    connect(m_playbackResolver,
            &BilibiliPlaybackResolver::playbackResolved,
            this,
            &OnlineVideoService::playbackResolved);
    connect(m_playbackResolver,
            &BilibiliPlaybackResolver::playbackResolveFailed,
            this,
            &OnlineVideoService::emitPlaybackFailure);
}

void OnlineVideoService::searchVideo(const QString& keyword)
{
    m_searchService->searchVideo(keyword);
}

void OnlineVideoService::resolvePlaybackAsync(const VideoInfo& video)
{
    m_playbackResolver->resolve(video);
}

QString OnlineVideoService::playbackStartedMessage(const OnlinePlaybackRequest& request) const
{
    return QStringLiteral("正在播放：%1\n作者：%2\n来源：%3\n\n已将可播放的媒体直链交给 QMediaPlayer。")
        .arg(request.title)
        .arg(request.author)
        .arg(request.mediaDescription.isEmpty() ? QStringLiteral("在线视频直链") : request.mediaDescription);
}

void OnlineVideoService::emitSearchFailure(const ServiceError& error)
{
    emit searchFailed(error);
    emit searchFailed(error.message);
}

void OnlineVideoService::emitPlaybackFailure(const ServiceError& error)
{
    emit playbackResolveFailed(error);
    emit playbackResolveFailed(error.message);
}
