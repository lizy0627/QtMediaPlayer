#ifndef FFMPEGMEDIAINFO_H
#define FFMPEGMEDIAINFO_H

#include <QString>
#include <QtGlobal>

struct FFmpegMediaInfo {
    bool valid = false;
    QString filePath;
    QString formatName;
    qint64 durationMs = 0;
    qint64 bitRate = 0;
    bool hasVideo = false;
    bool hasAudio = false;
    QString videoCodec;
    QString audioCodec;
    int width = 0;
    int height = 0;
    double fps = 0.0;
    int sampleRate = 0;
    int channels = 0;
    QString errorMessage;
};

#endif // FFMPEGMEDIAINFO_H
