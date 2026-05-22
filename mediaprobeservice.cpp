#include "mediaprobeservice.h"

#include "mediafileprobe.h"

namespace {
ProbeStatus statusForIssue(MediaProbeIssue issue)
{
    switch (issue) {
    case MediaProbeIssue::None:
        return ProbeStatus::Supported;
    case MediaProbeIssue::FileNotFound:
        return ProbeStatus::FileNotFound;
    case MediaProbeIssue::UnsupportedExtension:
        return ProbeStatus::UnsupportedExtension;
    case MediaProbeIssue::EmptyPath:
    case MediaProbeIssue::NotRegularFile:
    case MediaProbeIssue::NotReadable:
    case MediaProbeIssue::EmptyFile:
        return ProbeStatus::InvalidFile;
    }

    return ProbeStatus::UnknownError;
}

ProbeResult makeResult(const MediaProbeResult& probeResult)
{
    ProbeResult result;
    result.status = probeResult.supported ? ProbeStatus::Supported : statusForIssue(probeResult.issue);
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
