#include "audiodialogservice.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QWidget>

AudioDialogService::AudioDialogService(QObject* parent)
    : QObject(parent)
{
}

QString AudioDialogService::requestAlbumArtFile(QWidget* parent) const
{
    return QFileDialog::getOpenFileName(
        parent,
        QStringLiteral("选择专辑封面"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"));
}

QStringList AudioDialogService::requestAudioFiles(QWidget* parent) const
{
    return QFileDialog::getOpenFileNames(
        parent,
        QStringLiteral("选择音频文件"),
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
        QStringLiteral("音频文件 (*.mp3 *.wav *.flac *.ogg *.m4a *.aac)"));
}

bool AudioDialogService::confirm(QWidget* parent, const QString& title, const QString& text) const
{
    return QMessageBox::question(parent, title, text, QMessageBox::Yes | QMessageBox::No)
        == QMessageBox::Yes;
}

void AudioDialogService::showInformation(QWidget* parent, const QString& title, const QString& text) const
{
    QMessageBox::information(parent, title, text);
}

void AudioDialogService::showWarning(QWidget* parent, const QString& title, const QString& text) const
{
    QMessageBox::warning(parent, title, text);
}

void AudioDialogService::showCritical(QWidget* parent, const QString& title, const QString& text) const
{
    QMessageBox::critical(parent, title, text);
}
