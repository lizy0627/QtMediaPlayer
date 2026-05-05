#include "videodanmakucoordinator.h"

#include "danmakucontroller.h"
#include "mydanmakudialog.h"
#include "usersession.h"

VideoDanmakuCoordinator::VideoDanmakuCoordinator(DanmakuController* danmakuController,
                                                 UserSession* userSession,
                                                 QObject* parent)
    : QObject(parent)
    , m_danmakuController(danmakuController)
    , m_userSession(userSession)
{
    if (m_danmakuController) {
        connect(m_danmakuController,
                &DanmakuController::enabledChanged,
                this,
                &VideoDanmakuCoordinator::danmakuEnabledChanged);
    }
}

void VideoDanmakuCoordinator::loadVideo(const QString& filePath)
{
    if (!m_danmakuController) {
        return;
    }

    m_danmakuController->loadVideo(filePath);
    m_danmakuController->startSync();
}

void VideoDanmakuCoordinator::clearVideo()
{
    if (m_danmakuController) {
        m_danmakuController->clearVideo();
    }
}

void VideoDanmakuCoordinator::syncAfterSeek(qint64 position)
{
    if (!m_danmakuController) {
        return;
    }

    m_danmakuController->resetSyncPosition();
    m_danmakuController->syncToPosition(position);
}

void VideoDanmakuCoordinator::onPlaybackStarted(qint64 position)
{
    if (!m_danmakuController) {
        return;
    }

    m_danmakuController->startSync();
    m_danmakuController->resetSyncPosition(position);
}

void VideoDanmakuCoordinator::onPlaybackStopped()
{
    if (!m_danmakuController) {
        return;
    }

    m_danmakuController->stopSync();
    m_danmakuController->resetSyncPosition();
}

void VideoDanmakuCoordinator::onPlaybackPaused()
{
    if (m_danmakuController) {
        m_danmakuController->stopSync();
    }
}

void VideoDanmakuCoordinator::syncToPosition(qint64 position)
{
    if (m_danmakuController) {
        m_danmakuController->syncFromPlaybackPosition(position);
    }
}

void VideoDanmakuCoordinator::toggleDanmaku(qint64 position)
{
    if (!m_danmakuController) {
        emit warningRequested(QStringLiteral("错误"), QStringLiteral("弹幕系统当前不可用。"));
        return;
    }

    const bool enabled = m_danmakuController->toggleEnabled(position);
    emit danmakuEnabledChanged(enabled);
    emit infoRequested(QStringLiteral("弹幕"),
                       enabled ? QStringLiteral("弹幕显示已开启。")
                               : QStringLiteral("弹幕显示已关闭。"));
}

void VideoDanmakuCoordinator::sendDanmaku(const QString& content,
                                          const QString& color,
                                          int type,
                                          qint64 position)
{
    if (!m_danmakuController) {
        emit warningRequested(QStringLiteral("错误"), QStringLiteral("弹幕系统当前不可用。"));
        return;
    }

    const QString currentUser = m_userSession ? m_userSession->currentUser() : QString();
    if (!m_danmakuController->sendDanmaku(content, color, type, currentUser, position)) {
        const QString errorMessage = m_danmakuController->lastError().isEmpty()
            ? QStringLiteral("弹幕发送失败。")
            : m_danmakuController->lastError();
        emit warningRequested(QStringLiteral("提示"), errorMessage);
    }
}

void VideoDanmakuCoordinator::showMyDanmakuRecords(QWidget* parent,
                                                   const QString& currentVideoPath)
{
    const QString currentUser = m_userSession ? m_userSession->currentUser() : QString();
    if (currentUser.isEmpty()) {
        emit warningRequested(QStringLiteral("提示"), QStringLiteral("请先登录后再查看我的弹幕记录。"));
        return;
    }

    QVector<DanmakuItem> allRecords;
    if (m_danmakuController) {
        allRecords = m_danmakuController->userDanmaku(currentUser);
    }

    MyDanmakuDialog dialog(currentUser, currentVideoPath, allRecords, parent);
    connect(&dialog,
            &MyDanmakuDialog::locateRequested,
            this,
            &VideoDanmakuCoordinator::locateRequested);
    dialog.exec();
}

void VideoDanmakuCoordinator::highlight(qint64 position)
{
    if (m_danmakuController) {
        m_danmakuController->highlight(position);
    }
}
