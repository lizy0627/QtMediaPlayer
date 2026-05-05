#ifndef ONLINEVIDEOSEARCH_H
#define ONLINEVIDEOSEARCH_H

#include <QDialog>
#include <QList>

#include "onlinevideotypes.h"

class QLabel;
class QLineEdit;
class QListWidget;
class OnlineVideoService;
class QProgressBar;
class QPushButton;
class QWidget;

class OnlineVideoSearch : public QDialog
{
    Q_OBJECT

public:
    explicit OnlineVideoSearch(OnlineVideoService* service, QWidget* parent = nullptr);

    VideoInfo getSelectedVideo() const;

signals:
    void videoSelected(const VideoInfo& video);

private slots:
    void onSearch();
    void onSearchFinished(const QList<VideoInfo>& videos, const QString& statusMessage);
    void onSearchFailed(const QString& message);
    void onPlaySelected();
    void onOpenPageSelected();

private:
    void setupUI();
    QString formatVideoDisplay(const VideoInfo& video) const;

    OnlineVideoService* m_service = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_searchButton = nullptr;
    QListWidget* m_resultList = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;
    QList<VideoInfo> m_videos;
};

#endif // ONLINEVIDEOSEARCH_H
