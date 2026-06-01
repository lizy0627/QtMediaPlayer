#include "videohistorycoordinator.h"

#include "mediahistory.h"
#include "unifiedhistorydialog.h"

#include <QMessageBox>
#include <QPushButton>

namespace {
bool isUsefulResumeProgress(const MediaHistoryRecord& record)
{
    if (!record.isValid() || record.isCompleted) {
        return false;
    }

    const int progress = record.progressPercent();
    return progress >= 5 && progress <= 95;
}
}

VideoHistoryCoordinator::VideoHistoryCoordinator(MediaHistoryService* historyService,
                                                 QWidget* viewParent,
                                                 QObject* parent)
    : QObject(parent)
    , m_historyService(historyService)
    , m_viewParent(viewParent)
{
}

void VideoHistoryCoordinator::setCurrentVideo(const QString& filePath)
{
    m_currentVideoPath = filePath;
    m_historyStartRecorded = false;
    m_completionSaved = false;
}

void VideoHistoryCoordinator::clearCurrentVideo()
{
    m_currentVideoPath.clear();
    m_historyStartRecorded = false;
    m_completionSaved = false;
}

void VideoHistoryCoordinator::showHistoryDialog(QWidget* parent)
{
    if (!m_historyService) {
        return;
    }

    UnifiedHistoryDialog historyDialog(m_historyService, parent, MediaKind::Video, true);
    connect(&historyDialog,
            &UnifiedHistoryDialog::playVideo,
            this,
            &VideoHistoryCoordinator::playHistoryRequested);
    historyDialog.exec();
}

void VideoHistoryCoordinator::restoreProgressIfNeeded(QObject* context,
                                                      const QString& filePath,
                                                      const std::function<void(qint64)>& restorePosition)
{
    if (!m_historyService) {
        return;
    }

    Q_UNUSED(context);

    const auto record = m_historyService->recordFor(filePath, MediaKind::Video);
    if (!record.has_value() || !isUsefulResumeProgress(*record)) {
        return;
    }

    QMessageBox msgBox(m_viewParent);
    msgBox.setWindowTitle(QStringLiteral("继续播放"));
    msgBox.setText(QStringLiteral("检测到播放记录"));
    msgBox.setInformativeText(
        QStringLiteral("上次播放到：%1 / %2 (%3%)\n\n是否从上次位置继续播放？")
            .arg(record->positionText())
            .arg(record->durationText())
            .arg(record->progressPercent()));
    msgBox.setIcon(QMessageBox::Question);

    QPushButton* continueBtn = msgBox.addButton(QStringLiteral("继续播放"), QMessageBox::YesRole);
    msgBox.addButton(QStringLiteral("从头播放"), QMessageBox::NoRole);

    msgBox.exec();

    if (msgBox.clickedButton() == continueBtn && restorePosition) {
        restorePosition(record->lastPosition);
    }
}

bool VideoHistoryCoordinator::restoreProgressSilentlyIfUseful(
    const QString& filePath,
    const std::function<void(qint64)>& restorePosition)
{
    if (!m_historyService || !restorePosition) {
        return false;
    }

    const auto record = m_historyService->recordFor(filePath, MediaKind::Video);
    if (!record.has_value() || !isUsefulResumeProgress(*record)) {
        return false;
    }

    restorePosition(record->lastPosition);
    return true;
}

void VideoHistoryCoordinator::maybeMarkPlaybackStarted(qint64 duration)
{
    if (m_historyStartRecorded || !m_historyService || m_currentVideoPath.isEmpty() || duration <= 0) {
        return;
    }

    m_historyService->savePlaybackStart(m_currentVideoPath, MediaKind::Video, duration);
    m_historyStartRecorded = true;
}

void VideoHistoryCoordinator::saveCurrentProgress(qint64 position, qint64 duration)
{
    if (!m_historyService) {
        return;
    }

    if (m_currentVideoPath.isEmpty() || duration <= 0) {
        return;
    }

    if (position > 5000 && position < duration - 5000) {
        m_historyService->savePlaybackProgress(m_currentVideoPath,
                                               MediaKind::Video,
                                               position,
                                               duration);
    }
}

void VideoHistoryCoordinator::saveCompletedProgress(qint64 position, qint64 duration)
{
    if (!m_historyService) {
        return;
    }

    m_historyService->savePlaybackCompleted(m_currentVideoPath,
                                            MediaKind::Video,
                                            position,
                                            duration);
}

bool VideoHistoryCoordinator::completionSaved() const
{
    return m_completionSaved;
}

void VideoHistoryCoordinator::setCompletionSaved(bool saved)
{
    m_completionSaved = saved;
}
