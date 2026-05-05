#ifndef MYDANMAKUDIALOG_H
#define MYDANMAKUDIALOG_H

#include <QDialog>
#include <QString>
#include <QVector>

#include "danmakuitem.h"

class QCheckBox;
class QLabel;
class QListWidget;

class MyDanmakuDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MyDanmakuDialog(const QString& username,
                             const QString& currentVideoPath,
                             const QVector<DanmakuItem>& records,
                             QWidget* parent = nullptr);

signals:
    void locateRequested(const QString& videoPath, qint64 timestamp);

private:
    void renderRecords();
    void emitLocateForCurrentItem();
    QString formatTime(qint64 milliseconds) const;
    QString typeText(int type) const;

    QString m_username;
    QString m_currentVideoPath;
    QVector<DanmakuItem> m_records;
    QLabel* m_titleLabel = nullptr;
    QCheckBox* m_currentVideoOnlyCheck = nullptr;
    QListWidget* m_listWidget = nullptr;
};

#endif // MYDANMAKUDIALOG_H
