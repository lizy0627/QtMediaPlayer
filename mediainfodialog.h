#ifndef MEDIAINFODIALOG_H
#define MEDIAINFODIALOG_H

#include <QDialog>
#include <QString>

class QGridLayout;

class MediaInfoDialog : public QDialog
{
public:
    explicit MediaInfoDialog(const QString& filePath, QWidget* parent = nullptr);

private:
    void addInfoRow(QGridLayout* layout, int row, const QString& name, const QString& value);
};

#endif // MEDIAINFODIALOG_H
