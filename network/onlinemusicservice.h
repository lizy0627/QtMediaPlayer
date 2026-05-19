#ifndef ONLINEMUSICSERVICE_H
#define ONLINEMUSICSERVICE_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include "networkclient.h"
#include "searchcache.h"

class QWidget;
class NetworkClient;

struct SongInfo
{
    QString id;
    QString sourceId;
    QString sourceName;
    QString title;
    QString name;
    QString artist;
    QString album;
    QString url;
    QString sourceUrl;
    QString lyricUrl;
    int duration = 0;
};

class OnlineMusicService : public QObject
{
    Q_OBJECT

public:
    explicit OnlineMusicService(QObject* parent = nullptr);

    void searchMusic(QString keyword);
    void showSearchDialog(QWidget* parent);
    void search(const QString& keyword);
    void searchSongsAsync(const QString& keyword);
    void resolveSongUrlAsync(const QString& songId);

signals:
    void searchFinished(QStringList results);
    void searchFailed(const ServiceError& error);
    void searchError(QString message);
    void searchFinished(const QList<SongInfo>& songs, const QString& statusMessage);
    void resolveFinished(const SongInfo& song);
    void resolveError(const QString& songId, const QString& message);
    void songSelected(const SongInfo& song);

private slots:
    void onRequestFinished(const QString& requestId, const NetworkResult& result);

private:
    static QString buildSearchCacheKey(const QString& keyword);
    bool emitSearchResultsFromJson(const QByteArray& data,
                                   const QString& statusSuffix = QString());
    QList<SongInfo> parseSearchResults(const QByteArray& data,
                                       QString* statusMessage,
                                       bool* parseOk) const;
    SongInfo parseResolvedSongUrl(const QByteArray& data,
                                  const QString& songId,
                                  QString* statusMessage,
                                  bool* parseOk) const;

    NetworkClient* m_networkClient = nullptr;
    SearchCache m_searchCache;
    QString m_pendingSearchRequestId;
    QString m_pendingSearchKeyword;
    QString m_pendingSearchCacheKey;
    QHash<QString, QString> m_pendingResolveRequestIds;
};

#endif // ONLINEMUSICSERVICE_H
