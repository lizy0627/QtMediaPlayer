#ifndef VIDEOCAPTURE_H
#define VIDEOCAPTURE_H

#include <QObject>
#include <QStandardPaths>
#include <QString>

class QImage;
class QPixmap;
class QTimer;
class FrameCaptureService;
class VideoEncoder;
class QWidget;

class VideoCapture : public QObject
{
    Q_OBJECT

public:
    explicit VideoCapture(QWidget* videoWidget, QObject* parent = nullptr);

    QString captureScreenshot();
    QString captureScreenshot(const QString& filePath, qint64 positionMs);
    bool startRecording();
    QString stopRecording();
    bool isRecording() const;
    bool isProcessing() const;
    QString getSaveDirectory() const;
    QString screenshotDirectory() const;
    bool isFFmpegAvailable() const;
    FrameCaptureService* frameCaptureService() const;

signals:
    void screenshotSaved(QString filePath);
    void captureFailed(QString message);
    void recordingStarted(QString directory, bool willTranscodeToVideo);
    void recordingProcessingChanged(bool processing);
    void recordingFinished(QString path);
    void recordingFailed(QString message);

private slots:
    void captureFrame();

private:
    QString writableMediaDirectory(QStandardPaths::StandardLocation location, const QString& childDirectory) const;
    QPixmap captureCurrentFrame() const;
    QString saveScreenshotImage(const QImage& image);

    QString m_screenshotDirectory;
    QString m_recordingRootDirectory;
    QString m_currentSessionDirectory;
    QString m_recordSessionId;
    QWidget* m_videoWidget = nullptr;
    bool m_isRecording = false;
    bool m_isProcessing = false;
    QTimer* m_recordTimer = nullptr;
    FrameCaptureService* m_frameCaptureService = nullptr;
    int m_frameCount = 0;
    VideoEncoder* m_encoder = nullptr;
};

#endif // VIDEOCAPTURE_H
