#include "mainwindowcontroller.h"

#include "audioplayer.h"
#include "mediaplaybackrouter.h"
#include "mediahistory.h"
#include "mediaprobeservice.h"
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
    const QString videoPattern = QStringLiteral("*.") + videoExtensions().join(QStringLiteral(" *."));
    const QString audioPattern = QStringLiteral("*.") + audioExtensions().join(QStringLiteral(" *."));

    QFileDialog fileDialog(dialogParent);
    fileDialog.setDirectory(QDir::homePath());
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    fileDialog.setNameFilter(
        QStringLiteral("\u5a92\u4f53\u6587\u4ef6 (%1 %2);;"
                       "\u89c6\u9891\u6587\u4ef6 (%1);;"
                       "\u97f3\u9891\u6587\u4ef6 (%2);;"
                       "\u6240\u6709\u6587\u4ef6 (*.*)")
            .arg(videoPattern, audioPattern));

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
        const ProbeResult probeResult = MediaProbeService::probeLocalFile(filePath);
        if (probeResult.status != ProbeStatus::Supported) {
            const QString displayName = fileInfo.fileName().isEmpty() ? filePath : fileInfo.fileName();
            unsupportedFiles.append(QStringLiteral("%1: %2").arg(displayName, probeResult.reason));
            continue;
        }

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
            QStringLiteral("\u4e0d\u652f\u6301\u64ad\u653e\u8be5\u6587\u4ef6"),
            QStringLiteral("\u4e0d\u652f\u6301\u64ad\u653e\u4ee5\u4e0b\u6587\u4ef6\uff0c\u5df2\u963b\u6b62\u64ad\u653e\uff1a\n\n%1\n\n"
                           "\u652f\u6301\u7684\u97f3\u9891\u683c\u5f0f\uff1a\n%2\n\n"
                           "\u652f\u6301\u7684\u89c6\u9891\u683c\u5f0f\uff1a\n%3")
                .arg(unsupportedFiles.join(QStringLiteral("\n")),
                     audioExtensions().join(QStringLiteral(", ")),
                     videoExtensions().join(QStringLiteral(", "))));
        return;
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
    return MediaProbeService::supportedVideoFormats();
}

QStringList MainWindowController::audioExtensions() const
{
    return MediaProbeService::supportedAudioFormats();
}

