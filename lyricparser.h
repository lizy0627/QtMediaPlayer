#ifndef LYRICPARSER_H
#define LYRICPARSER_H

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QList>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include "lyricwidget.h"

// LRC 歌词解析器
class LyricParser
{
public:
    // 解析 LRC 格式歌词文件
    static QList<LyricLine> parseLrcFile(const QString& filePath)
    {
        QList<LyricLine> lyrics;
        
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "无法打开歌词文件:" << filePath;
            return lyrics;
        }
        
        QTextStream in(&file);
        in.setEncoding(QStringConverter::Utf8);
        
        // LRC 时间标签正则表达式: [mm:ss.xx] 或 [mm:ss]
        QRegularExpression timeRegex(R"(\[(\d{2}):(\d{2})(?:\.(\d{2,3}))?\])");
        
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            
            if (line.isEmpty()) continue;
            
            // 查找所有时间标签
            QRegularExpressionMatchIterator it = timeRegex.globalMatch(line);
            QList<qint64> timestamps;
            
            while (it.hasNext()) {
                QRegularExpressionMatch match = it.next();
                
                int minutes = match.captured(1).toInt();
                int seconds = match.captured(2).toInt();
                int milliseconds = 0;
                
                if (!match.captured(3).isEmpty()) {
                    QString msStr = match.captured(3);
                    // 处理两位或三位毫秒
                    if (msStr.length() == 2) {
                        milliseconds = msStr.toInt() * 10;
                    } else {
                        milliseconds = msStr.toInt();
                    }
                }
                
                qint64 totalMs = (minutes * 60 + seconds) * 1000 + milliseconds;
                timestamps.append(totalMs);
            }
            
            // 提取歌词文本（移除所有时间标签）
            QString lyricText = line;
            lyricText.remove(timeRegex).trimmed();
            
            // 跳过元数据标签（如 [ti:], [ar:], [al:] 等）
            if (lyricText.startsWith("[") && lyricText.contains(":")) {
                continue;
            }
            
            // 为每个时间戳创建歌词行
            for (qint64 timestamp : timestamps) {
                lyrics.append(LyricLine(timestamp, lyricText));
            }
        }
        
        file.close();
        
        // 按时间戳排序
        std::sort(lyrics.begin(), lyrics.end(), 
                  [](const LyricLine& a, const LyricLine& b) {
                      return a.timestamp < b.timestamp;
                  });
        
        qDebug() << "成功解析歌词文件，共" << lyrics.size() << "行";
        return lyrics;
    }
    
    // 自动查找歌词文件
    // 支持的查找规则：
    // 1. 同名 .lrc 文件（如 song.mp3 -> song.lrc）
    // 2. 同目录下的 lyrics 子文件夹
    static QString findLyricFile(const QString& audioFilePath)
    {
        QFileInfo audioInfo(audioFilePath);
        QString baseName = audioInfo.completeBaseName();
        QString dirPath = audioInfo.absolutePath();
        
        // 规则1: 同名 .lrc 文件
        QString sameDirLrc = dirPath + "/" + baseName + ".lrc";
        if (QFile::exists(sameDirLrc)) {
            qDebug() << "找到歌词文件:" << sameDirLrc;
            return sameDirLrc;
        }
        
        // 规则2: lyrics 子文件夹中的同名文件
        QString lyricsSubDir = dirPath + "/lyrics/" + baseName + ".lrc";
        if (QFile::exists(lyricsSubDir)) {
            qDebug() << "找到歌词文件:" << lyricsSubDir;
            return lyricsSubDir;
        }
        
        // 规则3: Lyrics 子文件夹（大写）
        QString lyricsSubDirCap = dirPath + "/Lyrics/" + baseName + ".lrc";
        if (QFile::exists(lyricsSubDirCap)) {
            qDebug() << "找到歌词文件:" << lyricsSubDirCap;
            return lyricsSubDirCap;
        }
        
        qDebug() << "未找到歌词文件:" << audioFilePath;
        return QString();
    }
    
    // 自动加载并解析歌词
    static QList<LyricLine> autoLoadLyrics(const QString& audioFilePath)
    {
        QString lyricFile = findLyricFile(audioFilePath);
        
        if (lyricFile.isEmpty()) {
            return QList<LyricLine>();
        }
        
        return parseLrcFile(lyricFile);
    }
    
    // 生成示例歌词文件（用于测试）
    static bool createSampleLyric(const QString& audioFilePath)
    {
        QFileInfo audioInfo(audioFilePath);
        QString lrcPath = audioInfo.absolutePath() + "/" + audioInfo.completeBaseName() + ".lrc";
        
        QFile file(lrcPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }
        
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        
        // 写入示例歌词
        out << "[ti:" << audioInfo.completeBaseName() << "]\n";
        out << "[ar:未知艺术家]\n";
        out << "[al:未知专辑]\n";
        out << "[by:QtMediaPlayer]\n";
        out << "\n";
        out << "[00:00.00]欢迎使用 QtMediaPlayer\n";
        out << "[00:05.00]这是自动生成的示例歌词\n";
        out << "[00:10.00]请将真实的 LRC 歌词文件\n";
        out << "[00:15.00]放在音频文件同目录下\n";
        out << "[00:20.00]文件名需要与音频文件相同\n";
        out << "[00:25.00]支持标准 LRC 格式\n";
        out << "[00:30.00]享受音乐，享受生活 ♪\n";
        
        file.close();
        
        qDebug() << "已创建示例歌词文件:" << lrcPath;
        return true;
    }
};

#endif // LYRICPARSER_H
