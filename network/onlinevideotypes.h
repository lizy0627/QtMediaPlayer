#ifndef ONLINEVIDEOTYPES_H
#define ONLINEVIDEOTYPES_H

#include <QList>
#include <QString>
#include <QUrl>

struct VideoInfo
{
    QString id;
    QString title;
    QString author;
    QString duration;
    QString url;
    QString pageUrl;
    QString description;
    QString thumbnail;
    QString bvid;
    qint64 cid = 0;
    int play = 0;
};

enum class PlaybackResolution
{
    DirectPlayable,
    BrowserOnly,
    Failed
};

struct OnlinePlaybackRequest
{
    PlaybackResolution resolution = PlaybackResolution::Failed;
    bool valid = false;
    QUrl mediaUrl;
    QString title;
    QString author;
    QString pageUrl;
    QString mediaDescription;
    QString errorMessage;
};

Q_DECLARE_METATYPE(VideoInfo)
Q_DECLARE_METATYPE(QList<VideoInfo>)
Q_DECLARE_METATYPE(PlaybackResolution)
Q_DECLARE_METATYPE(OnlinePlaybackRequest)

#endif // ONLINEVIDEOTYPES_H
