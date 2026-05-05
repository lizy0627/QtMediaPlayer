#ifndef DANMAKUITEM_H
#define DANMAKUITEM_H

#include <QDateTime>
#include <QMetaType>
#include <QString>

class DanmakuItem
{
public:
    explicit DanmakuItem(QString text = QString(), int time = 0);

    QString text() const;
    int time() const;

    int id = 0;
    QString mediaId;
    QString videoPath;
    QString username;
    QString content;
    qint64 timestamp = 0;
    QString color = QStringLiteral("#FFFFFF");
    int fontSize = 25;
    int type = 0;
    QDateTime createTime;
};

Q_DECLARE_METATYPE(DanmakuItem)

#endif // DANMAKUITEM_H
