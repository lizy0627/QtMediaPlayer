#include "mediafileprobe.h"

#include <QFile>
#include <QFileInfo>

namespace {
const char* const kAudioExtensions[] = {
    "aac",
    "flac",
    "m4a",
    "mp3",
    "ogg",
    "wav",
    "wma"
};

const char* const kVideoExtensions[] = {
    "avi",
    "flv",
    "m4v",
    "mkv",
    "mov",
    "mp4",
    "webm",
    "wmv"
};

MediaProbeResult makeResult(bool supported,
                            const QString& reason,
                            MediaRoute route,
                            MediaProbeIssue issue = MediaProbeIssue::None)
{
    MediaProbeResult result;
    result.supported = supported;
    result.reason = reason;
    result.route = route;
    result.issue = issue;
    return result;
}

QStringList makeExtensionList(const char* const* extensions, int count)
{
    QStringList result;
    for (int i = 0; i < count; ++i) {
        result.append(QString::fromLatin1(extensions[i]));
    }
    return result;
}

MediaRoute routeForSuffix(const QString& suffix)
{
    if (MediaFileProbe::supportedAudioFormats().contains(suffix)) {
        return MediaRoute::Audio;
    }
    if (MediaFileProbe::supportedVideoFormats().contains(suffix)) {
        return MediaRoute::Video;
    }
    return MediaRoute::Unsupported;
}
}

MediaProbeResult MediaFileProbe::probe(const QString& filePath)
{
    const QString normalizedPath = filePath.trimmed();
    if (normalizedPath.isEmpty()) {
        return makeResult(false,
                          QStringLiteral("\u6587\u4ef6\u8def\u5f84\u4e3a\u7a7a"),
                          MediaRoute::Unsupported,
                          MediaProbeIssue::EmptyPath);
    }

    const QFileInfo fileInfo(normalizedPath);
    if (!fileInfo.exists()) {
        return makeResult(false,
                          QStringLiteral("\u6587\u4ef6\u4e0d\u5b58\u5728"),
                          MediaRoute::Unsupported,
                          MediaProbeIssue::FileNotFound);
    }

    if (!fileInfo.isFile()) {
        return makeResult(false,
                          QStringLiteral("\u4e0d\u662f\u6709\u6548\u7684\u666e\u901a\u6587\u4ef6"),
                          MediaRoute::Unsupported,
                          MediaProbeIssue::NotRegularFile);
    }

    QFile file(fileInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return makeResult(false,
                          QStringLiteral("\u6587\u4ef6\u4e0d\u53ef\u8bfb\uff0c\u53ef\u80fd\u6ca1\u6709\u8bbf\u95ee\u6743\u9650"),
                          MediaRoute::Unsupported,
                          MediaProbeIssue::NotReadable);
    }
    file.close();

    if (fileInfo.size() == 0) {
        return makeResult(false,
                          QStringLiteral("\u6587\u4ef6\u4e3a\u7a7a\uff0c\u65e0\u6cd5\u64ad\u653e"),
                          MediaRoute::Unsupported,
                          MediaProbeIssue::EmptyFile);
    }

    const QString suffix = fileInfo.suffix().toLower();
    const MediaRoute route = routeForSuffix(suffix);
    if (route == MediaRoute::Unsupported) {
        return makeResult(false,
                          QStringLiteral("\u5f53\u524d\u6587\u4ef6\u6269\u5c55\u540d\u4e0d\u5728\u652f\u6301\u5217\u8868\u4e2d\uff1a%1")
                              .arg(suffix),
                          MediaRoute::Unsupported,
                          MediaProbeIssue::UnsupportedExtension);
    }

    return makeResult(true, QString(), route);
}

MediaProbeResult MediaFileProbe::probeLocalFile(const QString& filePath)
{
    return probe(filePath);
}

QList<ProbedMediaFile> MediaFileProbe::probeFiles(const QStringList& files)
{
    QList<ProbedMediaFile> results;
    results.reserve(files.size());

    for (const QString& filePath : files) {
        const MediaProbeResult probeResult = probe(filePath);

        ProbedMediaFile probedFile;
        probedFile.filePath = filePath;
        probedFile.route = probeResult.route;
        probedFile.supported = probeResult.supported;
        probedFile.reason = probeResult.reason;
        results.append(probedFile);
    }

    return results;
}

QStringList MediaFileProbe::supportedAudioFormats()
{
    return makeExtensionList(kAudioExtensions,
                             sizeof(kAudioExtensions) / sizeof(kAudioExtensions[0]));
}

QStringList MediaFileProbe::supportedVideoFormats()
{
    return makeExtensionList(kVideoExtensions,
                             sizeof(kVideoExtensions) / sizeof(kVideoExtensions[0]));
}
