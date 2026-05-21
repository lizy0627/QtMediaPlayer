#ifndef MEDIAPROBESERVICE_H
#define MEDIAPROBESERVICE_H

#include <QString>
#include <QStringList>

enum class ProbeStatus {
    Supported,
    FileNotFound,
    InvalidFile,
    UnsupportedExtension,
    UnsupportedByQtBackend,
    UnknownError
};

struct ProbeResult {
    ProbeStatus status = ProbeStatus::UnknownError;
    QString reason;
    QStringList supportedAudioFormats;
    QStringList supportedVideoFormats;
};

class MediaProbeService
{
public:
    static ProbeResult probeLocalFile(const QString& filePath);
    static QStringList supportedAudioFormats();
    static QStringList supportedVideoFormats();
};

#endif // MEDIAPROBESERVICE_H
