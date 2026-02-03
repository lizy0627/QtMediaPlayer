#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QMessageBox>
#include <QDialog>
#include <QListWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QMenuBar>
#include <QMenu>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include "videoplayer.h"
#include "audioplayer.h"
#include "menu.h"
#include "playhistory.h"

QT_BEGIN_NAMESPACE
namespace Ui { 
    class Widget; 
}
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget();

    // 初始化菜单
    void initMenu();
    
    // 显示播放历史对话框
    void showPlayHistory();

private:
    Ui::Widget *ui;
    VideoPlayer* m_video;
    AudioPlayer* m_audio;
    PlayHistoryManager* m_historyManager;
};

#endif // WIDGET_H
