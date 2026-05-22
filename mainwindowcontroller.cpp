#include "mainwindowcontroller.h"

#include "audioplayer.h"
#include "mediafileprobe.h"
#include "mediaplaybackrouter.h"
#include "mediahistory.h"
#include "videoplayer.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QMessageBox>
#include <QProgressDialog>
#include <QStackedWidget>
#include <QStringList>
#include <QWidget>
#include <QtConcurrent>

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
    , m_probeWatcher(new QFutureWatcher<QList<ProbedMediaFile>>(this))
{
    connect(m_probeWatcher,
            &QFutureWatcher<QList<ProbedMediaFile>>::finished,
            this,
            &MainWindowController::handleLocalMediaProbeFinished);
}

MainWindowController::~MainWindowController()
{
    if (m_probeWatcher) {
        disconnect(m_probeWatcher, nullptr, this, nullptr);
        if (m_probeWatcher->isRunning()) {
            m_probeWatcher->cancel();
        }
    }

    closeProbeProgressDialog(true);
    m_probeDialogParent.clear();
    m_probeRunning = false;
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
    if (m_probeRunning) {
        QMessageBox::information(dialogParent,
                                 QStringLiteral("\u63d0\u793a"),
                                 QStringLiteral("\u6b63\u5728\u68c0\u6d4b\u6587\u4ef6\uff0c\u8bf7\u7a0d\u540e"));
        return;
    }

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

    m_probeRunning = true;
    m_probeDialogParent = dialogParent;

    QProgressDialog* progressDialog = new QProgressDialog(dialogParent);
    progressDialog->setWindowTitle(QStringLiteral("\u63d0\u793a"));
    progressDialog->setLabelText(QStringLiteral("\u6b63\u5728\u68c0\u6d4b\u5a92\u4f53\u6587\u4ef6..."));
    progressDialog->setCancelButton(nullptr);
    progressDialog->setRange(0, 0);
    progressDialog->setMinimumDuration(0);
    progressDialog->setAutoClose(false);
    progressDialog->setAutoReset(false);
    progressDialog->show();
    m_probeProgressDialog = progressDialog;

    const auto future = QtConcurrent::run([selectedFiles]() {
        return MediaFileProbe::probeFiles(selectedFiles);
    });
    m_probeWatcher->setFuture(future);
}

void MainWindowController::handleLocalMediaProbeFinished()
{
    const QList<ProbedMediaFile> probedFiles = m_probeWatcher->result();
    m_probeRunning = false;

    closeProbeProgressDialog();

    QWidget* dialogParent = m_probeDialogParent.data();
    m_probeDialogParent.clear();

    QStringList videoFiles;
    QStringList audioFiles;
    QStringList unsupportedFiles;
    int unsupportedIndex = 1;

    for (const ProbedMediaFile& probedFile : probedFiles) {
        const QFileInfo fileInfo(probedFile.filePath);
        if (!probedFile.supported) {
            const QString displayName = fileInfo.fileName().isEmpty() ? probedFile.filePath : fileInfo.fileName();
            const QString reason = probedFile.reason.isEmpty()
                ? QStringLiteral("\u65e0\u6cd5\u8bc6\u522b\u8be5\u5a92\u4f53\u6587\u4ef6")
                : probedFile.reason;
            unsupportedFiles.append(QStringLiteral("%1. %2\uff1a%3")
                                        .arg(unsupportedIndex++)
                                        .arg(displayName, reason));
            continue;
        }

        switch (probedFile.route) {
        case MediaRoute::Video:
            videoFiles.append(probedFile.filePath);
            break;
        case MediaRoute::Audio:
            audioFiles.append(probedFile.filePath);
            break;
        case MediaRoute::Unsupported:
            unsupportedFiles.append(QStringLiteral("%1. %2\uff1a%3")
                                        .arg(unsupportedIndex++)
                                        .arg(fileInfo.fileName(),
                                             QStringLiteral("\u5f53\u524d\u6587\u4ef6\u6269\u5c55\u540d\u4e0d\u5728\u652f\u6301\u5217\u8868\u4e2d\uff1a%1")
                                                 .arg(fileInfo.suffix().toLower())));
            break;
        }
    }

    if (!videoFiles.isEmpty() && m_videoPlayer) {
        showVideoPage();
        m_videoPlayer->open(videoFiles.first());

        if (videoFiles.size() > 1) {
            QStringList ignoredVideoNames;
            ignoredVideoNames.reserve(videoFiles.size() - 1);
            for (int i = 1; i < videoFiles.size(); ++i) {
                const QFileInfo fileInfo(videoFiles.at(i));
                ignoredVideoNames.append(fileInfo.fileName().isEmpty()
                                             ? videoFiles.at(i)
                                             : fileInfo.fileName());
            }

            QMessageBox::information(
                dialogParent,
                QStringLiteral("\u89c6\u9891\u961f\u5217\u63d0\u793a"),
                QStringLiteral("\u5f53\u524d\u53ea\u652f\u6301\u64ad\u653e\u7b2c\u4e00\u4e2a\u89c6\u9891\uff0c\u5176\u4f59\u89c6\u9891\u6682\u672a\u52a0\u5165\u961f\u5217\uff1a\n%1")
                    .arg(ignoredVideoNames.join(QStringLiteral("\n"))));
        }
    }

    if (!audioFiles.isEmpty() && m_audioPlayer) {
        m_audioPlayer->addFiles(audioFiles);
        if (videoFiles.isEmpty()) {
            showAudioPage();
        } else {
            m_audioPlayer->audioPause();
        }
    }

    if (videoFiles.isEmpty() && audioFiles.isEmpty() && unsupportedFiles.isEmpty()) {
        QMessageBox::warning(dialogParent,
                             QStringLiteral("\u63d0\u793a"),
                             QStringLiteral("\u672a\u9009\u62e9\u4efb\u4f55\u6709\u6548\u7684\u5a92\u4f53\u6587\u4ef6\uff01"));
    }

    if (!unsupportedFiles.isEmpty()) {
        QMessageBox::warning(dialogParent,
                             QStringLiteral("\u4ee5\u4e0b\u6587\u4ef6\u65e0\u6cd5\u64ad\u653e"),
                             QStringLiteral("\u4ee5\u4e0b\u6587\u4ef6\u65e0\u6cd5\u64ad\u653e\uff1a\n%1\n\n"
                                            "\u8bf4\u660e\uff1a\u672c\u5730\u6587\u4ef6\u9009\u62e9\u9636\u6bb5\u4ec5\u505a\u5feb\u901f\u8fc7\u6ee4\uff1b"
                                            "\u6269\u5c55\u540d\u652f\u6301\u4e0d\u4ee3\u8868\u7f16\u7801\u4e00\u5b9a\u53ef\u64ad\u653e\u3002")
                                 .arg(unsupportedFiles.join(QStringLiteral("\n"))));
    }
}

void MainWindowController::closeProbeProgressDialog(bool deleteImmediately)
{
    QProgressDialog* progressDialog = m_probeProgressDialog.data();
    if (!progressDialog) {
        m_probeProgressDialog.clear();
        return;
    }

    progressDialog->close();
    m_probeProgressDialog.clear();

    if (deleteImmediately) {
        delete progressDialog;
    } else {
        progressDialog->deleteLater();
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

QStringList MainWindowController::videoExtensions() const
{
    return MediaFileProbe::supportedVideoFormats();
}

QStringList MainWindowController::audioExtensions() const
{
    return MediaFileProbe::supportedAudioFormats();
}

