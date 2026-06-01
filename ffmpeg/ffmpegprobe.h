#ifndef FFMPEGPROBE_H
#define FFMPEGPROBE_H

#include "ffmpegmediainfo.h"

#include <QString>

class FFmpegProbe
{
public:
    static FFmpegMediaInfo probeFile(const QString& filePath);
};

#endif // FFMPEGPROBE_H
