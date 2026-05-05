#ifndef LYRICDOWNLOADER_H
#define LYRICDOWNLOADER_H

#include <QObject>
#include <QString>

#include "network/lyricdownloadservice.h"

class Q_DECL_DEPRECATED_X("Use LyricService instead.") LyricDownloader : public QObject
{
    Q_OBJECT

public:
    using SongInfo = LyricDownloadService::SongInfo;

    explicit LyricDownloader(QObject* parent = nullptr)
        : QObject(parent)
        , m_service(new LyricDownloadService(this))
    {
        connect(m_service, &LyricDownloadService::downloadProgress,
                this, &LyricDownloader::downloadProgress);
        connect(m_service, &LyricDownloadService::lyricDownloaded,
                this, &LyricDownloader::lyricDownloaded);
        connect(m_service, qOverload<const QString&>(&LyricDownloadService::downloadError),
                this, [this](const QString& message) {
                    emit downloadFinished(false, message);
                });
        connect(m_service, &LyricDownloadService::downloadFinished,
                this, [this](const QString&) {
                    emit downloadFinished(true, QStringLiteral("歌词下载成功。"));
                });
    }

    QString lastError() const
    {
        return m_service->lastError();
    }

    static SongInfo parseSongInfo(const QString& filePath)
    {
        return LyricDownloadService::parseSongInfo(filePath);
    }

    void autoDownloadLyricAsync(const QString& audioFilePath)
    {
        m_service->autoDownloadLyricAsync(audioFilePath);
    }

    void searchAndDownloadLyricAsync(const QString& songName,
                                     const QString& artistName = QString(),
                                     const QString& audioFilePath = QString())
    {
        m_service->searchAndDownloadLyricAsync(songName, artistName, audioFilePath);
    }

    static bool saveLyricToFile(
        const QString& lyricText,
        const QString& audioFilePath,
        LyricDownloadService::LyricSaveStrategy strategy = LyricDownloadService::LyricSaveStrategy::AppDataLocation)
    {
        return LyricDownloadService::saveLyricToFile(lyricText, audioFilePath, strategy);
    }

signals:
    void downloadProgress(const QString& message);
    void downloadFinished(bool success, const QString& message);
    void lyricDownloaded(const QString& audioFilePath, bool success, const QString& message);

private:
    LyricDownloadService* m_service = nullptr;
};

#endif // LYRICDOWNLOADER_H
