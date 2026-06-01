#ifndef MEDIAPROBESERVICE_H
#define MEDIAPROBESERVICE_H

#include <QString>
#include <QStringList>
#include <QtGlobal>

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

struct MediaInfo {
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

class MediaProbeService
{
public:
    static ProbeResult probeLocalFile(const QString& filePath);
    static MediaInfo probeMediaInfo(const QString& filePath);
    static QStringList supportedAudioFormats();
    static QStringList supportedVideoFormats();
};

#endif // MEDIAPROBESERVICE_H
