#ifndef LOCALPLAYBACKDIAGNOSTICS_H
#define LOCALPLAYBACKDIAGNOSTICS_H

#include <QMediaPlayer>
#include <QString>

enum class LocalPlaybackFailureKind {
    FileMissing,
    FileNotReadable,
    EmptyOrDamaged,
    UnsupportedExtension,
    DamagedFile,
    UnsupportedCodec,
    MissingDecoder,
    Unknown
};

struct LocalPlaybackDiagnosis {
    LocalPlaybackFailureKind kind = LocalPlaybackFailureKind::Unknown;
    QString title;
    QString message;
};

class LocalPlaybackDiagnostics
{
public:
    static QString quickProbeNotice();
    static QString quickProbeStatusMessage(int acceptedFileCount);
    static LocalPlaybackDiagnosis diagnose(const QString& filePath,
                                           QMediaPlayer::Error error,
                                           const QString& errorString);
};

#endif // LOCALPLAYBACKDIAGNOSTICS_H
