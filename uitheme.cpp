#include "uitheme.h"

#include <QApplication>
#include <QDebug>
#include <QFile>

UiTheme::UiTheme(QObject* parent)
    : QObject(parent)
{
}

bool UiTheme::loadTheme(QString themeName)
{
    if (themeName.isEmpty()) {
        themeName = QStringLiteral("app");
    }

    const QString stylePath = QStringLiteral(":/styles/%1.qss").arg(themeName);
    QFile styleFile(stylePath);
    if (!styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to load UI theme:" << stylePath << styleFile.errorString();
        return false;
    }

    qApp->setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    emit themeChanged();
    return true;
}
