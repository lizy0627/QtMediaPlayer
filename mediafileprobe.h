#ifndef MEDIAFILEPROBE_H
#define MEDIAFILEPROBE_H

#include <QList>
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

struct ProbedMediaFile {
    QString filePath;
    MediaRoute route = MediaRoute::Unsupported;
    bool supported = false;
    QString reason;
};

class MediaFileProbe
{
public:
    static MediaProbeResult probe(const QString& filePath);
    static MediaProbeResult probeLocalFile(const QString& filePath);
    static QList<ProbedMediaFile> probeFiles(const QStringList& files);
    static QStringList supportedAudioFormats();
    static QStringList supportedVideoFormats();
};

#endif // MEDIAFILEPROBE_H
