#include "captureservice.h"

#include "framecaptureservice.h"
#include "videocapture.h"

CaptureService::CaptureService(QWidget* videoWidget, QObject* parent)
    : QObject(parent)
    , m_capture(new VideoCapture(videoWidget, this))
{
    connect(m_capture, &VideoCapture::screenshotSaved, this, &CaptureService::screenshotSaved);
    connect(m_capture, &VideoCapture::captureFailed, this, &CaptureService::captureFailed);
    connect(m_capture, &VideoCapture::recordingStarted, this, &CaptureService::recordingStarted);
    connect(m_capture,
            &VideoCapture::recordingProcessingChanged,
            this,
            &CaptureService::recordingProcessingChanged);
    connect(m_capture, &VideoCapture::recordingFinished, this, &CaptureService::recordingFinished);
    connect(m_capture, &VideoCapture::recordingFailed, this, &CaptureService::recordingFailed);
}

VideoCapture* CaptureService::capture() const
{
    return m_capture;
}

QString CaptureService::captureScreenshot()
{
    return m_capture->captureScreenshot();
}

QString CaptureService::captureScreenshot(const QString& filePath, qint64 positionMs)
{
    return m_capture->captureScreenshot(filePath, positionMs);
}

bool CaptureService::startRecording()
{
    return m_capture->startRecording();
}

QString CaptureService::stopRecording()
{
    return m_capture->stopRecording();
}

bool CaptureService::isRecording() const
{
    return m_capture->isRecording();
}

bool CaptureService::isProcessingRecording() const
{
    return m_capture->isProcessing();
}

QString CaptureService::saveDirectory() const
{
    return m_capture->getSaveDirectory();
}

QString CaptureService::screenshotDirectory() const
{
    return m_capture->screenshotDirectory();
}

bool CaptureService::isFFmpegAvailable() const
{
    return m_capture->isFFmpegAvailable();
}

QPixmap CaptureService::captureCurrentFrame() const
{
    return m_capture ? m_capture->frameCaptureService()->captureCurrentFrame() : QPixmap();
}

QImage CaptureService::captureVideoFrame(const QString& filePath, qint64 positionMs)
{
    FrameCaptureService* service = frameCaptureService();
    return service ? service->captureVideoFrame(filePath, positionMs) : QImage();
}

QString CaptureService::lastFrameCaptureError() const
{
    FrameCaptureService* service = frameCaptureService();
    return service ? service->lastError() : QStringLiteral("画面捕获服务当前不可用");
}

FrameCaptureService* CaptureService::frameCaptureService() const
{
    return m_capture ? m_capture->frameCaptureService() : nullptr;
}
