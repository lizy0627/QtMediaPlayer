#include "mediafileprobe.h"

#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QTimer>
#include <QUrl>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QMediaContent>
#endif

namespace {
constexpr int kProbeTimeoutMs = 3000;

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

MediaProbeResult makeResult(bool supported, const QString& reason, MediaRoute route)
{
    MediaProbeResult result;
    result.supported = supported;
    result.reason = reason;
    result.route = route;
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

bool isRecognizedStatus(QMediaPlayer::MediaStatus status)
{
    return status == QMediaPlayer::LoadedMedia
        || status == QMediaPlayer::BufferedMedia
        || status == QMediaPlayer::BufferingMedia;
}

bool isFinishedStatus(QMediaPlayer::MediaStatus status)
{
    return isRecognizedStatus(status) || status == QMediaPlayer::InvalidMedia;
}

void loadLocalFile(QMediaPlayer& player, const QString& filePath)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    player.setSource(QUrl::fromLocalFile(filePath));
#else
    player.setMedia(QUrl::fromLocalFile(filePath));
#endif
}

void clearPlayerSource(QMediaPlayer& player)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    player.setSource(QUrl());
#else
    player.setMedia(QMediaContent());
#endif
}

MediaProbeResult probePlayable(const QString& filePath, MediaRoute route)
{
    QMediaPlayer player;
    QEventLoop eventLoop;
    QTimer timeoutTimer;
    bool timedOut = false;
    bool hasError = false;

    timeoutTimer.setSingleShot(true);

    QObject::connect(&player,
                     &QMediaPlayer::mediaStatusChanged,
                     &eventLoop,
                     [&](QMediaPlayer::MediaStatus status) {
                         if (isFinishedStatus(status) && eventLoop.isRunning()) {
                             eventLoop.quit();
                         }
                     });

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QObject::connect(&player,
                     &QMediaPlayer::errorOccurred,
                     &eventLoop,
                     [&](QMediaPlayer::Error error, const QString&) {
                         if (error != QMediaPlayer::NoError) {
                             hasError = true;
                             if (eventLoop.isRunning()) {
                                 eventLoop.quit();
                             }
                         }
                     });
#else
    QObject::connect(&player,
                     QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error),
                     &eventLoop,
                     [&](QMediaPlayer::Error error) {
                         if (error != QMediaPlayer::NoError) {
                             hasError = true;
                             if (eventLoop.isRunning()) {
                                 eventLoop.quit();
                             }
                         }
                     });
#endif

    QObject::connect(&timeoutTimer, &QTimer::timeout, &eventLoop, [&]() {
        timedOut = true;
        eventLoop.quit();
    });

    loadLocalFile(player, filePath);

    if (!isFinishedStatus(player.mediaStatus()) && !hasError) {
        timeoutTimer.start(kProbeTimeoutMs);
        eventLoop.exec();
    }

    if (timeoutTimer.isActive()) {
        timeoutTimer.stop();
    }

    const bool recognized = isRecognizedStatus(player.mediaStatus());
    const bool invalid = player.mediaStatus() == QMediaPlayer::InvalidMedia;
    const bool playerError = player.error() != QMediaPlayer::NoError;

    player.stop();
    clearPlayerSource(player);

    if (recognized && !playerError) {
        return makeResult(true, QString(), route);
    }

    if (timedOut) {
        return makeResult(false,
                          QStringLiteral("\u5a92\u4f53\u63a2\u6d4b\u8d85\u65f6\uff0c\u53ef\u80fd\u6587\u4ef6\u635f\u574f\u6216\u5f53\u524d\u89e3\u7801\u5668\u4e0d\u652f\u6301"),
                          route);
    }

    if (invalid || playerError || hasError) {
        QString reason = QStringLiteral("\u5a92\u4f53\u683c\u5f0f\u6216\u7f16\u7801\u4e0d\u53d7\u5f53\u524d\u73af\u5883\u652f\u6301");
        const QString detail = player.errorString().trimmed();
        if (!detail.isEmpty()) {
            reason += QStringLiteral("\nQt \u8fd4\u56de\uff1a%1").arg(detail);
        }
        return makeResult(false, reason, route);
    }

    return makeResult(false,
                      QStringLiteral("\u5a92\u4f53\u63a2\u6d4b\u8d85\u65f6\uff0c\u53ef\u80fd\u6587\u4ef6\u635f\u574f\u6216\u5f53\u524d\u89e3\u7801\u5668\u4e0d\u652f\u6301"),
                      route);
}
}

MediaProbeResult MediaFileProbe::probe(const QString& filePath)
{
    const QString normalizedPath = filePath.trimmed();
    if (normalizedPath.isEmpty()) {
        return makeResult(false, QStringLiteral("\u6587\u4ef6\u8def\u5f84\u4e3a\u7a7a"), MediaRoute::Unsupported);
    }

    const QFileInfo fileInfo(normalizedPath);
    if (!fileInfo.exists()) {
        return makeResult(false, QStringLiteral("\u6587\u4ef6\u4e0d\u5b58\u5728"), MediaRoute::Unsupported);
    }

    if (!fileInfo.isFile()) {
        return makeResult(false, QStringLiteral("\u4e0d\u662f\u6709\u6548\u7684\u666e\u901a\u6587\u4ef6"), MediaRoute::Unsupported);
    }

    QFile file(fileInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return makeResult(false,
                          QStringLiteral("\u6587\u4ef6\u4e0d\u53ef\u8bfb\uff0c\u53ef\u80fd\u6ca1\u6709\u8bbf\u95ee\u6743\u9650"),
                          MediaRoute::Unsupported);
    }
    file.close();

    if (fileInfo.size() == 0) {
        return makeResult(false, QStringLiteral("\u6587\u4ef6\u4e3a\u7a7a\uff0c\u65e0\u6cd5\u64ad\u653e"), MediaRoute::Unsupported);
    }

    const QString suffix = fileInfo.suffix().toLower();
    const MediaRoute route = routeForSuffix(suffix);
    if (route == MediaRoute::Unsupported) {
        return makeResult(false,
                          QStringLiteral("\u5f53\u524d\u6587\u4ef6\u6269\u5c55\u540d\u4e0d\u5728\u652f\u6301\u5217\u8868\u4e2d\uff1a%1")
                              .arg(suffix),
                          MediaRoute::Unsupported);
    }

    return probePlayable(fileInfo.absoluteFilePath(), route);
}

MediaProbeResult MediaFileProbe::probeLocalFile(const QString& filePath)
{
    return probe(filePath);
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
