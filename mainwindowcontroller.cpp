#include "mainwindowcontroller.h"

#include "audioplayer.h"
#include "mediaplaybackrouter.h"
#include "mediahistory.h"
#include "videoplayer.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QStackedWidget>
#include <QStringList>
#include <QWidget>

MainWindowController::MainWindowController(QStackedWidget* pages,
                                           QWidget* videoPage,
                                           QWidget* audioPage,
                                           VideoPlayerWidget* videoPlayer,
                                           AudioPlayer* audioPlayer,
                                           MediaHistoryService* historyService,
                                           QObject* parent)
    : QObject(parent)
    , m_pages(pages)
    , m_videoPage(videoPage)
    , m_audioPage(audioPage)
    , m_videoPlayer(videoPlayer)
    , m_audioPlayer(audioPlayer)
    , m_historyService(historyService)
    , m_playbackRouter(new MediaPlaybackRouter(audioPlayer, videoPlayer, this))
{
}

void MainWindowController::showVideoPage()
{
    if (m_pages && m_videoPage) {
        m_pages->setCurrentWidget(m_videoPage);
    }
    if (m_audioPlayer) {
        m_audioPlayer->audioPause();
    }
}

void MainWindowController::showAudioPage()
{
    if (m_pages && m_audioPage) {
        m_pages->setCurrentWidget(m_audioPage);
    }
    if (m_videoPlayer) {
        m_videoPlayer->pause();
    }
}

void MainWindowController::openLocalMediaFiles(QWidget* dialogParent)
{
    QFileDialog fileDialog(dialogParent);
    fileDialog.setDirectory(QDir::homePath());
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    fileDialog.setNameFilter(QStringLiteral(
        "媒体文件 (*.mp4 *.avi *.mkv *.mov *.flv *.wmv *.webm *.m4v *.mpg *.mpeg *.3gp *.ts *.mp3 *.wav *.flac *.ogg *.m4a *.aac);;"
        "视频文件 (*.mp4 *.avi *.mkv *.mov *.flv *.wmv *.webm *.m4v *.mpg *.mpeg *.3gp *.ts);;"
        "音频文件 (*.mp3 *.wav *.flac *.ogg *.m4a *.aac);;"
        "所有文件 (*.*)"));

    if (!fileDialog.exec()) {
        return;
    }

    const QStringList selectedFiles = fileDialog.selectedFiles();
    if (selectedFiles.isEmpty()) {
        return;
    }

    QStringList videoFiles;
    QStringList audioFiles;
    QStringList unsupportedFiles;

    for (const QString& filePath : selectedFiles) {
        const QFileInfo fileInfo(filePath);
        switch (routeForFile(filePath)) {
        case MediaRoute::Video:
            videoFiles.append(filePath);
            break;
        case MediaRoute::Audio:
            audioFiles.append(filePath);
            break;
        case MediaRoute::Unsupported:
            unsupportedFiles.append(QStringLiteral("%1 (.%2)")
                                        .arg(fileInfo.fileName(), fileInfo.suffix().toLower()));
            break;
        }
    }

    if (!videoFiles.isEmpty() && m_videoPlayer) {
        showVideoPage();
        m_videoPlayer->open(videoFiles.first());
    }

    if (!audioFiles.isEmpty() && m_audioPlayer) {
        m_audioPlayer->addFiles(audioFiles);
        if (videoFiles.isEmpty()) {
            showAudioPage();
        } else {
            m_audioPlayer->audioPause();
        }
    }

    if (!unsupportedFiles.isEmpty()) {
        QMessageBox::warning(
            dialogParent,
            QStringLiteral("格式过滤提示"),
            QStringLiteral("以下文件格式暂不支持，已自动过滤：\n\n%1\n\n支持的视频格式：\n%2\n\n支持的音频格式：\n%3")
                .arg(unsupportedFiles.join(QStringLiteral("\n")),
                     videoExtensions().join(QStringLiteral(", ")),
                     audioExtensions().join(QStringLiteral(", "))));
    } else if (videoFiles.isEmpty() && audioFiles.isEmpty()) {
        QMessageBox::warning(dialogParent,
                             QStringLiteral("提示"),
                             QStringLiteral("未选择任何有效的媒体文件！"));
    }
}

void MainWindowController::playAudioFromHistory(const QString& filePath)
{
    MediaHistoryRecord record;
    record.filePath = filePath;
    record.fileType = mediaKindToString(MediaKind::Audio);
    playFromHistory(record);
}

void MainWindowController::playVideoFromHistory(const QString& filePath, qint64 position)
{
    MediaHistoryRecord record;
    record.filePath = filePath;
    record.fileType = mediaKindToString(MediaKind::Video);
    record.lastPosition = position;
    playFromHistory(record);
}

void MainWindowController::playFromHistory(const MediaHistoryRecord& record)
{
    const MediaKind kind = mediaKindFromString(record.fileType);
    if (kind == MediaKind::Audio) {
        showAudioPage();
    } else if (kind == MediaKind::Video) {
        showVideoPage();
    }

    if (!m_playbackRouter || m_playbackRouter->playFromHistory(record)) {
        return;
    }

    QWidget* dialogParent = qobject_cast<QWidget*>(parent());
    QMessageBox::warning(dialogParent,
                         QStringLiteral("播放历史"),
                         m_playbackRouter->lastError().isEmpty()
                             ? QStringLiteral("无法播放该历史记录。")
                             : m_playbackRouter->lastError());
}

MainWindowController::MediaRoute MainWindowController::routeForFile(const QString& filePath) const
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (videoExtensions().contains(suffix)) {
        return MediaRoute::Video;
    }
    if (audioExtensions().contains(suffix)) {
        return MediaRoute::Audio;
    }
    return MediaRoute::Unsupported;
}

QStringList MainWindowController::videoExtensions() const
{
    return {
        QStringLiteral("mp4"),
        QStringLiteral("avi"),
        QStringLiteral("mkv"),
        QStringLiteral("mov"),
        QStringLiteral("flv"),
        QStringLiteral("wmv"),
        QStringLiteral("webm"),
        QStringLiteral("m4v"),
        QStringLiteral("mpg"),
        QStringLiteral("mpeg"),
        QStringLiteral("3gp"),
        QStringLiteral("ts")
    };
}

QStringList MainWindowController::audioExtensions() const
{
    return {
        QStringLiteral("mp3"),
        QStringLiteral("wav"),
        QStringLiteral("flac"),
        QStringLiteral("ogg"),
        QStringLiteral("m4a"),
        QStringLiteral("aac")
    };
}
