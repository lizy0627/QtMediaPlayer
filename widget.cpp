#include "widget.h"
#include "ui_widget.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#include <dwmapi.h>
#endif

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    
    // 初始化播放历史管理器
    m_historyManager = new PlayHistoryManager(this);
    
    initMenu();
    setWindowTitle("Qt 影音娱乐系统 - 基于 Qt6.5.3");
    setWindowIcon(QIcon("./assets/logo.png"));
    
    m_video = new VideoPlayer(ui->page_video);
    m_audio = new AudioPlayer(ui->page_audio);
}

Widget::~Widget()
{
    delete ui;
}

// 显示播放历史对话框
void Widget::showPlayHistory()
{
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("播放历史");
    dialog->setMinimumSize(700, 500);
    dialog->setStyleSheet(
        "QDialog { background-color: #2b2b2b; }"
        "QListWidget { background-color: #1e1e1e; color: #ffffff; border: 1px solid #444; border-radius: 5px; padding: 5px; }"
        "QListWidget::item { padding: 8px; border-bottom: 1px solid #333; }"
        "QListWidget::item:hover { background-color: #3a3a3a; }"
        "QListWidget::item:selected { background-color: #0d47a1; }"
        "QPushButton { background-color: #0d47a1; color: white; border: none; padding: 8px 16px; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background-color: #1565c0; }"
        "QPushButton:pressed { background-color: #0a3d91; }"
        "QLabel { color: #ffffff; font-size: 14px; font-weight: bold; }"
    );
    
    QVBoxLayout *layout = new QVBoxLayout(dialog);
    layout->setSpacing(10);
    layout->setContentsMargins(15, 15, 15, 15);
    
    // 标题标签
    QLabel *titleLabel = new QLabel("最近播放记录", dialog);
    titleLabel->setStyleSheet("font-size: 16px; color: #64b5f6; margin-bottom: 5px;");
    layout->addWidget(titleLabel);
    
    // 历史记录列表
    QListWidget *historyList = new QListWidget(dialog);
    layout->addWidget(historyList);
    
    // 获取历史记录
    QVector<HistoryItem> history = m_historyManager->getRecentHistory(50);
    
    if (history.isEmpty()) {
        QListWidgetItem *item = new QListWidgetItem("暂无播放记录");
        item->setTextAlignment(Qt::AlignCenter);
        item->setFlags(Qt::NoItemFlags);
        historyList->addItem(item);
    } else {
        for (const auto &histItem : history) {
            QString displayText = QString("%1\n类型: %2 | 播放次数: %3 | 最后播放: %4")
                .arg(histItem.fileName)
                .arg(histItem.fileType == "video" ? "视频" : "音频")
                .arg(histItem.playCount)
                .arg(histItem.lastPlayTime.toString("yyyy-MM-dd hh:mm"));
            
            QListWidgetItem *item = new QListWidgetItem(displayText);
            item->setData(Qt::UserRole, histItem.filePath);
            item->setData(Qt::UserRole + 1, histItem.fileType);
            historyList->addItem(item);
        }
    }
    
    // 按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    
    QPushButton *playButton = new QPushButton("播放选中", dialog);
    QPushButton *clearButton = new QPushButton("清空历史", dialog);
    QPushButton *closeButton = new QPushButton("关闭", dialog);
    
    buttonLayout->addWidget(playButton);
    buttonLayout->addWidget(clearButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    
    layout->addLayout(buttonLayout);
    
    // 连接信号
    connect(playButton, &QPushButton::clicked, [=]() {
        QListWidgetItem *currentItem = historyList->currentItem();
        if (currentItem && currentItem->flags() != Qt::NoItemFlags) {
            QString filePath = currentItem->data(Qt::UserRole).toString();
            QString fileType = currentItem->data(Qt::UserRole + 1).toString();
            
            if (QFileInfo::exists(filePath)) {
                if (fileType == "video") {
                    ui->st->setCurrentWidget(ui->page_video);
                    m_video->open(filePath);
                    m_audio->audioPause();
                } else if (fileType == "audio") {
                    ui->st->setCurrentWidget(ui->page_audio);
                    m_audio->addFiles(QStringList() << filePath);
                    m_video->pause();
                }
                
                // 更新历史记录
                m_historyManager->addOrUpdateHistory(filePath, fileType);
                dialog->accept();
            } else {
                QMessageBox::warning(dialog, "错误", "文件不存在或已被删除！");
            }
        }
    });
    
    connect(clearButton, &QPushButton::clicked, [=]() {
        QMessageBox::StandardButton reply = QMessageBox::question(
            dialog, "确认", "确定要清空所有播放历史吗？",
            QMessageBox::Yes | QMessageBox::No
        );
        
        if (reply == QMessageBox::Yes) {
            m_historyManager->clearHistory();
            historyList->clear();
            QListWidgetItem *item = new QListWidgetItem("暂无播放记录");
            item->setTextAlignment(Qt::AlignCenter);
            item->setFlags(Qt::NoItemFlags);
            historyList->addItem(item);
        }
    });
    
    connect(closeButton, &QPushButton::clicked, dialog, &QDialog::accept);
    
    // 双击播放
    connect(historyList, &QListWidget::itemDoubleClicked, [=](QListWidgetItem *item) {
        if (item->flags() != Qt::NoItemFlags) {
            playButton->click();
        }
    });
    
    dialog->exec();
    delete dialog;
}

// 初始化菜单
void Widget::initMenu()
{
    // 在菜单栏中创建三个菜单项
    QMenuBar *menuBar = new QMenuBar(this);
    QMenu *fileMenu = menuBar->addMenu("文件(&F)");
    QMenu *playerMenu = menuBar->addMenu("播放器(&E)");
    QMenu *helpMenu = menuBar->addMenu("帮助(&H)");

    // 文件菜单
    Menu *m = new Menu(fileMenu);
    m->createActionGroup({
        {"打开", "./assets/open.png", [=]() {
            // 创建一个文件对话框
            QFileDialog fileDialog;
            fileDialog.setDirectory(QDir::homePath());
            fileDialog.setNameFilter("视频文件 (*.mp4 *.avi *.mkv *.mov *.flv *.wmv)");

            if (fileDialog.exec()) {
                QStringList lst = fileDialog.selectedFiles();
                if(lst.size() > 0) {
                    QString filePath = lst.at(0);
                    m_video->open(filePath);
                    m_historyManager->addOrUpdateHistory(filePath, "video");
                }
            }
        }},
        {"播放历史", "./assets/disc.png", [=](){ showPlayHistory(); }},
        {"退出", "./assets/exit.png", [=](){ exit(0); }}
    }, true);

    // 播放器菜单
    m = new Menu(playerMenu);
    m->createActionGroup({
        {"视频播放器", "./assets/video.png", [=]() {
            ui->st->setCurrentWidget(ui->page_video);
            m_audio->audioPause();
        }},
        {"音频播放器", "./assets/audio.png", [=]() { 
            ui->st->setCurrentWidget(ui->page_audio); 
            m_video->pause(); 
        }}
    }, true);

    // 关于菜单
    m = new Menu(helpMenu);
    m->createAction("关于", "./assets/about.png", [=]() {
        QMessageBox::information(this, "关于", 
            "Qt 影音娱乐系统\n\n"
            "版本: 1.0.0\n"
            "基于: Qt 6.5.3\n\n"
            "功能特性:\n"
            "• 视频播放（支持多种格式）\n"
            "• 音频播放（支持频谱可视化）\n"
            "• 播放历史记录\n"
            "• 视频滤镜效果\n"
            "• 倍速播放");
    });

    // 创建布局
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(menuBar);
    layout->addWidget(ui->st, 1);
    this->setLayout(layout);

#if defined(Q_OS_WIN)
    if (HWND hwnd = (HWND)winId()) {
        COLORREF color = RGB(51, 65, 92);
        DwmSetWindowAttribute(hwnd, 35, &color, sizeof(color));
    }
#endif
}
