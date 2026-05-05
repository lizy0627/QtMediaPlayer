#ifndef MEDIAPLAYBACKROUTER_H
#define MEDIAPLAYBACKROUTER_H

#include <QObject>
#include <QString>

#include "mediahistory.h"

class AudioPlayer;
class VideoPlayerWidget;

class MediaPlaybackRouter : public QObject
{
public:
    explicit MediaPlaybackRouter(AudioPlayer* audioPlayer,
                                 VideoPlayerWidget* videoPlayer,
                                 QObject* parent = nullptr);

    bool playFromHistory(const MediaHistoryRecord& record);
    QString lastError() const;

private:
    AudioPlayer* m_audioPlayer = nullptr;
    VideoPlayerWidget* m_videoPlayer = nullptr;
    QString m_lastError;
};

#endif // MEDIAPLAYBACKROUTER_H
