#ifndef VIDEOCAPTURECOORDINATOR_H
#define VIDEOCAPTURECOORDINATOR_H

#include <QObject>
#include <QString>

class CaptureService;

class VideoCaptureCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit VideoCaptureCoordinator(CaptureService* captureService, QObject* parent = nullptr);

    void requestScreenshot(bool playbackActive,
                           const QString& filePath = QString(),
                           qint64 positionMs = 0);
    void toggleRecording(bool playbackActive);

    bool isRecording() const;
    bool isProcessingRecording() const;
    QString saveDirectory() const;
    QString screenshotDirectory() const;

signals:
    void warningRequested(QString title, QString message);
    void infoRequested(QString title, QString message);
    void recordingErrorRequested(QString title, QString message);

private:
    CaptureService* m_captureService = nullptr;
};

#endif // VIDEOCAPTURECOORDINATOR_H
