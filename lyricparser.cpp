#include "lyricparser.h"

#include <algorithm>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextStream>

namespace {
QString safeLyricBaseName(const QString& audioFilePath)
{
    QString baseName = QFileInfo(audioFilePath).completeBaseName().trimmed();
    if (baseName.isEmpty()) {
        baseName = audioFilePath.trimmed();
    }
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("untitled");
    }

    baseName.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|]+)")), QStringLiteral("_"));
    baseName.replace(QRegularExpression(QStringLiteral(R"(\s+)")), QStringLiteral(" "));
    return baseName.left(120).trimmed();
}

QString appLyricPathForAudio(const QString& audioFilePath)
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty()) {
        appDataPath = QDir::homePath() + QStringLiteral("/.QtMediaPlayer");
    }

    return QDir(appDataPath).filePath(QStringLiteral("lyrics/%1.lrc").arg(safeLyricBaseName(audioFilePath)));
}
}

QList<LyricLine> LyricParser::parseLrcText(const QString& lrcText)
{
    QList<LyricLine> lyrics;

    QRegularExpression timeRegex(R"(\[(\d{2}):(\d{2})(?:\.(\d{2,3}))?\])");
    const QStringList lines = lrcText.split(QRegularExpression(QStringLiteral("\\r?\\n")));

    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        QRegularExpressionMatchIterator it = timeRegex.globalMatch(line);
        QList<qint64> timestamps;

        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();

            const int minutes = match.captured(1).toInt();
            const int seconds = match.captured(2).toInt();
            int milliseconds = 0;

            if (!match.captured(3).isEmpty()) {
                const QString msStr = match.captured(3);
                milliseconds = msStr.length() == 2 ? msStr.toInt() * 10 : msStr.toInt();
            }

            timestamps.append((minutes * 60 + seconds) * 1000 + milliseconds);
        }

        QString lyricText = line.remove(timeRegex).trimmed();
        if (lyricText.startsWith(QLatin1Char('[')) && lyricText.contains(QLatin1Char(':'))) {
            continue;
        }

        for (qint64 timestamp : timestamps) {
            lyrics.append(LyricLine(timestamp, lyricText));
        }
    }

    std::sort(lyrics.begin(), lyrics.end(), [](const LyricLine& a, const LyricLine& b) {
        return a.timestamp < b.timestamp;
    });

    return lyrics;
}

QList<LyricLine> LyricParser::parseLrcFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open lyric file:" << filePath << file.errorString();
        return {};
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    const QList<LyricLine> lyrics = parseLrcText(in.readAll());

    qDebug() << "Parsed lyric file:" << filePath << "lines:" << lyrics.size();
    return lyrics;
}

QString LyricParser::findLyricFile(const QString& audioFilePath)
{
    const QFileInfo audioInfo(audioFilePath);
    const QString baseName = audioInfo.completeBaseName().trimmed();
    const QString safeBaseName = safeLyricBaseName(audioFilePath);
    const QString dirPath = audioInfo.absolutePath();

    if (!dirPath.isEmpty()) {
        const QString sameDirLrc = QDir(dirPath).filePath(baseName + QStringLiteral(".lrc"));
        if (QFile::exists(sameDirLrc)) {
            qDebug() << "Found lyric file:" << sameDirLrc;
            return sameDirLrc;
        }

        const QString lyricsSubDir = QDir(dirPath).filePath(QStringLiteral("lyrics/%1.lrc").arg(baseName));
        if (QFile::exists(lyricsSubDir)) {
            qDebug() << "Found lyric file:" << lyricsSubDir;
            return lyricsSubDir;
        }

        const QString lyricsSubDirCap = QDir(dirPath).filePath(QStringLiteral("Lyrics/%1.lrc").arg(baseName));
        if (QFile::exists(lyricsSubDirCap)) {
            qDebug() << "Found lyric file:" << lyricsSubDirCap;
            return lyricsSubDirCap;
        }
    }

    const QString appCachedLrc = appLyricPathForAudio(audioFilePath);
    if (QFile::exists(appCachedLrc)) {
        qDebug() << "Found lyric file:" << appCachedLrc;
        return appCachedLrc;
    }

    qDebug() << "No lyric file found for:" << audioFilePath;
    return QString();
}

QList<LyricLine> LyricParser::autoLoadLyrics(const QString& audioFilePath)
{
    const QString lyricFile = findLyricFile(audioFilePath);
    if (lyricFile.isEmpty()) {
        return {};
    }

    return parseLrcFile(lyricFile);
}

bool LyricParser::createSampleLyric(const QString& audioFilePath)
{
    const QFileInfo audioInfo(audioFilePath);
    const QString lrcPath = QDir(audioInfo.absolutePath())
                                .filePath(audioInfo.completeBaseName() + QStringLiteral(".lrc"));

    QFile file(lrcPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    out << "[ti:" << audioInfo.completeBaseName() << "]\n";
    out << "[ar:未知艺术家]\n";
    out << "[al:未知专辑]\n";
    out << "[by:QtMediaPlayer]\n\n";
    out << "[00:00.00]欢迎使用 QtMediaPlayer\n";
    out << "[00:05.00]这是自动生成的示例歌词\n";
    out << "[00:10.00]请将真实的 LRC 歌词文件\n";
    out << "[00:15.00]放在音频文件同目录下\n";
    out << "[00:20.00]文件名需要与音频文件相同\n";
    out << "[00:25.00]支持标准 LRC 格式\n";
    out << "[00:30.00]享受音乐，享受生活\n";

    qDebug() << "Created sample lyric file:" << lrcPath;
    return true;
}
