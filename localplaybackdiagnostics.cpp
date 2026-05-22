#include "localplaybackdiagnostics.h"

#include "mediafileprobe.h"

#include <QFile>
#include <QFileInfo>
#include <QStringList>

namespace {
bool containsAny(const QString& value, const QStringList& needles)
{
    const QString lowerValue = value.toLower();
    for (const QString& needle : needles) {
        if (lowerValue.contains(needle)) {
            return true;
        }
    }
    return false;
}

bool hasSupportedExtension(const QFileInfo& fileInfo)
{
    const QString suffix = fileInfo.suffix().toLower();
    return MediaFileProbe::supportedAudioFormats().contains(suffix)
        || MediaFileProbe::supportedVideoFormats().contains(suffix);
}

QString fileDisplayName(const QString& filePath)
{
    const QFileInfo fileInfo(filePath);
    const QString fileName = fileInfo.fileName();
    return fileName.isEmpty() ? filePath : fileName;
}

QString titleForKind(LocalPlaybackFailureKind kind)
{
    switch (kind) {
    case LocalPlaybackFailureKind::FileMissing:
        return QStringLiteral("\u6587\u4ef6\u4e0d\u5b58\u5728\u6216\u5df2\u88ab\u79fb\u52a8");
    case LocalPlaybackFailureKind::FileNotReadable:
        return QStringLiteral("\u6587\u4ef6\u65e0\u6cd5\u8bbf\u95ee");
    case LocalPlaybackFailureKind::EmptyOrDamaged:
    case LocalPlaybackFailureKind::DamagedFile:
        return QStringLiteral("\u6587\u4ef6\u53ef\u80fd\u5df2\u635f\u574f");
    case LocalPlaybackFailureKind::UnsupportedExtension:
        return QStringLiteral("\u6587\u4ef6\u683c\u5f0f\u4e0d\u5728\u652f\u6301\u5217\u8868\u4e2d");
    case LocalPlaybackFailureKind::UnsupportedCodec:
        return QStringLiteral("\u7f16\u7801\u4e0d\u53d7\u652f\u6301");
    case LocalPlaybackFailureKind::MissingDecoder:
        return QStringLiteral("\u7cfb\u7edf\u7f3a\u5c11\u89e3\u7801\u5668");
    case LocalPlaybackFailureKind::Unknown:
        return QStringLiteral("\u64ad\u653e\u5931\u8d25");
    }

    return QStringLiteral("\u64ad\u653e\u5931\u8d25");
}

QString reasonForKind(LocalPlaybackFailureKind kind)
{
    switch (kind) {
    case LocalPlaybackFailureKind::FileMissing:
        return QStringLiteral("\u6587\u4ef6\u4e0d\u5b58\u5728\uff0c\u6216\u8005\u5728\u52a0\u5165\u540e\u88ab\u79fb\u52a8\u3001\u91cd\u547d\u540d\u6216\u5220\u9664\u4e86\u3002");
    case LocalPlaybackFailureKind::FileNotReadable:
        return QStringLiteral("\u5f53\u524d\u7a0b\u5e8f\u6ca1\u6709\u8bfb\u53d6\u6743\u9650\uff0c\u6216\u6587\u4ef6\u6b63\u88ab\u5176\u4ed6\u7a0b\u5e8f\u5360\u7528\u3002");
    case LocalPlaybackFailureKind::EmptyOrDamaged:
        return QStringLiteral("\u6587\u4ef6\u4e3a\u7a7a\uff0c\u6216\u8005\u5185\u5bb9\u4e0d\u5b8c\u6574\u3001\u5df2\u635f\u574f\u3002");
    case LocalPlaybackFailureKind::UnsupportedExtension:
        return QStringLiteral("\u8be5\u6587\u4ef6\u7684\u6269\u5c55\u540d\u4e0d\u5728\u5f53\u524d\u652f\u6301\u5217\u8868\u4e2d\u3002");
    case LocalPlaybackFailureKind::DamagedFile:
        return QStringLiteral("\u64ad\u653e\u5668\u8bfb\u5230\u7684\u6587\u4ef6\u5185\u5bb9\u50cf\u662f\u4e0d\u5b8c\u6574\u6216\u5df2\u635f\u574f\u3002");
    case LocalPlaybackFailureKind::UnsupportedCodec:
        return QStringLiteral("\u6269\u5c55\u540d\u5df2\u901a\u8fc7\u5feb\u901f\u68c0\u67e5\uff0c\u4f46\u6587\u4ef6\u5185\u90e8\u7684\u97f3\u89c6\u9891\u7f16\u7801\u5f53\u524d\u65e0\u6cd5\u89e3\u7801\u3002");
    case LocalPlaybackFailureKind::MissingDecoder:
        return QStringLiteral("\u7cfb\u7edf\u6216 Qt \u540e\u7aef\u7f3a\u5c11\u64ad\u653e\u8be5\u7f16\u7801\u6240\u9700\u7684\u89e3\u7801\u5668\u3002");
    case LocalPlaybackFailureKind::Unknown:
        return QStringLiteral("\u64ad\u653e\u5668\u672a\u80fd\u52a0\u8f7d\u8be5\u672c\u5730\u5a92\u4f53\uff0c\u4f46\u6ca1\u6709\u8fd4\u56de\u8db3\u591f\u660e\u786e\u7684\u539f\u56e0\u3002");
    }

    return QString();
}

QString suggestionForKind(LocalPlaybackFailureKind kind)
{
    switch (kind) {
    case LocalPlaybackFailureKind::FileMissing:
        return QStringLiteral("\u8bf7\u91cd\u65b0\u9009\u62e9\u8be5\u6587\u4ef6\uff0c\u6216\u68c0\u67e5\u5b83\u7684\u65b0\u4f4d\u7f6e\u3002");
    case LocalPlaybackFailureKind::FileNotReadable:
        return QStringLiteral("\u8bf7\u68c0\u67e5\u6587\u4ef6\u6743\u9650\uff0c\u6216\u5173\u95ed\u53ef\u80fd\u6b63\u5728\u5360\u7528\u5b83\u7684\u7a0b\u5e8f\u540e\u91cd\u8bd5\u3002");
    case LocalPlaybackFailureKind::EmptyOrDamaged:
    case LocalPlaybackFailureKind::DamagedFile:
        return QStringLiteral("\u8bf7\u91cd\u65b0\u4e0b\u8f7d\u6216\u91cd\u65b0\u5bfc\u51fa\u8be5\u6587\u4ef6\uff0c\u5e76\u786e\u8ba4\u5b83\u5728\u5176\u4ed6\u64ad\u653e\u5668\u4e2d\u53ef\u4ee5\u6b63\u5e38\u6253\u5f00\u3002");
    case LocalPlaybackFailureKind::UnsupportedExtension:
        return QStringLiteral("\u8bf7\u9009\u62e9\u652f\u6301\u7684\u97f3\u9891\u6216\u89c6\u9891\u6587\u4ef6\uff0c\u6216\u5148\u5c06\u6587\u4ef6\u8f6c\u6362\u4e3a\u5e38\u89c1\u683c\u5f0f\u3002");
    case LocalPlaybackFailureKind::UnsupportedCodec:
        return QStringLiteral("\u53ef\u4ee5\u5c1d\u8bd5\u8f6c\u6362\u4e3a MP4\uff08H.264/AAC\uff09\u6216 MP3/WAV \u7b49\u66f4\u901a\u7528\u7684\u7f16\u7801\u540e\u518d\u64ad\u653e\u3002");
    case LocalPlaybackFailureKind::MissingDecoder:
        return QStringLiteral("\u8bf7\u5b89\u88c5\u7cfb\u7edf\u5a92\u4f53\u6269\u5c55\u6216\u5bf9\u5e94\u89e3\u7801\u5668\uff0c\u7136\u540e\u91cd\u65b0\u6253\u5f00\u6587\u4ef6\u3002");
    case LocalPlaybackFailureKind::Unknown:
        return QStringLiteral("\u8bf7\u5c1d\u8bd5\u91cd\u65b0\u6253\u5f00\u6587\u4ef6\uff1b\u5982\u679c\u4ecd\u7136\u5931\u8d25\uff0c\u53ef\u5c06\u6587\u4ef6\u8f6c\u6362\u4e3a\u5e38\u89c1\u7f16\u7801\u540e\u518d\u8bd5\u3002");
    }

    return QString();
}

LocalPlaybackFailureKind kindFromBackendError(QMediaPlayer::Error error, const QString& errorString)
{
    const QString lowerError = errorString.toLower();
    if (containsAny(lowerError,
                    QStringList{QStringLiteral("decoder not found"),
                                QStringLiteral("missing decoder"),
                                QStringLiteral("no decoder"),
                                QStringLiteral("codec not found"),
                                QStringLiteral("no suitable decoder")})) {
        return LocalPlaybackFailureKind::MissingDecoder;
    }

    if (containsAny(lowerError,
                    QStringList{QStringLiteral("corrupt"),
                                QStringLiteral("damaged"),
                                QStringLiteral("truncated"),
                                QStringLiteral("invalid data"),
                                QStringLiteral("moov atom not found"),
                                QStringLiteral("malformed")})) {
        return LocalPlaybackFailureKind::DamagedFile;
    }

    if (containsAny(lowerError,
                    QStringList{QStringLiteral("unsupported"),
                                QStringLiteral("not supported"),
                                QStringLiteral("codec"),
                                QStringLiteral("format"),
                                QStringLiteral("decode"),
                                QStringLiteral("demux")})) {
        return LocalPlaybackFailureKind::UnsupportedCodec;
    }

    switch (error) {
    case QMediaPlayer::AccessDeniedError:
        return LocalPlaybackFailureKind::FileNotReadable;
    case QMediaPlayer::FormatError:
        return LocalPlaybackFailureKind::UnsupportedCodec;
    case QMediaPlayer::ResourceError:
        return LocalPlaybackFailureKind::DamagedFile;
    case QMediaPlayer::NoError:
    case QMediaPlayer::NetworkError:
        break;
    }

    return LocalPlaybackFailureKind::Unknown;
}

QString buildMessage(const QString& filePath,
                     LocalPlaybackFailureKind kind,
                     const QString& errorString)
{
    QString message = QStringLiteral("\u65e0\u6cd5\u64ad\u653e\u201c%1\u201d\u3002\n\n"
                                     "\u53ef\u80fd\u539f\u56e0\uff1a%2\n\n"
                                     "\u5efa\u8bae\uff1a%3")
                          .arg(fileDisplayName(filePath),
                               reasonForKind(kind),
                               suggestionForKind(kind));

    const QString trimmedError = errorString.trimmed();
    if (!trimmedError.isEmpty()) {
        message += QStringLiteral("\n\n\u64ad\u653e\u5668\u8865\u5145\u4fe1\u606f\uff1a\n%1").arg(trimmedError);
    }

    message += LocalPlaybackDiagnostics::quickProbeNotice();
    return message;
}
}

QString LocalPlaybackDiagnostics::quickProbeNotice()
{
    return QStringLiteral("\n\n\u8bf4\u660e\uff1a\u672c\u5730\u6587\u4ef6\u9009\u62e9\u9636\u6bb5\u53ea\u505a\u5feb\u901f\u68c0\u67e5"
                          "\uff08\u5b58\u5728\u3001\u53ef\u8bfb\u3001\u975e\u7a7a\u3001\u6269\u5c55\u540d\uff09\u3002"
                          "\u5df2\u901a\u8fc7\u5feb\u901f\u68c0\u67e5\u4e0d\u4ee3\u8868\u7f16\u7801\u4e00\u5b9a\u53ef\u64ad\u653e\uff0c"
                          "\u5b9e\u9645\u64ad\u653e\u80fd\u529b\u53d6\u51b3\u4e8e\u7cfb\u7edf\u89e3\u7801\u5668\u3002");
}

QString LocalPlaybackDiagnostics::quickProbeStatusMessage(int acceptedFileCount)
{
    return QStringLiteral("\u5df2\u901a\u8fc7\u5feb\u901f\u68c0\u67e5\uff1a%1 \u4e2a\u672c\u5730\u5a92\u4f53\u6587\u4ef6\u5df2\u52a0\u5165\u3002"
                          "\u5b9e\u9645\u64ad\u653e\u80fd\u529b\u53d6\u51b3\u4e8e\u7cfb\u7edf\u89e3\u7801\u5668\uff1b"
                          "\u5982\u679c\u64ad\u653e\u5931\u8d25\uff0c\u4f1a\u7ed9\u51fa\u66f4\u5177\u4f53\u7684\u539f\u56e0\u3002")
        .arg(acceptedFileCount);
}

LocalPlaybackDiagnosis LocalPlaybackDiagnostics::diagnose(const QString& filePath,
                                                          QMediaPlayer::Error error,
                                                          const QString& errorString)
{
    LocalPlaybackFailureKind kind = LocalPlaybackFailureKind::Unknown;
    const QString normalizedPath = filePath.trimmed();
    const QFileInfo fileInfo(normalizedPath);

    if (normalizedPath.isEmpty() || !fileInfo.exists()) {
        kind = LocalPlaybackFailureKind::FileMissing;
    } else if (!fileInfo.isFile()) {
        kind = LocalPlaybackFailureKind::FileNotReadable;
    } else {
        QFile file(fileInfo.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            kind = LocalPlaybackFailureKind::FileNotReadable;
        } else if (fileInfo.size() == 0) {
            kind = LocalPlaybackFailureKind::EmptyOrDamaged;
        } else if (!hasSupportedExtension(fileInfo)) {
            kind = LocalPlaybackFailureKind::UnsupportedExtension;
        } else {
            kind = kindFromBackendError(error, errorString);
        }
    }

    LocalPlaybackDiagnosis diagnosis;
    diagnosis.kind = kind;
    diagnosis.title = titleForKind(kind);
    diagnosis.message = buildMessage(normalizedPath, kind, errorString);
    return diagnosis;
}
