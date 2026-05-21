#ifndef MEDIAFILEPROBE_H
#define MEDIAFILEPROBE_H

#include <QString>
#include <QStringList>

enum class MediaRoute {
    Audio,
    Video,
    Unsupported
};

struct MediaProbeResult {
    bool supported = false;
    QString reason;
    MediaRoute route = MediaRoute::Unsupported;
};

class MediaFileProbe
{
public:
    static MediaProbeResult probe(const QString& filePath);
    static MediaProbeResult probeLocalFile(const QString& filePath);
    static QStringList supportedAudioFormats();
    static QStringList supportedVideoFormats();
};

#endif // MEDIAFILEPROBE_H
