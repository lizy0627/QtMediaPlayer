#include "appbootstrapper.h"
#include "widget.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    AppBootstrapper bootstrapper(&a);
    const AppStartupState startupState = bootstrapper.initialize();
    
    Widget w(startupState);
    w.resize(1200, 800);
    w.show();
    
    return a.exec();
}
