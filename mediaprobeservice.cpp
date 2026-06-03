#include "mediaprobeservice.h"

#include "mediafileprobe.h"

#ifdef USE_FFMPEG
#include "ffmpeg/ffmpegmediainfo.h"
#include "ffmpeg/ffmpegprobe.h"
#endif

#include <QFileInfo>

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
    result.route = probeResult.route;
    result.supportedAudioFormats = MediaFileProbe::supportedAudioFormats();
    result.supportedVideoFormats = MediaFileProbe::supportedVideoFormats();
    return result;
}

QString fileDisplayName(const QString& filePath)
{
    const QFileInfo fileInfo(filePath);
    const QString fileName = fileInfo.fileName();
    return fileName.isEmpty() ? filePath : fileName;
}

QString supportedFormatText(const ProbeResult& result)
{
    return QStringLiteral("\n\n\u652f\u6301\u7684\u97f3\u9891\u683c\u5f0f\uff1a%1\n"
                          "\u652f\u6301\u7684\u89c6\u9891\u683c\u5f0f\uff1a%2")
        .arg(result.supportedAudioFormats.join(QStringLiteral(", ")),
             result.supportedVideoFormats.join(QStringLiteral(", ")));
}

QString quickProbeFailureMessage(const QString& filePath, const ProbeResult& result)
{
    const QString reason = result.reason.trimmed().isEmpty()
        ? QStringLiteral("\u6587\u4ef6\u672a\u901a\u8fc7\u64ad\u653e\u524d\u68c0\u67e5")
        : result.reason.trimmed();
    return QStringLiteral("\u65e0\u6cd5\u64ad\u653e\u201c%1\u201d\u3002\n\n"
                          "\u53ef\u80fd\u539f\u56e0\uff1a%2%3")
        .arg(fileDisplayName(filePath), reason, supportedFormatText(result));
}

QString routeMismatchMessage(const QString& filePath,
                             const ProbeResult& result,
                             MediaRoute expectedRoute)
{
    QString reason;
    if (expectedRoute == MediaRoute::Audio && result.route == MediaRoute::Video) {
        reason = QStringLiteral("\u8be5\u6587\u4ef6\u662f\u89c6\u9891\u6587\u4ef6\uff0c"
                                "\u8bf7\u4f7f\u7528\u89c6\u9891\u64ad\u653e\u5668\u6253\u5f00\u3002");
    } else if (expectedRoute == MediaRoute::Video && result.route == MediaRoute::Audio) {
        reason = QStringLiteral("\u8be5\u6587\u4ef6\u662f\u97f3\u9891\u6587\u4ef6\uff0c"
                                "\u8bf7\u4f7f\u7528\u97f3\u9891\u64ad\u653e\u5668\u6253\u5f00\u3002");
    } else if (expectedRoute == MediaRoute::Audio) {
        reason = QStringLiteral("\u8be5\u6587\u4ef6\u4e0d\u662f\u53ef\u8bc6\u522b\u7684\u97f3\u9891\u6587\u4ef6\u3002");
    } else {
        reason = QStringLiteral("\u8be5\u6587\u4ef6\u4e0d\u662f\u53ef\u8bc6\u522b\u7684\u89c6\u9891\u6587\u4ef6\u3002");
    }

    return QStringLiteral("\u65e0\u6cd5\u64ad\u653e\u201c%1\u201d\u3002\n\n"
                          "\u53ef\u80fd\u539f\u56e0\uff1a%2%3")
        .arg(fileDisplayName(filePath), reason, supportedFormatText(result));
}

LocalPlaybackProbeResult makeRejectedPlaybackResult(const QString& filePath,
                                                    const ProbeResult& quickProbe,
                                                    const QString& title,
                                                    const QString& message,
                                                    const MediaInfo& mediaInfo = MediaInfo())
{
    LocalPlaybackProbeResult result;
    result.playable = false;
    result.route = quickProbe.route;
    result.title = title;
    result.message = message;
    result.quickProbe = quickProbe;
    result.mediaInfo = mediaInfo;
    if (result.mediaInfo.filePath.isEmpty()) {
        result.mediaInfo.filePath = filePath;
    }
    return result;
}

#ifdef USE_FFMPEG
QString ffmpegFailureReason(const MediaInfo& info)
{
    const QString reason = info.errorMessage.trimmed();
    return reason.isEmpty()
        ? QStringLiteral("FFmpeg \u672a\u8fd4\u56de\u66f4\u5177\u4f53\u7684\u9519\u8bef\u539f\u56e0\u3002")
        : reason;
}
#endif
}

ProbeResult MediaProbeService::probeLocalFile(const QString& filePath)
{
    return makeResult(MediaFileProbe::probe(filePath));
}

LocalPlaybackProbeResult MediaProbeService::probeLocalPlayback(const QString& filePath,
                                                               MediaRoute expectedRoute)
{
    const ProbeResult quickProbe = probeLocalFile(filePath);
    if (quickProbe.status != ProbeStatus::Supported) {
        return makeRejectedPlaybackResult(
            filePath,
            quickProbe,
            QStringLiteral("\u6587\u4ef6\u65e0\u6cd5\u64ad\u653e"),
            quickProbeFailureMessage(filePath, quickProbe));
    }

    if (expectedRoute != MediaRoute::Unsupported && quickProbe.route != expectedRoute) {
        return makeRejectedPlaybackResult(
            filePath,
            quickProbe,
            QStringLiteral("\u5a92\u4f53\u7c7b\u578b\u4e0d\u5339\u914d"),
            routeMismatchMessage(filePath, quickProbe, expectedRoute));
    }

    LocalPlaybackProbeResult result;
    result.playable = true;
    result.route = quickProbe.route;
    result.quickProbe = quickProbe;

#ifdef USE_FFMPEG
    const MediaInfo info = probeMediaInfo(filePath);
    result.mediaInfo = info;

    const QString ffmpegReason = ffmpegFailureReason(info);
    if (!info.valid && ffmpegReason == QStringLiteral("NO_PLAYABLE_AUDIO_VIDEO_STREAM")) {
        const QString message = QStringLiteral("\u65e0\u6cd5\u64ad\u653e\u201c%1\u201d\u3002\n\n"
                                               "\u53ef\u80fd\u539f\u56e0\uff1a"
                                               "\u672a\u68c0\u6d4b\u5230\u53ef\u64ad\u653e\u7684\u97f3\u89c6\u9891\u6d41\u3002")
                                    .arg(fileDisplayName(filePath));
        return makeRejectedPlaybackResult(
            filePath,
            quickProbe,
            QStringLiteral("\u672a\u68c0\u6d4b\u5230\u53ef\u64ad\u653e\u7684\u97f3\u89c6\u9891\u6d41"),
            message,
            info);
    }

    if (!info.valid) {
        const QString message = QStringLiteral("\u65e0\u6cd5\u64ad\u653e\u201c%1\u201d\u3002\n\n"
                                               "FFmpeg \u8fd4\u56de\u7684\u9519\u8bef\u539f\u56e0\uff1a\n%2")
                                    .arg(fileDisplayName(filePath), ffmpegReason);
        return makeRejectedPlaybackResult(
            filePath,
            quickProbe,
            QStringLiteral("FFmpeg \u6df1\u5ea6\u63a2\u6d4b\u5931\u8d25"),
            message,
            info);
    }

    if (!info.hasAudio && !info.hasVideo) {
        const QString message = QStringLiteral("\u65e0\u6cd5\u64ad\u653e\u201c%1\u201d\u3002\n\n"
                                               "\u53ef\u80fd\u539f\u56e0\uff1a"
                                               "\u672a\u68c0\u6d4b\u5230\u53ef\u64ad\u653e\u7684\u97f3\u89c6\u9891\u6d41\u3002")
                                    .arg(fileDisplayName(filePath));
        return makeRejectedPlaybackResult(
            filePath,
            quickProbe,
            QStringLiteral("\u672a\u68c0\u6d4b\u5230\u53ef\u64ad\u653e\u7684\u97f3\u89c6\u9891\u6d41"),
            message,
            info);
    }

    if (expectedRoute == MediaRoute::Video && !info.hasVideo) {
        const QString message = QStringLiteral("\u65e0\u6cd5\u64ad\u653e\u201c%1\u201d\u3002\n\n"
                                               "\u53ef\u80fd\u539f\u56e0\uff1a"
                                               "\u8be5\u6587\u4ef6\u4e0d\u5305\u542b\u89c6\u9891\u6d41\u3002")
                                    .arg(fileDisplayName(filePath));
        return makeRejectedPlaybackResult(
            filePath,
            quickProbe,
            QStringLiteral("\u8be5\u6587\u4ef6\u4e0d\u5305\u542b\u89c6\u9891\u6d41"),
            message,
            info);
    }

    if (expectedRoute == MediaRoute::Audio && !info.hasAudio) {
        const QString message = QStringLiteral("\u65e0\u6cd5\u64ad\u653e\u201c%1\u201d\u3002\n\n"
                                               "\u53ef\u80fd\u539f\u56e0\uff1a"
                                               "\u8be5\u6587\u4ef6\u4e0d\u5305\u542b\u97f3\u9891\u6d41\u3002")
                                    .arg(fileDisplayName(filePath));
        return makeRejectedPlaybackResult(
            filePath,
            quickProbe,
            QStringLiteral("\u8be5\u6587\u4ef6\u4e0d\u5305\u542b\u97f3\u9891\u6d41"),
            message,
            info);
    }
#endif

    return result;
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
