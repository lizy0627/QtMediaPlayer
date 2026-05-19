#ifndef BILIBILISEARCHSERVICE_H
#define BILIBILISEARCHSERVICE_H

#include <QObject>
#include <QList>
#include <QString>

#include "networkclient.h"
#include "onlinevideotypes.h"
#include "searchcache.h"

class BilibiliSearchService : public QObject
{
    Q_OBJECT

public:
    explicit BilibiliSearchService(QObject* parent = nullptr);

    void searchVideo(const QString& keyword);

signals:
    void searchStarted(const QString& keyword);
    void searchFinished(const QList<VideoInfo>& videos, const QString& statusMessage);
    void searchFailed(const ServiceError& error);

private slots:
    void onRequestFinished(const QString& requestId, const NetworkResult& result);

private:
    static QVariantMap bilibiliHeaders();
    static QString removeHtmlTags(const QString& html);
    static QString formatPlayCount(int count);
    bool emitSearchResultsFromJson(const QByteArray& data,
                                   const QString& statusSuffix = QString());
    QList<VideoInfo> parseSearchResults(const QByteArray& data, QString* errorMessage) const;

    NetworkClient* m_networkClient = nullptr;
    SearchCache m_searchCache;
    QString m_pendingSearchRequestId;
    QString m_pendingSearchKeyword;
};

#endif // BILIBILISEARCHSERVICE_H
