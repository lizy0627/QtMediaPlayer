#ifndef APPBOOTSTRAPPER_H
#define APPBOOTSTRAPPER_H

#include "appstartupstate.h"
#include "uitheme.h"

#include <QObject>

class QCoreApplication;

class AppBootstrapper : public QObject
{
    Q_OBJECT

public:
    explicit AppBootstrapper(QCoreApplication* app);

    AppStartupState initialize();
    const AppStartupState& startupState() const;

private:
    UiTheme m_theme;
    AppStartupState m_state;
};

#endif // APPBOOTSTRAPPER_H
