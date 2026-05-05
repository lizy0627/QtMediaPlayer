#ifndef PLAYLISTPANEL_H
#define PLAYLISTPANEL_H

#include <QGroupBox>

class QLabel;
class QListWidget;
class QListWidgetItem;
class PlaylistModel;
class QPoint;
class QPushButton;

class PlaylistPanel : public QGroupBox
{
    Q_OBJECT

public:
    explicit PlaylistPanel(QWidget* parent = nullptr);

    void setModel(PlaylistModel* model);
    void setCurrentRow(int row);
    int currentRow() const;
    QString currentSongName() const;
    void setLoggedInUser(const QString& username);

signals:
    void addFilesRequested();
    void onlineSearchRequested();
    void deleteSelectedRequested();
    void clearRequested();
    void testAudioRequested();
    void loginRequested();
    void songActivated(int row);

private slots:
    void refreshFromModel();
    void showContextMenu(const QPoint& pos);
    void onItemDoubleClicked(QListWidgetItem* item);

private:
    QPushButton* createActionButton(const QString& text, const char* role);

    QLabel* m_userLabel = nullptr;
    QPushButton* m_loginButton = nullptr;
    QListWidget* m_listWidget = nullptr;
    PlaylistModel* m_model = nullptr;
};

#endif // PLAYLISTPANEL_H
