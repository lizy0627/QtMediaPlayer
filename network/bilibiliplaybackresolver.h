#ifndef BILIBILIPLAYBACKRESOLVER_H
#define BILIBILIPLAYBACKRESOLVER_H

#include <QHash>
#include <QObject>
#include <QString>

#include "networkclient.h"
#include "onlinevideotypes.h"

class BilibiliPlaybackResolver : public QObject
{
    Q_OBJECT

public:
    explicit BilibiliPlaybackResolver(QObject* parent = nullptr);

    void resolve(const VideoInfo& video);

signals:
    void playbackResolved(const OnlinePlaybackRequest& request);
    void playbackResolveFailed(const ServiceError& error);

private slots:
    void onRequestFinished(const QString& requestId, const NetworkResult& result);

private:
    enum class PlaybackStage {
        ResolveCid,
        ResolveSingleStream,
        ProbeDash
    };

    struct PendingPlaybackContext
    {
        PlaybackStage stage = PlaybackStage::ResolveCid;
        VideoInfo video;
        QString bvid;
        qint64 cid = 0;
        OnlinePlaybackRequest request;
        QString streamError;
    };

    void startPlaybackRequest(const PendingPlaybackContext& context);
    void handlePlaybackResponse(const QString& requestId, const NetworkResult& result);
    void handleCidResponse(const NetworkResult& result, PendingPlaybackContext context);
    void handleSingleStreamResponse(const NetworkResult& result, PendingPlaybackContext context);
    void handleDashProbeResponse(const NetworkResult& result, PendingPlaybackContext context);
    void emitBrowserOnly(PendingPlaybackContext context,
                         const QString& message,
                         const QString& code);
    void emitFailure(const QString& message, const QString& code);

    static QVariantMap bilibiliHeaders();
    static QString extractBvid(const VideoInfo& video);
    static QString pageUrlForVideo(const VideoInfo& video, const QString& bvid = QString());

    NetworkClient* m_networkClient = nullptr;
    QHash<QString, PendingPlaybackContext> m_pendingPlaybackRequests;
};

#endif // BILIBILIPLAYBACKRESOLVER_H
