#include "mediaprobeservice.h"

#include "mediafileprobe.h"

namespace {
ProbeResult makeResult(const MediaProbeResult& probeResult)
{
    ProbeResult result;
    result.status = probeResult.supported ? ProbeStatus::Supported : ProbeStatus::UnsupportedByQtBackend;
    result.reason = probeResult.reason;
    result.supportedAudioFormats = MediaFileProbe::supportedAudioFormats();
    result.supportedVideoFormats = MediaFileProbe::supportedVideoFormats();
    return result;
}
}

ProbeResult MediaProbeService::probeLocalFile(const QString& filePath)
{
    return makeResult(MediaFileProbe::probe(filePath));
}

QStringList MediaProbeService::supportedAudioFormats()
{
    return MediaFileProbe::supportedAudioFormats();
}

QStringList MediaProbeService::supportedVideoFormats()
{
    return MediaFileProbe::supportedVideoFormats();
}
