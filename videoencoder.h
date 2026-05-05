#ifndef VIDEOENCODER_H
#define VIDEOENCODER_H

#include <QObject>
#include <QProcess>
#include <QString>

class VideoEncoder : public QObject
{
    Q_OBJECT

public:
    explicit VideoEncoder(QObject* parent = nullptr);

    bool isFFmpegAvailable() const;
    bool isBusy() const;
    QString getFFmpegPath() const;
    bool startConvertToVideo(const QString& inputDir, const QString& outputPath, int fps = 5);

signals:
    void conversionFinished(QString outputPath);
    void conversionFailed(QString outputPath, QString message);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    void findFFmpeg();
    void emitConversionFailed(const QString& message);

    QString m_ffmpegPath;
    QString m_pendingOutputPath;
    QProcess* m_process = nullptr;
};

#endif // VIDEOENCODER_H
