#ifndef FFMPEGFRAMEEXTRACTOR_H
#define FFMPEGFRAMEEXTRACTOR_H

#ifdef USE_FFMPEG

#include <QImage>
#include <QString>

class FFmpegFrameExtractor
{
public:
    static bool extractFrame(const QString& filePath,
                             qint64 positionMs,
                             QImage& outImage,
                             QString* errorMessage = nullptr);
};

#endif // USE_FFMPEG

#endif // FFMPEGFRAMEEXTRACTOR_H
