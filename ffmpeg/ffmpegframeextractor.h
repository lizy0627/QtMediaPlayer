#ifndef FFMPEGFRAMEEXTRACTOR_H
#define FFMPEGFRAMEEXTRACTOR_H

#include <QImage>
#include <QString>

class FFmpegFrameExtractor
{
public:
    QImage extractFrame(const QString& filePath, qint64 positionMs);
    QString lastError() const;

    static bool extractFrame(const QString& filePath,
                             qint64 positionMs,
                             QImage& outImage,
                             QString* errorMessage = nullptr);

private:
    QString m_lastError;
};

#endif // FFMPEGFRAMEEXTRACTOR_H
