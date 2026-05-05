#include "videocapturecoordinator.h"

#include "captureservice.h"

VideoCaptureCoordinator::VideoCaptureCoordinator(CaptureService* captureService, QObject* parent)
    : QObject(parent)
    , m_captureService(captureService)
{
}

void VideoCaptureCoordinator::requestScreenshot(bool playbackActive)
{
    if (!m_captureService) {
        emit warningRequested(QStringLiteral("提示"), QStringLiteral("截图服务当前不可用。"));
        return;
    }

    if (!playbackActive) {
        emit warningRequested(QStringLiteral("提示"), QStringLiteral("请先播放视频后再截图。"));
        return;
    }

    m_captureService->captureScreenshot();
}

void VideoCaptureCoordinator::toggleRecording(bool playbackActive)
{
    if (!m_captureService) {
        emit recordingErrorRequested(QStringLiteral("提示"), QStringLiteral("\u753b\u9762\u5f55\u5236\u670d\u52a1\u5f53\u524d\u4e0d\u53ef\u7528\u3002"));
        return;
    }

    if (m_captureService->isProcessingRecording()) {
        emit infoRequested(QStringLiteral("\u5904\u7406\u4e2d"), QStringLiteral("\u6b63\u5728\u5904\u7406\u65e0\u58f0\u753b\u9762\u5f55\u5236\u5185\u5bb9\uff0c\u8bf7\u7a0d\u5019\u3002"));
        return;
    }

    if (!m_captureService->isRecording()) {
        if (!playbackActive) {
            emit warningRequested(QStringLiteral("提示"), QStringLiteral("\u8bf7\u5148\u64ad\u653e\u89c6\u9891\u540e\u518d\u5f00\u59cb\u753b\u9762\u5f55\u5236\u3002"));
            return;
        }

        if (!m_captureService->startRecording()) {
            emit recordingErrorRequested(QStringLiteral("提示"), QStringLiteral("\u5f00\u59cb\u753b\u9762\u5f55\u5236\u5931\u8d25\uff0c\u8bf7\u7a0d\u540e\u91cd\u8bd5\u3002"));
        }
        return;
    }

    m_captureService->stopRecording();
}

bool VideoCaptureCoordinator::isRecording() const
{
    return m_captureService && m_captureService->isRecording();
}

bool VideoCaptureCoordinator::isProcessingRecording() const
{
    return m_captureService && m_captureService->isProcessingRecording();
}

QString VideoCaptureCoordinator::saveDirectory() const
{
    return m_captureService ? m_captureService->saveDirectory() : QString();
}

QString VideoCaptureCoordinator::screenshotDirectory() const
{
    return m_captureService ? m_captureService->screenshotDirectory() : QString();
}
