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

FrameCaptureService* CaptureService::frameCaptureService() const
{
    return m_capture ? m_capture->frameCaptureService() : nullptr;
}
