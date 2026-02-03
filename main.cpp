#include "widget.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    // 设置应用程序信息
    a.setApplicationName("Qt媒体播放器");
    a.setApplicationVersion("1.0.0");
    a.setOrganizationName("QtMediaPlayer");
    
    Widget w;
    w.resize(1200, 800);
    w.show();
    
    return a.exec();
}
