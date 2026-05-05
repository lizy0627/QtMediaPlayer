#ifndef AUDIODIALOGSERVICE_H
#define AUDIODIALOGSERVICE_H

#include <QObject>
#include <QString>
#include <QStringList>

class QWidget;

class AudioDialogService : public QObject
{
    Q_OBJECT

public:
    explicit AudioDialogService(QObject* parent = nullptr);

    QString requestAlbumArtFile(QWidget* parent) const;
    QStringList requestAudioFiles(QWidget* parent) const;
    bool confirm(QWidget* parent, const QString& title, const QString& text) const;
    void showInformation(QWidget* parent, const QString& title, const QString& text) const;
    void showWarning(QWidget* parent, const QString& title, const QString& text) const;
    void showCritical(QWidget* parent, const QString& title, const QString& text) const;
};

#endif // AUDIODIALOGSERVICE_H
