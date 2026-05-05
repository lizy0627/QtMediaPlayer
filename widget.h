#ifndef WIDGET_H
#define WIDGET_H

#include "appstartupstate.h"

#include <QString>
#include <QWidget>

class AudioPlayer;
class AuthService;
class AuthDialogController;
class QLabel;
class MainWindowController;
class MediaHistoryService;
class QMenuBar;
class UserSession;
class VideoPlayerWidget;

using VideoPlayer = VideoPlayerWidget;

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
    explicit Widget(const AppStartupState& startupState, QWidget *parent = nullptr);
    ~Widget();

    // 初始化菜单
    void initMenu();
    
    // 显示播放历史对话框
    void showPlayHistory();

private:
    void setupMainMenu(QMenuBar *menuBar);
    void showStartupStatus(const QString& message);
    void showStartupWarnings();
    void showAboutDialog();

    Ui::Widget *ui;
    VideoPlayer* m_video;
    AudioPlayer* m_audio;
    MediaHistoryService* m_historyService;
    UserSession* m_userSession;
    AuthService* m_authService;
    AuthDialogController* m_authDialogController = nullptr;
    MainWindowController* m_controller = nullptr;
    QLabel* m_statusLabel = nullptr;
    AppStartupState m_startupState;
};

#endif // WIDGET_H
