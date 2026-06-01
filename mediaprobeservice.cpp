#include "mediaprobeservice.h"

#include "mediafileprobe.h"

#ifdef USE_FFMPEG
#include "ffmpeg/ffmpegmediainfo.h"
#include "ffmpeg/ffmpegprobe.h"
#endif

namespace {
#ifdef USE_FFMPEG
MediaInfo toMediaInfo(const FFmpegMediaInfo& ffmpegInfo)
{
    MediaInfo info;
    info.valid = ffmpegInfo.valid;
    info.filePath = ffmpegInfo.filePath;
    info.formatName = ffmpegInfo.formatName;
    info.durationMs = ffmpegInfo.durationMs;
    info.bitRate = ffmpegInfo.bitRate;
    info.hasVideo = ffmpegInfo.hasVideo;
    info.hasAudio = ffmpegInfo.hasAudio;
    info.videoCodec = ffmpegInfo.videoCodec;
    info.audioCodec = ffmpegInfo.audioCodec;
    info.width = ffmpegInfo.width;
    info.height = ffmpegInfo.height;
    info.fps = ffmpegInfo.fps;
    info.sampleRate = ffmpegInfo.sampleRate;
    info.channels = ffmpegInfo.channels;
    info.errorMessage = ffmpegInfo.errorMessage;
    return info;
}
#endif

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

MediaInfo MediaProbeService::probeMediaInfo(const QString& filePath)
{
#ifdef USE_FFMPEG
    return toMediaInfo(FFmpegProbe::probeFile(filePath));
#else
    MediaInfo info;
    info.filePath = filePath;

    const MediaProbeResult probeResult = MediaFileProbe::probe(filePath);
    info.errorMessage = probeResult.supported
        ? QStringLiteral("\u5f53\u524d\u672a\u542f\u7528 FFmpeg \u6df1\u5ea6\u63a2\u6d4b")
        : probeResult.reason;
    return info;
#endif
}

QStringList MediaProbeService::supportedAudioFormats()
{
    return MediaFileProbe::supportedAudioFormats();
}

QStringList MediaProbeService::supportedVideoFormats()
{
    return MediaFileProbe::supportedVideoFormats();
}
