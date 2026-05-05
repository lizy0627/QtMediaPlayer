#include "mediaplaybackrouter.h"

#include "audioplayer.h"
#include "videoplayer.h"

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

    const MediaKind kind = mediaKindFromString(record.fileType);
    if (kind == MediaKind::Audio) {
        if (!m_audioPlayer) {
            m_lastError = QStringLiteral("音频播放器尚未初始化。");
            return false;
        }
        return m_audioPlayer->playFromHistory(record);
    }

    if (kind == MediaKind::Video) {
        if (!m_videoPlayer) {
            m_lastError = QStringLiteral("视频播放器尚未初始化。");
            return false;
        }
        m_videoPlayer->openAtPosition(record.filePath,
                                      record.isCompleted ? qint64(0) : record.lastPosition);
        return true;
    }

    m_lastError = QStringLiteral("无法识别该历史记录的媒体类型。");
    return false;
}

QString MediaPlaybackRouter::lastError() const
{
    return m_lastError;
}
