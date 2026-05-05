#ifndef AUDIOTRACK_H
#define AUDIOTRACK_H

#include <QFileInfo>
#include <QString>
#include <QUrl>

enum class AudioTrackPlaybackStatus
{
    Idle,
    PendingValidation,
    Loading,
    Playing,
    Failed
};

struct AudioTrack
{
    QString id;
    QString sourceId;
    QString sourceName;
    QUrl url;
    QString title;
    QString artist;
    QString album;
    QString sourceUrl;
    QString lyricUrl;
    QString coverPath;
    QString statusMessage;
    int duration = 0;
    bool isLocal = true;
    AudioTrackPlaybackStatus playbackStatus = AudioTrackPlaybackStatus::Idle;

    QString displayText() const
    {
        if (!title.isEmpty() && !artist.isEmpty()) {
            return QStringLiteral("%1 - %2").arg(title, artist);
        }

        if (!title.isEmpty()) {
            return title;
        }

        if (url.isLocalFile()) {
            return QFileInfo(url.toLocalFile()).fileName();
        }

        const QString fileName = url.fileName();
        return fileName.isEmpty() ? url.toString() : fileName;
    }
};

#endif // AUDIOTRACK_H
