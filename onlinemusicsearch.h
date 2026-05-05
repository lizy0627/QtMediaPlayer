#ifndef ONLINEMUSICSEARCH_H
#define ONLINEMUSICSEARCH_H

#include <QDialog>
#include <QList>

#include "onlinemusicservice.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;

class OnlineMusicSearch : public QDialog
{
    Q_OBJECT

public:
    explicit OnlineMusicSearch(OnlineMusicService* service, QWidget* parent = nullptr);

    SongInfo getSelectedSong() const;

signals:
    void songSelected(const SongInfo& song);

private slots:
    void onSearch();
    void onSearchFinished(const QList<SongInfo>& songs, const QString& statusMessage);
    void onSearchError(const QString& message);
    void onPlaySelected();

private:
    void setupUI();
    void showSongs(const QList<SongInfo>& songs);
    QString formatSongDisplay(const SongInfo& song) const;

    OnlineMusicService* m_service = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_searchButton = nullptr;
    QListWidget* m_resultList = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;
    QList<SongInfo> m_songs;
};

#endif // ONLINEMUSICSEARCH_H
