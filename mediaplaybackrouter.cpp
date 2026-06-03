#include "mediaplaybackrouter.h"

#include "audioplayer.h"
#include "mediaprobeservice.h"
#include "videoplayer.h"

#include <QUrl>

namespace {
bool isOnlineHistoryPath(const QString& filePath)
{
    const QString trimmedPath = filePath.trimmed();
    if (trimmedPath.startsWith(QStringLiteral("online-audio:"), Qt::CaseInsensitive)) {
        return true;
    }

    const QUrl url(trimmedPath);
    const QString scheme = url.scheme().toLower();
    return scheme == QStringLiteral("http") || scheme == QStringLiteral("https");
}
}

MediaPlaybackRouter::MediaPlaybackRouter(AudioPlayer* audioPlayer,
                                         VideoPlayerWidget* videoPlayer,
                                         QObject* parent)
    : QObject(parent)
    , m_audioPlayer(audioPlayer)
    , m_videoPlayer(videoPlayer)
{
}

bool MediaPlaybackRouter::playFromHistory(const MediaHistoryRecord& record)
{
    m_lastError.clear();

    MediaKind kind = mediaKindFromString(record.fileType);
    if (!isOnlineHistoryPath(record.filePath)) {
        const ProbeResult probeResult = MediaProbeService::probeLocalFile(record.filePath);
        if (probeResult.status != ProbeStatus::Supported) {
            const QString reason = probeResult.reason.trimmed().isEmpty()
                ? QStringLiteral("\u6587\u4ef6\u672a\u901a\u8fc7\u64ad\u653e\u524d\u68c0\u67e5")
                : probeResult.reason.trimmed();
            m_lastError = QStringLiteral("\u65e0\u6cd5\u64ad\u653e\u8be5\u5386\u53f2\u8bb0\u5f55\u3002\n\n"
                                         "\u53ef\u80fd\u539f\u56e0\uff1a%1")
                              .arg(reason);
            return false;
        }

        if (probeResult.route == MediaRoute::Audio) {
            kind = MediaKind::Audio;
        } else if (probeResult.route == MediaRoute::Video) {
            kind = MediaKind::Video;
        }
    }

    if (kind == MediaKind::Audio) {
        if (!m_audioPlayer) {
            m_lastError = QStringLiteral("\u97f3\u9891\u64ad\u653e\u5668\u5c1a\u672a\u521d\u59cb\u5316\u3002");
            return false;
        }
        return m_audioPlayer->playFromHistory(record);
    }

    if (kind == MediaKind::Video) {
        if (!m_videoPlayer) {
            m_lastError = QStringLiteral("\u89c6\u9891\u64ad\u653e\u5668\u5c1a\u672a\u521d\u59cb\u5316\u3002");
            return false;
        }
        m_videoPlayer->openAtPosition(record.filePath,
                                      record.isCompleted ? qint64(0) : record.lastPosition);
        return true;
    }

    m_lastError = QStringLiteral("\u65e0\u6cd5\u8bc6\u522b\u8be5\u5386\u53f2\u8bb0\u5f55\u7684\u5a92\u4f53\u7c7b\u578b\u3002");
    return false;
}

QString MediaPlaybackRouter::lastError() const
{
    return m_lastError;
}
