#ifndef CAPTURESERVICE_H
#define CAPTURESERVICE_H

#include <QObject>
#include <QPixmap>
#include <QString>

class FrameCaptureService;
class VideoCapture;
class QWidget;

class CaptureService : public QObject
{
    Q_OBJECT

public:
    explicit CaptureService(QWidget* videoWidget, QObject* parent = nullptr);

    VideoCapture* capture() const;
    QString captureScreenshot();
    QString captureScreenshot(const QString& filePath, qint64 positionMs);
    bool startRecording();
    QString stopRecording();
    bool isRecording() const;
    bool isProcessingRecording() const;
    QString saveDirectory() const;
    QString screenshotDirectory() const;
    bool isFFmpegAvailable() const;
    QPixmap captureCurrentFrame() const;
    FrameCaptureService* frameCaptureService() const;

signals:
    void screenshotSaved(QString filePath);
    void captureFailed(QString message);
    void recordingStarted(QString directory, bool willTranscodeToVideo);
    void recordingProcessingChanged(bool processing);
    void recordingFinished(QString path);
    void recordingFailed(QString message);

private:
    VideoCapture* m_capture = nullptr;
};

#endif // CAPTURESERVICE_H
