#include "videoencoder.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <QtGlobal>

namespace {
QString cleanEnvironmentPath(QString path)
{
    path = path.trimmed();
    if (path.size() >= 2 && path.startsWith(QLatin1Char('"')) && path.endsWith(QLatin1Char('"'))) {
        path = path.mid(1, path.size() - 2);
    }
    return QDir::fromNativeSeparators(path);
}

QString ffmpegExecutableFromPath(const QString& path)
{
    if (path.isEmpty()) {
        return QString();
    }

    const QFileInfo fileInfo(path);
    if (fileInfo.isDir()) {
#ifdef Q_OS_WIN
        return QDir(path).filePath(QStringLiteral("ffmpeg.exe"));
#else
        return QDir(path).filePath(QStringLiteral("ffmpeg"));
#endif
    }

    return path;
}

bool isUsableFFmpeg(const QString& path)
{
    const QFileInfo fileInfo(path);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }

    QProcess process;
    process.start(fileInfo.absoluteFilePath(), QStringList() << QStringLiteral("-version"));
    if (!process.waitForStarted(2000)) {
        return false;
    }
    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished(1000);
        return false;
    }
    return process.exitCode() == 0;
}
}

VideoEncoder::VideoEncoder(QObject* parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    findFFmpeg();

    connect(m_process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &VideoEncoder::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &VideoEncoder::onProcessError);
}

bool VideoEncoder::isFFmpegAvailable() const
{
    return !m_ffmpegPath.isEmpty() && QFile::exists(m_ffmpegPath);
}

bool VideoEncoder::isBusy() const
{
    return m_process->state() != QProcess::NotRunning;
}

QString VideoEncoder::getFFmpegPath() const
{
    return m_ffmpegPath;
}

bool VideoEncoder::startConvertToVideo(const QString& inputDir, const QString& outputPath, int fps)
{
    if (isBusy()) {
        emit conversionFailed(outputPath, QStringLiteral("\u5df2\u6709\u8f6c\u7801\u4efb\u52a1\u6b63\u5728\u6267\u884c\u3002"));
        return false;
    }

    if (!isFFmpegAvailable()) {
        emit conversionFailed(outputPath, QStringLiteral("\u672a\u627e\u5230 FFmpeg\uff0c\u65e0\u6cd5\u8f6c\u6362\u753b\u9762\u5f55\u5236\u89c6\u9891\u3002"));
        return false;
    }

    const QDir dir(inputDir);
    if (!dir.exists()) {
        emit conversionFailed(outputPath, QStringLiteral("\u753b\u9762\u5f55\u5236\u5e27\u76ee\u5f55\u4e0d\u5b58\u5728\u3002"));
        return false;
    }

    const QStringList imageFiles = dir.entryList(QStringList() << QStringLiteral("frame_*.png"),
                                                 QDir::Files,
                                                 QDir::Name);
    if (imageFiles.isEmpty()) {
        emit conversionFailed(outputPath, QStringLiteral("\u753b\u9762\u5f55\u5236\u76ee\u5f55\u4e2d\u6ca1\u6709\u53ef\u7528\u5e27\u3002"));
        return false;
    }

    m_pendingOutputPath = outputPath;
    QStringList arguments;
    arguments << QStringLiteral("-y")
              << QStringLiteral("-framerate") << QString::number(fps)
              << QStringLiteral("-i") << dir.filePath(QStringLiteral("frame_%05d.png"))
              << QStringLiteral("-vf") << QStringLiteral("scale=trunc(iw/2)*2:trunc(ih/2)*2")
              << QStringLiteral("-c:v") << QStringLiteral("libx264")
              << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p")
              << QStringLiteral("-preset") << QStringLiteral("fast")
              << QStringLiteral("-crf") << QStringLiteral("23")
              << outputPath;

    m_process->setWorkingDirectory(QFileInfo(outputPath).absolutePath());
    m_process->start(m_ffmpegPath, arguments);

    if (!m_process->waitForStarted(5000)) {
        QString message = QStringLiteral("FFmpeg \u542f\u52a8\u5931\u8d25\u3002");
        const QString processError = m_process->errorString().trimmed();
        if (!processError.isEmpty()) {
            message += QLatin1Char('\n') + processError;
        }
        if (!m_pendingOutputPath.isEmpty()) {
            emitConversionFailed(message);
        }
        return false;
    }

    return true;
}

void VideoEncoder::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_pendingOutputPath.isEmpty()) {
        return;
    }

    const QFileInfo outputInfo(m_pendingOutputPath);
    if (exitStatus == QProcess::NormalExit && exitCode == 0 && outputInfo.exists() && outputInfo.size() > 0) {
        emit conversionFinished(outputInfo.absoluteFilePath());
        m_pendingOutputPath.clear();
        return;
    }

    QString message = QStringLiteral("FFmpeg \u8f6c\u7801\u5931\u8d25\u3002");
    const QString processError = QString::fromLocal8Bit(m_process->readAllStandardError()).trimmed();
    if (!processError.isEmpty()) {
        message += QLatin1Char('\n') + processError;
    }

    emitConversionFailed(message);
}

void VideoEncoder::onProcessError(QProcess::ProcessError error)
{
    Q_UNUSED(error)

    if (m_pendingOutputPath.isEmpty()) {
        return;
    }

    QString message = QStringLiteral("FFmpeg \u8fd0\u884c\u5931\u8d25\u3002");
    const QString processError = m_process->errorString().trimmed();
    if (!processError.isEmpty()) {
        message += QLatin1Char('\n') + processError;
    }

    emitConversionFailed(message);
}

void VideoEncoder::findFFmpeg()
{
    QStringList candidates;

    const QString environmentPath = ffmpegExecutableFromPath(cleanEnvironmentPath(qEnvironmentVariable("FFMPEG_PATH")));
    if (!environmentPath.isEmpty()) {
        candidates << environmentPath;
    }

    const QString executable = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (!executable.isEmpty()) {
        candidates << executable;
    }

    candidates << QCoreApplication::applicationDirPath() + QStringLiteral("/ffmpeg.exe")
               << QCoreApplication::applicationDirPath() + QStringLiteral("/ffmpeg/ffmpeg.exe")
               << QStringLiteral("C:/ffmpeg/ffmpeg-8.0.1-essentials_build/bin/ffmpeg.exe")
               << QStringLiteral("C:/ffmpeg/bin/ffmpeg.exe")
               << QStringLiteral("C:/Program Files/ffmpeg/bin/ffmpeg.exe");

    for (const QString& candidate : std::as_const(candidates)) {
        const QString path = ffmpegExecutableFromPath(candidate);
        if (!isUsableFFmpeg(path)) {
            continue;
        }

        m_ffmpegPath = QFileInfo(path).absoluteFilePath();
        return;
    }
}

void VideoEncoder::emitConversionFailed(const QString& message)
{
    emit conversionFailed(m_pendingOutputPath, message);
    m_pendingOutputPath.clear();
}
