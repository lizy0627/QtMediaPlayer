#include "danmakucontroller.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFileInfo>
#include <algorithm>

#include "danmakuoverlay.h"
#include "danmakupanel.h"
#include "danmakurepository.h"

DanmakuController::DanmakuController(DanmakuRepository* repository,
                                     DanmakuOverlay* overlayWidget,
                                     DanmakuPanel* listDisplay,
                                     QObject* parent)
    : QObject(parent)
    , m_repository(repository)
    , m_overlayWidget(overlayWidget)
    , m_listDisplay(listDisplay)
{
    if (!m_repository) {
        m_lastError = QStringLiteral("danmaku repository is not available");
        return;
    }

    if (!m_repository->ensureReady()) {
        m_lastError = m_repository->lastError();
    }
}

void DanmakuController::setOverlayGeometry(const QRect& geometry)
{
    if (!m_overlayWidget) {
        return;
    }

    m_overlayWidget->setGeometry(geometry);
    m_overlayWidget->raise();
}

void DanmakuController::startSync()
{
    m_syncActive = true;
}

void DanmakuController::stopSync()
{
    m_syncActive = false;
}

void DanmakuController::resetSyncPosition(qint64 position)
{
    m_lastSyncPosition = position;
    m_nextDanmakuIndex = position < 0 ? 0 : findFirstIndexAfter(position);
}

void DanmakuController::loadVideo(const QString& videoPath)
{
    if (!m_repository || videoPath.isEmpty()) {
        return;
    }

    m_currentVideoPath = videoPath;
    m_currentMediaId = mediaIdForVideo(videoPath);
    m_currentDanmakuList = m_repository->getDanmakuList(m_currentMediaId, videoPath);
    std::stable_sort(m_currentDanmakuList.begin(),
                     m_currentDanmakuList.end(),
                     [](const DanmakuItem& left, const DanmakuItem& right) {
                         return left.timestamp < right.timestamp;
                     });
    m_lastSyncPosition = -1;
    m_nextDanmakuIndex = 0;
    m_lastError = m_repository->lastError();

    if (m_listDisplay) {
        m_listDisplay->updateList(m_currentDanmakuList);
        m_listDisplay->setVisible(m_enabled);
    }

    if (m_overlayWidget) {
        m_overlayWidget->clear();
        m_overlayWidget->setEnabled(m_enabled);
        if (!m_enabled) {
            m_overlayWidget->hide();
        }
    }

    emit danmakuLoaded(m_currentDanmakuList.size());
}

void DanmakuController::clearVideo()
{
    m_currentMediaId.clear();
    m_currentVideoPath.clear();
    m_currentDanmakuList.clear();
    m_lastSyncPosition = -1;
    m_nextDanmakuIndex = 0;
    m_syncActive = false;

    if (m_overlayWidget) {
        m_overlayWidget->clear();
    }
    if (m_listDisplay) {
        m_listDisplay->clearPanel();
    }

    emit danmakuCleared();
}

bool DanmakuController::toggleEnabled(qint64 currentPosition)
{
    setEnabled(!m_enabled, currentPosition);
    return m_enabled;
}

void DanmakuController::setEnabled(bool enabled, qint64 currentPosition)
{
    m_enabled = enabled;

    if (m_listDisplay) {
        m_listDisplay->setVisible(m_enabled);
        if (m_enabled) {
            m_listDisplay->highlightByTime(currentPosition);
        }
    }

    if (!m_overlayWidget) {
        emit enabledChanged(m_enabled);
        return;
    }

    m_overlayWidget->setEnabled(m_enabled);
    if (m_enabled) {
        m_overlayWidget->show();
        m_overlayWidget->raise();
        resetSyncPosition();
        syncToPosition(currentPosition);
    } else {
        m_overlayWidget->clear();
        m_overlayWidget->hide();
        resetSyncPosition(currentPosition);
    }

    emit enabledChanged(m_enabled);
}

bool DanmakuController::isEnabled() const
{
    return m_enabled;
}

bool DanmakuController::sendDanmaku(const QString& content,
                                    const QString& color,
                                    int type,
                                    const QString& username,
                                    qint64 position)
{
    const QString trimmedContent = content.trimmed();
    if (username.isEmpty()) {
        m_lastError = QStringLiteral("\u8bf7\u5148\u767b\u5f55\u540e\u518d\u53d1\u9001\u5f39\u5e55\u3002");
        return false;
    }

    if (m_currentVideoPath.isEmpty() || m_currentMediaId.isEmpty()) {
        m_lastError = QStringLiteral("\u8bf7\u5148\u6253\u5f00\u672c\u5730\u89c6\u9891\u3002");
        return false;
    }

    if (trimmedContent.isEmpty()) {
        m_lastError = QStringLiteral("\u5f39\u5e55\u5185\u5bb9\u4e0d\u80fd\u4e3a\u7a7a\u3002");
        return false;
    }

    if (trimmedContent.size() > 100) {
        m_lastError = QStringLiteral("\u5f39\u5e55\u5185\u5bb9\u4e0d\u80fd\u8d85\u8fc7100\u4e2a\u5b57\u7b26\u3002");
        return false;
    }

    DanmakuItem danmaku;
    danmaku.mediaId = m_currentMediaId;
    danmaku.videoPath = m_currentVideoPath;
    danmaku.username = username;
    danmaku.content = trimmedContent;
    danmaku.timestamp = qMax<qint64>(0, position);
    danmaku.color = color;
    danmaku.fontSize = 25;
    danmaku.type = type;
    danmaku.createTime = QDateTime::currentDateTime();

    if (!m_repository || !m_repository->addDanmaku(danmaku)) {
        m_lastError = m_repository ? m_repository->lastError()
                                   : QStringLiteral("\u5f39\u5e55\u6570\u636e\u5e93\u4e0d\u53ef\u7528\u3002");
        return false;
    }

    const int insertIndex = insertDanmakuSorted(danmaku);
    if (insertIndex <= m_nextDanmakuIndex && danmaku.timestamp <= position) {
        ++m_nextDanmakuIndex;
    }

    m_lastError.clear();
    emit danmakuAdded(danmaku);
    return true;
}

void DanmakuController::addDanmaku(const DanmakuItem& item)
{
    emit danmakuAdded(item);
}

void DanmakuController::clear()
{
    clearVideo();
}

void DanmakuController::syncToTime(int ms)
{
    syncToPosition(ms);
}

void DanmakuController::syncFromPlaybackPosition(qint64 currentPosition)
{
    if (!m_syncActive) {
        return;
    }

    syncToPosition(currentPosition);
}

void DanmakuController::syncToPosition(qint64 currentPosition)
{
    if (m_currentVideoPath.isEmpty()) {
        return;
    }

    if (m_listDisplay) {
        m_listDisplay->highlightByTime(currentPosition);
    }

    if (!m_enabled || !m_overlayWidget) {
        m_lastSyncPosition = currentPosition;
        m_nextDanmakuIndex = findFirstIndexAfter(currentPosition);
        return;
    }

    showOverlayIfNeeded();

    int startIndex = m_nextDanmakuIndex;
    if (m_lastSyncPosition < 0
        || currentPosition < m_lastSyncPosition
        || currentPosition - m_lastSyncPosition > 3000) {
        m_overlayWidget->clear();
        m_lastSyncPosition = qMax<qint64>(0, currentPosition - 200);
        startIndex = findFirstIndexAfter(m_lastSyncPosition);
    }

    while (startIndex < m_currentDanmakuList.size()
           && m_currentDanmakuList[startIndex].timestamp <= currentPosition) {
        m_overlayWidget->showDanmaku(m_currentDanmakuList[startIndex]);
        ++startIndex;
    }

    m_nextDanmakuIndex = startIndex;
    m_lastSyncPosition = currentPosition;
}

void DanmakuController::highlight(qint64 position)
{
    if (m_listDisplay) {
        m_listDisplay->highlightByTime(position);
    }
}

QVector<DanmakuItem> DanmakuController::userDanmaku(const QString& username) const
{
    if (!m_repository || username.isEmpty()) {
        return {};
    }

    return m_repository->getUserDanmaku(username);
}

int DanmakuController::userDanmakuCount(const QString& username) const
{
    if (!m_repository || username.isEmpty()) {
        return 0;
    }

    return m_repository->getUserDanmakuCount(username);
}

QString DanmakuController::lastError() const
{
    return m_lastError;
}

QString DanmakuController::mediaIdForVideo(const QString& videoPath) const
{
    const QFileInfo fileInfo(videoPath);
    const QString path = fileInfo.exists()
        ? fileInfo.canonicalFilePath()
        : QFileInfo(videoPath).absoluteFilePath();
    const QByteArray seed = QStringLiteral("%1|%2|%3")
                                .arg(path)
                                .arg(fileInfo.exists() ? fileInfo.size() : 0)
                                .arg(fileInfo.exists()
                                         ? fileInfo.lastModified().toUTC().toMSecsSinceEpoch()
                                         : 0)
                                .toUtf8();

    return QString::fromLatin1(QCryptographicHash::hash(seed, QCryptographicHash::Sha256).toHex());
}

int DanmakuController::findFirstIndexAfter(qint64 timestamp) const
{
    const auto it = std::upper_bound(
        m_currentDanmakuList.cbegin(),
        m_currentDanmakuList.cend(),
        timestamp,
        [](qint64 value, const DanmakuItem& item) {
            return value < item.timestamp;
        });
    return static_cast<int>(it - m_currentDanmakuList.cbegin());
}

int DanmakuController::insertDanmakuSorted(const DanmakuItem& item)
{
    const int insertIndex = findFirstIndexAfter(item.timestamp);
    m_currentDanmakuList.insert(insertIndex, item);
    return insertIndex;
}

void DanmakuController::showOverlayIfNeeded()
{
    if (!m_overlayWidget || m_overlayWidget->isVisible()) {
        return;
    }

    m_overlayWidget->show();
    m_overlayWidget->raise();
}
