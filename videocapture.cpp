#include "videocapture.h"

#include <QDateTime>
#include <QDir>
#include <QImage>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

#include "framecaptureservice.h"
#include "videoencoder.h"

VideoCapture::VideoCapture(QWidget* videoWidget, QObject* parent)
    : QObject(parent)
    , m_videoWidget(videoWidget)
    , m_recordTimer(new QTimer(this))
    , m_frameCaptureService(new FrameCaptureService(videoWidget, this))
    , m_encoder(new VideoEncoder(this))
{
    m_screenshotDirectory = writableMediaDirectory(QStandardPaths::PicturesLocation,
                                                   QStringLiteral("Screenshots"));
    m_recordingRootDirectory = writableMediaDirectory(QStandardPaths::MoviesLocation,
                                                      QStringLiteral("Recordings"));

    m_recordTimer->setInterval(200);
    connect(m_recordTimer, &QTimer::timeout, this, &VideoCapture::captureFrame);
    connect(m_encoder, &VideoEncoder::conversionFinished, this, [this](const QString& outputPath) {
        m_isProcessing = false;
        emit recordingProcessingChanged(false);
        emit recordingFinished(outputPath);
    });
    connect(m_encoder, &VideoEncoder::conversionFailed, this, [this](const QString& outputPath, const QString& message) {
        Q_UNUSED(outputPath)
        m_isProcessing = false;
        emit recordingProcessingChanged(false);
        emit recordingFailed(message);
    });
}

QString VideoCapture::captureScreenshot()
{
    const QPixmap pixmap = captureCurrentFrame();
    if (pixmap.isNull()) {
        emit captureFailed(QStringLiteral("\u65e0\u6cd5\u622a\u53d6\u5f53\u524d\u89c6\u9891\u753b\u9762\u3002"));
        return QString();
    }

    return saveScreenshotImage(pixmap.toImage());
}

QString VideoCapture::captureScreenshot(const QString& filePath, qint64 positionMs)
{
    if (m_frameCaptureService && !filePath.trimmed().isEmpty()) {
        const QImage image = m_frameCaptureService->captureVideoFrame(filePath, positionMs);
        if (!image.isNull()) {
            return saveScreenshotImage(image);
        }
    }

    return captureScreenshot();
}

QString VideoCapture::saveScreenshotImage(const QImage& image)
{
    if (image.isNull()) {
        emit captureFailed(QStringLiteral("\u65e0\u6cd5\u622a\u53d6\u5f53\u524d\u89c6\u9891\u753b\u9762\u3002"));
        return QString();
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    const QString filePath = m_screenshotDirectory + QStringLiteral("/Screenshot_%1.png").arg(timestamp);
    if (!image.save(filePath, "PNG", 100)) {
        emit captureFailed(QStringLiteral("\u622a\u56fe\u4fdd\u5b58\u5931\u8d25\uff0c\u8bf7\u7a0d\u540e\u91cd\u8bd5\u3002"));
        return QString();
    }

    emit screenshotSaved(filePath);
    return filePath;
}

bool VideoCapture::startRecording()
{
    if (m_isRecording || m_isProcessing) {
        return false;
    }

    m_isRecording = true;
    m_frameCount = 0;
    m_recordSessionId = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    m_currentSessionDirectory = m_recordingRootDirectory + QStringLiteral("/Recording_%1").arg(m_recordSessionId);

    if (!QDir().mkpath(m_currentSessionDirectory)) {
        m_isRecording = false;
        emit recordingFailed(QStringLiteral("\u65e0\u6cd5\u521b\u5efa\u753b\u9762\u5f55\u5236\u4fdd\u5b58\u76ee\u5f55\u3002"));
        return false;
    }

    m_recordTimer->start();
    emit recordingStarted(m_currentSessionDirectory, m_encoder->isFFmpegAvailable());
    return true;
}

QString VideoCapture::stopRecording()
{
    if (!m_isRecording) {
        return QString();
    }

    m_isRecording = false;
    m_recordTimer->stop();

    if (m_frameCount == 0) {
        emit recordingFailed(QStringLiteral("\u753b\u9762\u5f55\u5236\u7ed3\u675f\uff0c\u4f46\u6ca1\u6709\u6355\u83b7\u5230\u4efb\u4f55\u753b\u9762\u3002"));
        return QString();
    }

    if (!m_encoder->isFFmpegAvailable()) {
        emit recordingFinished(m_currentSessionDirectory);
        return m_currentSessionDirectory;
    }

    const QString outputPath = m_recordingRootDirectory + QStringLiteral("/Recording_%1.mp4").arg(m_recordSessionId);
    m_isProcessing = true;
    emit recordingProcessingChanged(true);

    if (!m_encoder->startConvertToVideo(m_currentSessionDirectory, outputPath, 5)) {
        if (m_isProcessing) {
            m_isProcessing = false;
            emit recordingProcessingChanged(false);
        }
        return QString();
    }

    return outputPath;
}

bool VideoCapture::isRecording() const
{
    return m_isRecording;
}

bool VideoCapture::isProcessing() const
{
    return m_isProcessing;
}

QString VideoCapture::getSaveDirectory() const
{
    return m_recordingRootDirectory;
}

QString VideoCapture::screenshotDirectory() const
{
    return m_screenshotDirectory;
}

bool VideoCapture::isFFmpegAvailable() const
{
    return m_encoder->isFFmpegAvailable();
}

FrameCaptureService* VideoCapture::frameCaptureService() const
{
    return m_frameCaptureService;
}

void VideoCapture::captureFrame()
{
    if (!m_isRecording) {
        return;
    }

    const QPixmap pixmap = captureCurrentFrame();
    if (pixmap.isNull()) {
        return;
    }

    const QString fileName = QStringLiteral("frame_%1.png").arg(m_frameCount, 5, 10, QChar('0'));
    if (pixmap.save(m_currentSessionDirectory + QLatin1Char('/') + fileName, "PNG", 90)) {
        ++m_frameCount;
    }
}

QString VideoCapture::writableMediaDirectory(QStandardPaths::StandardLocation location, const QString& childDirectory) const
{
    QString basePath = QStandardPaths::writableLocation(location);
    if (basePath.isEmpty()) {
        basePath = QDir::homePath();
    }

    const QString fullPath = basePath + QStringLiteral("/QtMediaPlayer/") + childDirectory;
    QDir().mkpath(fullPath);
    return fullPath;
}

QPixmap VideoCapture::captureCurrentFrame() const
{
    return m_frameCaptureService ? m_frameCaptureService->captureCurrentFrame() : QPixmap();
}
