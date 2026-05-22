#ifndef AUDIOPLAYERCONTROLLER_H
#define AUDIOPLAYERCONTROLLER_H

#include <QObject>
#include <QStringList>

#include "playlistmodel.h"

class AudioPlaybackController;
class LyricService;
class MediaHistoryService;
class OnlineMusicService;
struct SongInfo;
struct MediaHistoryRecord;

class AudioPlayerController : public QObject
{
    Q_OBJECT

public:
    explicit AudioPlayerController(PlaylistModel* playlistModel,
                                   AudioPlaybackController* playbackController,
                                   LyricService* lyricService,
                                   MediaHistoryService* mediaHistoryService,
                                   OnlineMusicService* onlineMusicService,
                                   QObject* parent = nullptr);

    PlaylistModel* playlistModel() const;

    void addLocalFiles(const QStringList& files);
    void addOnlineSong(const SongInfo& song);
    bool playHistoryRecord(const MediaHistoryRecord& record);
    bool removeTrackAt(int index);
    void clearPlaylist();

    void play();
    void pause();
    void togglePlayback();
    void playAt(int index);
    void playPrevious();
    void playNext();
    void setPlayMode(PlaylistPlayMode mode);
    void setPosition(qint64 position);
    void setVolume(int volume);
    void handleMediaStatusChanged(int status);
    void handlePlaybackStateChanged(int state);
    void handlePlayerError(int error, const QString& errorString);
    QString diagnosticText() const;

private slots:
    void onOnlineSongUrlResolved(const SongInfo& song);
    void onOnlineSongUrlResolveError(const QString& songId, const QString& message);

signals:
    void warningRequested(const QString& title, const QString& message);
    void informationRequested(const QString& title, const QString& message);
    void volumeAdjusted(int value);

private:
    void syncLyricsForCurrentTrack();
    void ensureCurrentTrackSelected();
    void markCurrentTrackPlaybackStarted();
    void markCurrentTrackFailed(const QString& message);
    bool currentTrackNeedsOnlineResolve(const AudioTrack& track) const;
    void resolveCurrentOnlineTrack();
    void clearPendingSeek();
    qint64 resolvedPendingSeekPosition(qint64 requestedPosition) const;
    void schedulePendingSeek(qint64 position);
    void tryApplyPendingSeek(int status);
    QString historyKeyForTrack(const AudioTrack& track) const;
    bool populateTrackFromOnlineHistory(const QString& historyPath,
                                        const MediaHistoryRecord& record,
                                        AudioTrack* track) const;

    PlaylistModel* m_playlistModel = nullptr;
    AudioPlaybackController* m_playbackController = nullptr;
    LyricService* m_lyricService = nullptr;
    MediaHistoryService* m_mediaHistoryService = nullptr;
    OnlineMusicService* m_onlineMusicService = nullptr;
    QString m_lastHistoryStartKey;
    qint64 m_pendingSeekPosition = -1;
    int m_pendingResolveIndex = -1;
    QString m_pendingResolveSourceId;
};

#endif // AUDIOPLAYERCONTROLLER_H
