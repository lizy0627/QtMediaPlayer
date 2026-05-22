#include "widget.h"
#include "ui_widget.h"

#include "authdialogcontroller.h"
#include "authservice.h"
#include "audioplayer.h"
#include "mainwindowcontroller.h"
#include "mediahistory.h"
#include "menu.h"
#include "unifiedhistorydialog.h"
#include "usersession.h"
#include "videoplayer.h"

#include <QApplication>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <dwmapi.h>
#endif

Widget::Widget(QWidget *parent)
    : Widget(AppStartupState(), parent)
{
}

Widget::Widget(const AppStartupState& startupState, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
    , m_startupState(startupState)
{
    ui->setupUi(this);

    // 初始化播放历史管理器
    m_historyService = new MediaHistoryService(this);
    m_userSession = new UserSession(this);
    m_authService = new AuthService(m_userSession, this);
    m_authDialogController = new AuthDialogController(m_authService, this);
    m_authService->initialize();

    initMenu();
    setWindowTitle("Qt 影音娱乐系统 - 基于 Qt6.5.3");
    setWindowIcon(QIcon(":/assets/logo.png"));

    m_video = new VideoPlayer(ui->page_video,
                              m_userSession,
                              m_authService,
                              m_authDialogController,
                              m_historyService);
    m_audio = new AudioPlayer(ui->page_audio,
                              m_userSession,
                              m_authService,
                              m_authDialogController,
                              m_historyService);
    m_controller = new MainWindowController(ui->st,
                                            ui->page_video,
                                            ui->page_audio,
                                            m_video,
                                            m_audio,
                                            m_historyService,
                                            this);
    connect(m_controller,
            &MainWindowController::localMediaProbeNoticeRequested,
            this,
            [this](const QString& message) {
                showStartupStatus(message);
            });

    if (!m_startupState.databaseAvailable) {
        showStartupStatus(QStringLiteral("数据库不可用，登录/历史/弹幕功能可能无法使用"));
        QTimer::singleShot(0, this, &Widget::showStartupWarnings);
    }
}

Widget::~Widget()
{
    delete ui;
}

// 显示播放历史对话框
void Widget::showPlayHistory()
{
    UnifiedHistoryDialog dialog(m_historyService, this);
    connect(&dialog, &UnifiedHistoryDialog::playRequested, this, [this](const MediaHistoryRecord& record) {
        m_controller->playFromHistory(record);
    });
    dialog.exec();
}

// 初始化菜单
void Widget::initMenu()
{
    // 在菜单栏中创建三个菜单项
    QMenuBar *menuBar = new QMenuBar(this);
    setupMainMenu(menuBar);

    // 创建布局
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setVisible(false);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: #fff3cd; color: #664d03; padding: 6px 12px; border-top: 1px solid #ffecb5; }"));

    layout->addWidget(menuBar);
    layout->addWidget(ui->st, 1);
    layout->addWidget(m_statusLabel);
    this->setLayout(layout);

#if defined(Q_OS_WIN)
    if (HWND hwnd = (HWND)winId()) {
        COLORREF color = RGB(51, 65, 92);
        DwmSetWindowAttribute(hwnd, 35, &color, sizeof(color));
    }
#endif
}

void Widget::setupMainMenu(QMenuBar *menuBar)
{
    QMenu *fileMenu = menuBar->addMenu("文件(&F)");
    QMenu *playerMenu = menuBar->addMenu("播放器(&E)");
    QMenu *helpMenu = menuBar->addMenu("帮助(&H)");

    Menu *m = new Menu(fileMenu);
    m->createActionGroup({
        {"打开媒体", ":/assets/open.png", [this]() { m_controller->openLocalMediaFiles(this); }},
        {"播放历史", ":/assets/disc.png", [this]() { showPlayHistory(); }},
        {"退出", ":/assets/exit.png", []() { QApplication::quit(); }}
    }, true);

    m = new Menu(playerMenu);
    m->createActionGroup({
        {"视频播放器", ":/assets/video.png", [this]() { m_controller->showVideoPage(); }},
        {"音频播放器", ":/assets/audio.png", [this]() { m_controller->showAudioPage(); }}
    }, true);

    m = new Menu(helpMenu);
    m->createAction("关于", ":/assets/about.png", [this]() { showAboutDialog(); });
}

void Widget::showStartupStatus(const QString& message)
{
    if (!m_statusLabel) {
        return;
    }

    m_statusLabel->setText(message);
    m_statusLabel->setVisible(true);
}

void Widget::showStartupWarnings()
{
    if (m_startupState.databaseAvailable) {
        return;
    }

    QString message = QStringLiteral("数据库不可用，登录/历史/弹幕功能可能无法使用");
    if (!m_startupState.databaseError.trimmed().isEmpty()) {
        message += QStringLiteral("\n\n错误详情：%1").arg(m_startupState.databaseError);
    }
    QMessageBox::warning(this, QStringLiteral("启动提示"), message);
}

void Widget::showAboutDialog()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("关于");
    msgBox.setIconPixmap(QPixmap(":/assets/logo.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    QString aboutText =
        "<h2 style='color: #2196F3;'>Qt 影音娱乐系统</h2>"
        "<p style='font-size: 11pt;'>"
        "<b>版本:</b> 1.2.0<br>"
        "<b>基于:</b> Qt 6.5.3<br>"
        "<b>开发框架:</b> Qt6 + OpenCV (可选)<br>"
        "</p>"

        "<h3 style='color: #4CAF50; margin-top: 15px;'>视频播放功能</h3>"
        "<ul style='margin: 5px 0; padding-left: 20px;'>"
        "<li>多格式支持 (MP4/AVI/MKV/MOV/FLV/WMV/WEBM等)</li>"
        "<li>倍速播放 (0.5x - 2.0x)</li>"
        "<li>视频滤镜效果 (黑白/复古/模糊/锐化等)</li>"
        "<li>在线视频搜索与播放</li>"
        "<li>播放历史记录与进度恢复</li>"
        "<li>视频截图功能 (PNG格式)</li>"
        "<li>视频录制功能 (支持MP4/图片序列)</li>"
        "</ul>"

        "<h3 style='color: #FF9800; margin-top: 15px;'>音频播放功能</h3>"
        "<ul style='margin: 5px 0; padding-left: 20px;'>"
        "<li>多格式音频播放</li>"
        "<li>模拟频谱视觉效果</li>"
        "<li>在线歌词搜索与显示</li>"
        "<li>歌词同步滚动</li>"
        "<li>播放列表管理</li>"
        "<li>多种播放模式 (单曲循环/列表循环/随机播放)</li>"
        "<li>专辑封面显示</li>"
        "</ul>"

        "<h3 style='color: #9C27B0; margin-top: 15px;'>高级特性</h3>"
        "<ul style='margin: 5px 0; padding-left: 20px;'>"
        "<li>播放历史记录与统计</li>"
        "<li>视频播放进度自动保存</li>"
        "<li>FFmpeg视频编码支持 (可选)</li>"
        "<li>现代化UI设计</li>"
        "<li>跨平台支持 (Windows/Linux/macOS)</li>"
        "</ul>"

        "<p style='margin-top: 15px; color: #666; font-size: 9pt;'>"
        "提示: 安装FFmpeg可启用视频录制自动转换为MP4功能<br>"
        "反馈与建议: 欢迎提出您的宝贵意见"
        "</p>";

    msgBox.setText(aboutText);
    msgBox.setStandardButtons(QMessageBox::Ok);

    msgBox.setStyleSheet(
        "QMessageBox { background-color: #f5f5f5; }"
        "QLabel { color: #333; }"
    );
    if (auto* okButton = msgBox.button(QMessageBox::Ok)) {
        okButton->setProperty("role", "primary");
        okButton->setMinimumWidth(80);
    }

    msgBox.exec();
}
