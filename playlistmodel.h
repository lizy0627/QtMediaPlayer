#ifndef PLAYLISTMODEL_H
#define PLAYLISTMODEL_H

#include <QObject>
#include <QUrl>
#include <QVector>

#include "audiotrack.h"

enum PlaylistPlayMode
{
    PlaylistSingleLoop,
    PlaylistRandom,
    PlaylistListLoop
};

class PlaylistModel : public QObject
{
    Q_OBJECT

public:
    explicit PlaylistModel(QObject* parent = nullptr);

    bool isEmpty() const;
    int count() const;
    int currentIndex() const;
    bool hasCurrent() const;
    AudioTrack currentTrack() const;
    QUrl currentUrl() const;
    AudioTrack at(int index) const;

    void add(const AudioTrack& track);
    bool updateTrack(int index, const AudioTrack& track);
    bool removeAt(int index);
    void clear();
    void setCurrentIndex(int index);
    bool updateTrackPlaybackStatus(int index,
                                   AudioTrackPlaybackStatus status,
                                   const QString& statusMessage = QString());
    bool updateCurrentTrackPlaybackStatus(AudioTrackPlaybackStatus status,
                                          const QString& statusMessage = QString());
    bool moveToPrevious();
    bool moveToNext();

    PlaylistPlayMode playMode() const;
    void setPlayMode(PlaylistPlayMode mode);

signals:
    void changed();
    void currentIndexChanged(int index);
    void playModeChanged(PlaylistPlayMode mode);

private:
    QVector<AudioTrack> m_items;
    int m_currentIndex = -1;
    PlaylistPlayMode m_playMode = PlaylistListLoop;
};

#endif // PLAYLISTMODEL_H
