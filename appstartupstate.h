#ifndef APPSTARTUPSTATE_H
#define APPSTARTUPSTATE_H

#include <QString>

struct AppStartupState
{
    bool themeLoaded = false;
    QString themeError;
    bool databaseAvailable = false;
    QString databaseError;
};

#endif // APPSTARTUPSTATE_H
