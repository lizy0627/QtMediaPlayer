#ifndef LYRICSERVICE_H
#define LYRICSERVICE_H

#include <QObject>
#include <QList>
#include <QString>

#include "lyricwidget.h"

class LyricDownloadService;
class NetworkClient;
struct NetworkResult;

class LyricService : public QObject
{
    Q_OBJECT

public:
    explicit LyricService(QObject* parent = nullptr);

    void loadLyricsForAudio(const QString& audioPath);
    void loadLyricsForAudio(const QString& audioPath, const QString& lyricUrl);
    QString currentAudioPath() const;

signals:
    void lyricsReady(const QString& audioPath, const QList<LyricLine>& lyrics);
    void lyricsCleared();
    void lyricsUnavailable(LyricDisplayState state, const QString& message);
    void statusMessage(const QString& message);

private slots:
    void onLyricDownloaded(const QString& audioPath, bool success, const QString& message);
    void onRequestFinished(const QString& requestId, const NetworkResult& result);

private:
    void loadLocalOrDownloadLyrics(const QString& audioPath, bool clearBeforeDownload);
    void handleOnlineLyricResponse(const NetworkResult& result);
    void emitUnavailable(LyricDisplayState state, const QString& message);

    LyricDownloadService* m_downloader = nullptr;
    NetworkClient* m_networkClient = nullptr;
    QString m_currentAudioPath;
    QString m_pendingOnlineLyricRequestId;
    QString m_pendingOnlineLyricUrl;
};

#endif // LYRICSERVICE_H
