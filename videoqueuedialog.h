#ifndef VIDEOQUEUEDIALOG_H
#define VIDEOQUEUEDIALOG_H

#include <QDialog>
#include <QStringList>

class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class VideoPlayerController;

class VideoQueueDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VideoQueueDialog(VideoPlayerController* controller, QWidget* parent = nullptr);

private slots:
    void refreshQueue(const QStringList& filePaths, int currentIndex);
    void playSelectedItem(QListWidgetItem* item);
    void removeSelectedItem();
    void updateActions();

private:
    int selectedQueueIndex() const;
    void markCurrentItem(int currentIndex);

    VideoPlayerController* m_controller = nullptr;
    QListWidget* m_listWidget = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QPushButton* m_removeButton = nullptr;
};

#endif // VIDEOQUEUEDIALOG_H
