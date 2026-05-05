#ifndef UNIFIEDHISTORYDIALOG_H
#define UNIFIEDHISTORYDIALOG_H

#include <QDialog>
#include <QVector>

#include "mediahistory.h"

class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTabBar;

class UnifiedHistoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UnifiedHistoryDialog(MediaHistoryService* historyService,
                                  QWidget* parent = nullptr,
                                  MediaKind initialFilter = MediaKind::Unknown,
                                  bool lockFilter = false);

signals:
    void playRequested(const MediaHistoryRecord& record);
    void playAudio(const QString& filePath);
    void playVideo(const QString& filePath, qint64 position);

private slots:
    void reload();
    void playSelected();
    void removeSelected();
    void clearCurrentFilter();

private:
    void setupUi();
    MediaKind currentFilter() const;
    QVector<MediaHistoryRecord> recordsForCurrentFilter() const;
    void addRecordItem(const MediaHistoryRecord& record, int index);
    QString displayNameForKind(MediaKind kind) const;
    QString displayTextForRecord(const MediaHistoryRecord& record) const;
    bool confirmMissingFileRemoval(const QString& filePath);
    QListWidgetItem* currentPlayableItem() const;
    int tabIndexForFilter(MediaKind kind) const;

    MediaHistoryService* m_historyService = nullptr;
    MediaKind m_initialFilter = MediaKind::Unknown;
    bool m_lockFilter = false;
    QTabBar* m_filterTabs = nullptr;
    QListWidget* m_listWidget = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_playButton = nullptr;
    QPushButton* m_removeButton = nullptr;
    QPushButton* m_clearButton = nullptr;
    QVector<MediaHistoryRecord> m_records;
};

#endif // UNIFIEDHISTORYDIALOG_H
