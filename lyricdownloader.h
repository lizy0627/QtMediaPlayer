#ifndef LYRICDOWNLOADER_H
#define LYRICDOWNLOADER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QUrlQuery>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include "lyricwidget.h"

// 在线歌词下载器
class LyricDownloader : public QObject
{
    Q_OBJECT
    
private:
    QNetworkAccessManager* m_networkManager;
    QString m_lastError;
    
public:
    explicit LyricDownloader(QObject* parent = nullptr)
        : QObject(parent)
    {
        m_networkManager = new QNetworkAccessManager(this);
    }
    
    ~LyricDownloader()
    {
        delete m_networkManager;
    }
    
    // 获取最后的错误信息
    QString lastError() const { return m_lastError; }
    
    // 从文件名提取歌曲信息
    struct SongInfo {
        QString title;      // 歌曲名
        QString artist;     // 艺术家
        
        SongInfo() {}
        SongInfo(const QString& t, const QString& a) : title(t), artist(a) {}
    };
    
    // 解析文件名获取歌曲信息
    // 支持格式：
    // 1. "艺术家 - 歌曲名.mp3"
    // 2. "歌曲名 - 艺术家.mp3"
    // 3. "歌曲名.mp3"
    static SongInfo parseSongInfo(const QString& filePath)
    {
        QFileInfo fileInfo(filePath);
        QString baseName = fileInfo.completeBaseName();
        
        // 尝试分割艺术家和歌曲名
        if (baseName.contains(" - ")) {
            QStringList parts = baseName.split(" - ");
            if (parts.size() >= 2) {
                QString part1 = parts[0].trimmed();
                QString part2 = parts[1].trimmed();
                
                // 假设第一部分是艺术家，第二部分是歌曲名
                return SongInfo(part2, part1);
            }
        }
        
        // 如果没有分隔符，整个文件名作为歌曲名
        return SongInfo(baseName, "");
    }
    
    // 搜索歌词（网易云音乐 API）
    // 注意：这是一个简化的实现，实际使用时可能需要处理更多细节
    QString searchAndDownloadLyric(const QString& songName, const QString& artistName = "")
    {
        m_lastError.clear();
        
        // 构建搜索关键词
        QString keyword = songName;
        if (!artistName.isEmpty()) {
            keyword = artistName + " " + songName;
        }
        
        qDebug() << "搜索歌词:" << keyword;
        
        // 使用网易云音乐 API（第三方接口）
        // 注意：这里使用的是公开的第三方 API，实际使用时请确保合法性
        QString searchUrl = QString("https://netease-cloud-music-api-psi-drab.vercel.app/search?keywords=%1&limit=1")
                           .arg(QString(keyword.toUtf8().toPercentEncoding()));
        
        QNetworkRequest request;
        request.setUrl(QUrl(searchUrl));
        request.setHeader(QNetworkRequest::UserAgentHeader, "QtMediaPlayer/1.0");
        
        QNetworkReply* reply = m_networkManager->get(request);
        
        // 等待响应
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        
        timer.start(10000); // 10秒超时
        loop.exec();
        
        if (!timer.isActive()) {
            m_lastError = "请求超时";
            reply->deleteLater();
            return QString();
        }
        timer.stop();
        
        if (reply->error() != QNetworkReply::NoError) {
            m_lastError = "网络错误: " + reply->errorString();
            qDebug() << m_lastError;
            reply->deleteLater();
            return QString();
        }
        
        QByteArray data = reply->readAll();
        reply->deleteLater();
        
        // 解析搜索结果
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) {
            m_lastError = "解析搜索结果失败";
            return QString();
        }
        
        QJsonObject obj = doc.object();
        QJsonObject result = obj["result"].toObject();
        QJsonArray songs = result["songs"].toArray();
        
        if (songs.isEmpty()) {
            m_lastError = "未找到歌曲";
            qDebug() << "未找到歌曲:" << keyword;
            return QString();
        }
        
        // 获取第一首歌曲的 ID
        QJsonObject song = songs[0].toObject();
        int songId = song["id"].toInt();
        
        qDebug() << "找到歌曲 ID:" << songId;
        
        // 获取歌词
        return downloadLyricById(songId);
    }
    
    // 根据歌曲 ID 下载歌词
    QString downloadLyricById(int songId)
    {
        QString lyricUrl = QString("https://netease-cloud-music-api-psi-drab.vercel.app/lyric?id=%1").arg(songId);
        
        QNetworkRequest request;
        request.setUrl(QUrl(lyricUrl));
        request.setHeader(QNetworkRequest::UserAgentHeader, "QtMediaPlayer/1.0");
        
        QNetworkReply* reply = m_networkManager->get(request);
        
        // 等待响应
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        
        timer.start(10000); // 10秒超时
        loop.exec();
        
        if (!timer.isActive()) {
            m_lastError = "请求超时";
            reply->deleteLater();
            return QString();
        }
        timer.stop();
        
        if (reply->error() != QNetworkReply::NoError) {
            m_lastError = "网络错误: " + reply->errorString();
            reply->deleteLater();
            return QString();
        }
        
        QByteArray data = reply->readAll();
        reply->deleteLater();
        
        // 解析歌词
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) {
            m_lastError = "解析歌词失败";
            return QString();
        }
        
        QJsonObject obj = doc.object();
        QJsonObject lrc = obj["lrc"].toObject();
        QString lyricText = lrc["lyric"].toString();
        
        if (lyricText.isEmpty()) {
            m_lastError = "歌词为空";
            return QString();
        }
        
        qDebug() << "成功下载歌词，长度:" << lyricText.length();
        return lyricText;
    }
    
    // 保存歌词到文件
    static bool saveLyricToFile(const QString& lyricText, const QString& audioFilePath)
    {
        QFileInfo audioInfo(audioFilePath);
        QString lrcPath = audioInfo.absolutePath() + "/" + audioInfo.completeBaseName() + ".lrc";
        
        QFile file(lrcPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qDebug() << "无法创建歌词文件:" << lrcPath;
            return false;
        }
        
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << lyricText;
        
        file.close();
        
        qDebug() << "歌词已保存到:" << lrcPath;
        return true;
    }
    
    // 自动下载并保存歌词
    bool autoDownloadLyric(const QString& audioFilePath)
    {
        // 解析歌曲信息
        SongInfo info = parseSongInfo(audioFilePath);
        
        qDebug() << "尝试下载歌词 - 歌曲:" << info.title << "艺术家:" << info.artist;
        
        // 搜索并下载歌词
        QString lyricText = searchAndDownloadLyric(info.title, info.artist);
        
        if (lyricText.isEmpty()) {
            qDebug() << "下载歌词失败:" << m_lastError;
            return false;
        }
        
        // 保存到文件
        return saveLyricToFile(lyricText, audioFilePath);
    }
    
signals:
    void downloadProgress(const QString& message);
    void downloadFinished(bool success, const QString& message);
};

#endif // LYRICDOWNLOADER_H
