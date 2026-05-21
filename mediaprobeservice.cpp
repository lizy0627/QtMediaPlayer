#include "mediaprobeservice.h"

#include <QFileInfo>

namespace {
ProbeResult makeResult(ProbeStatus status, const QString& reason)
{
    ProbeResult result;
    result.status = status;
    result.reason = reason;
    result.supportedAudioFormats = MediaProbeService::supportedAudioFormats();
    result.supportedVideoFormats = MediaProbeService::supportedVideoFormats();
    return result;
}
}

ProbeResult MediaProbeService::probeLocalFile(const QString& filePath)
{
    const QString normalizedPath = filePath.trimmed();
    if (normalizedPath.isEmpty()) {
        return makeResult(ProbeStatus::InvalidFile,
                          QStringLiteral("\u6587\u4ef6\u8def\u5f84\u4e3a\u7a7a\u3002"));
    }

    const QFileInfo fileInfo(normalizedPath);
    if (!fileInfo.exists()) {
        return makeResult(ProbeStatus::FileNotFound,
                          QStringLiteral("\u6587\u4ef6\u4e0d\u5b58\u5728\u6216\u5df2\u88ab\u5220\u9664\u3002"));
    }

    if (!fileInfo.isFile()) {
        return makeResult(ProbeStatus::InvalidFile,
                          QStringLiteral("\u9009\u62e9\u7684\u8def\u5f84\u4e0d\u662f\u4e00\u4e2a\u6709\u6548\u6587\u4ef6\u3002"));
    }

    const QString suffix = fileInfo.suffix().toLower();
    if (suffix.isEmpty()) {
        return makeResult(ProbeStatus::UnsupportedExtension,
                          QStringLiteral("\u6587\u4ef6\u6ca1\u6709\u6269\u5c55\u540d\uff0c\u65e0\u6cd5\u5224\u65ad\u5a92\u4f53\u683c\u5f0f\u3002"));
    }

    const QStringList audioFormats = supportedAudioFormats();
    const QStringList videoFormats = supportedVideoFormats();
    if (!audioFormats.contains(suffix) && !videoFormats.contains(suffix)) {
        return makeResult(ProbeStatus::UnsupportedExtension,
                          QStringLiteral("\u6587\u4ef6\u6269\u5c55\u540d .%1 \u4e0d\u5728\u5f53\u524d\u64ad\u653e\u5668\u652f\u6301\u5217\u8868\u4e2d\u3002")
                              .arg(suffix));
    }

    return makeResult(ProbeStatus::Supported,
                      QStringLiteral("\u6587\u4ef6\u5df2\u901a\u8fc7\u57fa\u7840\u68c0\u67e5\u3002"));
}

QStringList MediaProbeService::supportedAudioFormats()
{
    return {
        QStringLiteral("mp3"),
        QStringLiteral("wav"),
        QStringLiteral("flac"),
        QStringLiteral("aac"),
        QStringLiteral("m4a"),
        QStringLiteral("ogg")
    };
}

QStringList MediaProbeService::supportedVideoFormats()
{
    return {
        QStringLiteral("mp4"),
        QStringLiteral("avi"),
        QStringLiteral("mkv"),
        QStringLiteral("mov"),
        QStringLiteral("wmv"),
        QStringLiteral("flv")
    };
}
