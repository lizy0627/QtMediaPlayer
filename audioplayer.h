#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include <QMediaPlayer>
#include <QPixmap>
#include <QStringList>
#include <QWidget>

class AudioPlaybackController;
class AudioDialogService;
class AudioPlayerController;
class AudioPlayerWidget;
class AuthDialogController;
class LyricService;
class MediaHistoryService;
class OnlineMusicService;
class PlaylistModel;
class AuthService;
class UserSession;
struct MediaHistoryRecord;

class AudioPlayer : public QWidget
{
    Q_OBJECT

public:
    explicit AudioPlayer(QWidget* parent = nullptr,
                         UserSession* userSession = nullptr,
                         AuthService* authService = nullptr,
                         AuthDialogController* authDialogController = nullptr,
                         MediaHistoryService* mediaHistoryService = nullptr);

    void addFiles(const QStringList& files);
    bool playFromHistory(const MediaHistoryRecord& record);
    void audioPause();
    bool showMediaInfo();

private:
    void createUI();
    void setupConnections();
    void setDefaultAlbumArt();

private slots:
    void changeAlbumArt();
    void onAddFiles();
    void onSearchOnline();
    void testAudio();
    void deleteSelectedSong();
    void clearPlaylist();
    void onLoginClicked();
    void updatePlayButton(QMediaPlayer::PlaybackState state);
    void updatePosition(qint64 position);
    void updateDuration(qint64 duration);
    void onPlayerError(QMediaPlayer::Error error, const QString& errorString);

private:
    QWidget* m_parent = nullptr;
    AudioPlayerWidget* m_view = nullptr;
    AudioDialogService* m_dialogService = nullptr;
    AudioPlaybackController* m_playbackController = nullptr;
    AudioPlayerController* m_controller = nullptr;
    PlaylistModel* m_playlistModel = nullptr;
    LyricService* m_lyricService = nullptr;
    MediaHistoryService* m_mediaHistoryService = nullptr;
    OnlineMusicService* m_onlineMusicService = nullptr;
    UserSession* m_userSession = nullptr;
    AuthService* m_authService = nullptr;
    AuthDialogController* m_authDialogController = nullptr;
    QMediaPlayer* m_player = nullptr;
    QString m_customAlbumArtPath;
    QPixmap m_customAlbumArt;
};

#endif // AUDIOPLAYER_H
