#ifndef LYRICDOWNLOADSERVICE_H
#define LYRICDOWNLOADSERVICE_H

#include <QHash>
#include <QObject>
#include <QString>

#include "networkclient.h"

class LyricDownloadService : public QObject
{
    Q_OBJECT

public:
    enum class LyricSaveStrategy {
        AppDataLocation,
        AudioDirectory
    };

    struct SongInfo
    {
        QString title;
        QString artist;

        SongInfo() = default;
        SongInfo(const QString& t, const QString& a)
            : title(t)
            , artist(a)
        {
        }
    };

    explicit LyricDownloadService(QObject* parent = nullptr);

    QString lastError() const;

    static SongInfo parseSongInfo(const QString& filePath);
    static bool saveLyricToFile(const QString& lyricText,
                                const QString& audioFilePath,
                                LyricSaveStrategy strategy = LyricSaveStrategy::AppDataLocation,
                                QString* savedPath = nullptr,
                                QString* errorMessage = nullptr);

    void autoDownloadLyricAsync(const QString& audioFilePath);
    void searchAndDownloadLyricAsync(const QString& songName,
                                     const QString& artistName = QString(),
                                     const QString& audioFilePath = QString());

signals:
    void downloadProgress(const QString& message);
    void downloadFinished(const QString& lyrics);
    void downloadFailed(const ServiceError& error);
    void downloadError(const ServiceError& error);
    void downloadError(const QString& message);
    void lyricDownloaded(const QString& audioFilePath, bool success, const QString& message);

private:
    enum class RequestStage {
        SearchSong,
        FetchLyric
    };

    struct PendingRequestContext
    {
        RequestStage stage = RequestStage::SearchSong;
        QString audioFilePath;
    };

    void downloadLyricByIdAsync(int songId, const QString& audioFilePath);
    void finish(const QString& audioFilePath,
                bool success,
                const QString& message,
                const QString& lyricText = QString(),
                const ServiceError& error = ServiceError());
    void handleSearchResponse(const NetworkResult& result, const PendingRequestContext& context);
    void handleLyricResponse(const NetworkResult& result, const PendingRequestContext& context);

private slots:
    void onRequestFinished(const QString& requestId, const NetworkResult& result);

private:
    NetworkClient* m_networkClient = nullptr;
    QHash<QString, PendingRequestContext> m_pendingRequests;
    QString m_lastError;
};

#endif // LYRICDOWNLOADSERVICE_H
