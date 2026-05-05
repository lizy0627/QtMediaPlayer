#ifndef UITHEME_H
#define UITHEME_H

#include <QObject>
#include <QString>

class UiTheme : public QObject
{
    Q_OBJECT

public:
    explicit UiTheme(QObject* parent = nullptr);

    bool loadTheme(QString themeName);

signals:
    void themeChanged();
};

#endif // UITHEME_H
