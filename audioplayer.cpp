#include "audioplayer.h"

#include <QCursor>
#include <QImage>
#include <QPainter>
#include <QVBoxLayout>
#include <QtDebug>

#include "authdialogcontroller.h"
#include "authservice.h"
#include "audiocontrolbar.h"
#include "audiodialogservice.h"
#include "audioplaybackcontroller.h"
#include "audioplayercontroller.h"
#include "audioplayerwidget.h"
#include "lyricpanel.h"
#include "lyricservice.h"
#include "mediahistory.h"
#include "network/onlinemusicservice.h"
#include "playlistmodel.h"
#include "playlistpanel.h"
#include "spectrumpanel.h"
#include "usersession.h"

AudioPlayer::AudioPlayer(QWidget* parent,
                         UserSession* userSession,
                         AuthService* authService,
                         AuthDialogController* authDialogController,
                         MediaHistoryService* mediaHistoryService)
    : QWidget(parent)
    , m_parent(parent)
{
    m_dialogService = new AudioDialogService(this);
    m_playbackController = new AudioPlaybackController(this);
    m_playlistModel = new PlaylistModel(this);
    m_lyricService = new LyricService(this);
    m_mediaHistoryService = mediaHistoryService;
    m_onlineMusicService = new OnlineMusicService(this);
    m_controller = new AudioPlayerController(
        m_playlistModel,
        m_playbackController,
        m_lyricService,
        m_mediaHistoryService,
        m_onlineMusicService,
        this);
    m_userSession = userSession ? userSession : new UserSession(this);
    m_authService = authService ? authService : new AuthService(m_userSession, this);
    m_authDialogController = authDialogController
        ? authDialogController
        : new AuthDialogController(m_authService, this);
    m_authService->initialize();

    m_player = m_playbackController->player();

    createUI();
    setupConnections();
    setDefaultAlbumArt();

    if (m_parent) {
        auto* parentLayout = m_parent->layout();
        if (!parentLayout) {
            parentLayout = new QVBoxLayout(m_parent);
            parentLayout->setContentsMargins(0, 0, 0, 0);
            parentLayout->setSpacing(0);
            m_parent->setLayout(parentLayout);
        }
        parentLayout->addWidget(this);
    }

    m_view->controlBar()->setPlayMode(m_playlistModel->playMode());
    m_view->controlBar()->setPlaying(false);

}

void AudioPlayer::addFiles(const QStringList& files)
{
    m_controller->addLocalFiles(files);
}

bool AudioPlayer::playFromHistory(const MediaHistoryRecord& record)
{
    return m_controller && m_controller->playHistoryRecord(record);
}

void AudioPlayer::audioPause()
{
    m_controller->pause();
}

void AudioPlayer::createUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_view = new AudioPlayerWidget(this);
    m_view->setPlaylistModel(m_playlistModel);
    layout->addWidget(m_view);
}

void AudioPlayer::setupConnections()
{
    auto* playlistPanel = m_view->playlistPanel();
    auto* controlBar = m_view->controlBar();
    auto* lyricPanel = m_view->lyricPanel();
    auto* spectrumPanel = m_view->spectrumPanel();

    connect(m_view, &AudioPlayerWidget::albumArtChangeRequested,
            this, &AudioPlayer::changeAlbumArt);

    connect(playlistPanel, &PlaylistPanel::addFilesRequested,
            this, &AudioPlayer::onAddFiles);
    connect(playlistPanel, &PlaylistPanel::onlineSearchRequested,
            this, &AudioPlayer::onSearchOnline);
    connect(playlistPanel, &PlaylistPanel::deleteSelectedRequested,
            this, &AudioPlayer::deleteSelectedSong);
    connect(playlistPanel, &PlaylistPanel::clearRequested,
            this, &AudioPlayer::clearPlaylist);
    connect(playlistPanel, &PlaylistPanel::testAudioRequested,
            this, &AudioPlayer::testAudio);
    connect(playlistPanel, &PlaylistPanel::loginRequested,
            this, &AudioPlayer::onLoginClicked);
    connect(playlistPanel, &PlaylistPanel::songActivated,
            m_controller, &AudioPlayerController::playAt);

    connect(controlBar, &AudioControlBar::playPauseRequested,
            m_controller, &AudioPlayerController::togglePlayback);
    connect(controlBar, &AudioControlBar::previousRequested,
            m_controller, &AudioPlayerController::playPrevious);
    connect(controlBar, &AudioControlBar::nextRequested,
            m_controller, &AudioPlayerController::playNext);
    connect(controlBar, &AudioControlBar::playModeRequested,
            m_controller, &AudioPlayerController::setPlayMode);
    connect(controlBar, &AudioControlBar::volumeChanged,
            m_controller, &AudioPlayerController::setVolume);
    connect(controlBar, &AudioControlBar::positionRequested,
            m_controller, &AudioPlayerController::setPosition);

    connect(m_player, &QMediaPlayer::positionChanged,
            this, &AudioPlayer::updatePosition);
    connect(m_player, &QMediaPlayer::durationChanged,
            this, &AudioPlayer::updateDuration);
    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this, &AudioPlayer::updatePlayButton);
    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this, [this](QMediaPlayer::PlaybackState state) {
                m_controller->handlePlaybackStateChanged(static_cast<int>(state));
            });
    connect(m_player, &QMediaPlayer::mediaStatusChanged,
            this, [this](QMediaPlayer::MediaStatus status) {
                m_controller->handleMediaStatusChanged(static_cast<int>(status));
            });
    connect(m_player, &QMediaPlayer::errorOccurred,
            this, &AudioPlayer::onPlayerError);

    spectrumPanel->setMediaPlayer(m_player);

    connect(m_player, &QMediaPlayer::positionChanged,
            lyricPanel, &LyricPanel::updatePosition);
    connect(m_lyricService, &LyricService::lyricsReady, this,
            [lyricPanel](const QString&, const QList<LyricLine>& lyrics) {
                lyricPanel->setLyrics(lyrics);
            });
    connect(m_lyricService, &LyricService::lyricsCleared,
            lyricPanel, &LyricPanel::clear);
    connect(m_lyricService, &LyricService::lyricsUnavailable,
            lyricPanel, &LyricPanel::showStatus);

    connect(m_userSession, &UserSession::sessionChanged, this,
            [playlistPanel](const SessionState& state) {
                playlistPanel->setLoggedInUser(state.username);
            });
    playlistPanel->setLoggedInUser(m_userSession->currentUser());

    connect(m_onlineMusicService, &OnlineMusicService::songSelected,
            m_controller, &AudioPlayerController::addOnlineSong);
    connect(m_controller, &AudioPlayerController::warningRequested,
            this, [this](const QString& title, const QString& message) {
                m_dialogService->showWarning(this, title, message);
            });
    connect(m_controller, &AudioPlayerController::informationRequested,
            this, [this](const QString& title, const QString& message) {
                m_dialogService->showInformation(this, title, message);
            });
    connect(m_controller, &AudioPlayerController::volumeAdjusted,
            controlBar, &AudioControlBar::setVolumeValue);
    connect(m_playlistModel, &PlaylistModel::playModeChanged,
            controlBar, &AudioControlBar::setPlayMode);
}

void AudioPlayer::setDefaultAlbumArt()
{
    if (!m_customAlbumArt.isNull()) {
        const QPixmap scaled = m_customAlbumArt.scaled(
            m_view->albumArtSize(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation);
        m_view->setAlbumArtPixmap(scaled);
        return;
    }

    QPixmap pixmap(400, 400);
    pixmap.fill(Qt::darkGray);

    QPainter painter(&pixmap);
    painter.drawImage(pixmap.rect(), QImage(":/assets/disc.png"));
    m_view->setAlbumArtPixmap(pixmap);
}

void AudioPlayer::changeAlbumArt()
{
    const QString fileName = m_dialogService->requestAlbumArtFile(this);
    if (fileName.isEmpty()) {
        return;
    }

    QPixmap pixmap(fileName);
    if (pixmap.isNull()) {
        m_dialogService->showWarning(this, QStringLiteral("错误"), QStringLiteral("无法加载图片文件。"));
        return;
    }

    m_customAlbumArt = pixmap;
    m_customAlbumArtPath = fileName;

    const QPixmap scaled = pixmap.scaled(
        m_view->albumArtSize(),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation);
    m_view->setAlbumArtPixmap(scaled);
    m_view->setAlbumArtToolTip(QStringLiteral("专辑封面已更新\n点击可再次更换"));
}

void AudioPlayer::onAddFiles()
{
    const QStringList files = m_dialogService->requestAudioFiles(this);
    if (!files.isEmpty()) {
        m_controller->addLocalFiles(files);
    }
}

void AudioPlayer::onSearchOnline()
{
    m_onlineMusicService->showSearchDialog(this);
}

void AudioPlayer::testAudio()
{
    m_dialogService->showInformation(this,
                                     QStringLiteral("音频系统诊断"),
                                     m_controller->diagnosticText());
}

void AudioPlayer::deleteSelectedSong()
{
    const int selectedRow = m_view->playlistPanel()->currentRow();
    if (selectedRow < 0) {
        m_dialogService->showWarning(this, QStringLiteral("提示"), QStringLiteral("请先选择要删除的歌曲。"));
        return;
    }

    const QString songName = m_view->playlistPanel()->currentSongName();
    const bool confirmed = m_dialogService->confirm(
        this,
        QStringLiteral("确认删除"),
        QStringLiteral("确定要删除这首歌曲吗？\n\n%1").arg(songName));
    if (!confirmed) {
        return;
    }

    m_controller->removeTrackAt(selectedRow);
}

void AudioPlayer::clearPlaylist()
{
    if (m_playlistModel->isEmpty()) {
        m_dialogService->showInformation(this, QStringLiteral("提示"), QStringLiteral("播放列表已经是空的。"));
        return;
    }

    const bool confirmed = m_dialogService->confirm(
        this,
        QStringLiteral("确认清空"),
        QStringLiteral("确定要清空整个播放列表吗？\n\n共 %1 首歌曲").arg(m_playlistModel->count()));
    if (!confirmed) {
        return;
    }

    m_controller->clearPlaylist();
}

void AudioPlayer::onLoginClicked()
{
    if (m_authDialogController) {
        m_authDialogController->showUserMenu(this, QCursor::pos());
    }
}

void AudioPlayer::updatePlayButton(QMediaPlayer::PlaybackState state)
{
    const bool isPlaying = state == QMediaPlayer::PlayingState;
    m_view->controlBar()->setPlaying(isPlaying);
    m_view->spectrumPanel()->setPlaying(isPlaying);
}

void AudioPlayer::updatePosition(qint64 position)
{
    m_view->controlBar()->setProgress(position);
}

void AudioPlayer::updateDuration(qint64 duration)
{
    m_view->controlBar()->setDuration(duration);
}

void AudioPlayer::onPlayerError(QMediaPlayer::Error error, const QString& errorString)
{
    if (error == QMediaPlayer::NoError) {
        return;
    }

    qWarning() << "Audio player error:" << error << errorString;
    m_controller->handlePlayerError(static_cast<int>(error), errorString);
}
