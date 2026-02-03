#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include <QWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QFileDialog>
#include <QTime>
#include <QIcon>
#include <QToolButton>
#include <QGroupBox>
#include <QPainter>
#include <QStandardPaths>
#include <QMouseEvent>
#include <QMessageBox>
#include <QFileInfo>
#include <QUrl>
#include <QRandomGenerator>
#include <QTransform>
#include <QPixmap>
#include <QImage>
#include <QEvent>
#include <QMenu>
#include <QAction>
#include "spectrumwidget.h"
#include "lyricwidget.h"
#include "lyricparser.h"
#include "lyricdownloader.h"
#include "onlinemusicsearch.h"

// 枚举播放模式
enum PlayMode
{
    SingleLoop,     // 单曲循环
    Random,         // 随机播放
    ListLoop        // 列表循环
};

// Qt6 音频播放器
class AudioPlayer : public QWidget
{
    Q_OBJECT
private:
    // UI组件
    QLabel *m_albumArt;             // 播放器图片
    QListWidget *m_playListWidget;  // 播放列表
    SpectrumWidget *m_spectrumWidget; // 频谱可视化组件
    LyricWidget *m_lyricWidget;     // 歌词显示组件
    LyricDownloader *m_lyricDownloader; // 歌词下载器

    // 控制按钮
    QPushButton *m_btnPlayPause;    // 播放/暂停
    QPushButton *m_btnPrev;         // 上一首
    QPushButton *m_btnNext;         // 下一首

    // 播放模式按钮
    QToolButton *m_btnLoopList;     // 列表循环
    QToolButton *m_btnLoopSingle;   // 单曲循环
    QToolButton *m_btnRandom;       // 随机播放

    // 进度控制
    QSlider *m_progressSlider;      // 进度条
    QLabel *m_currentTime;          // 当前时间
    QLabel *m_totalTime;            // 总时间
    
    // 音量控制
    QSlider *m_volumeSlider;        // 音量滑块
    QLabel *m_volumeLabel;          // 音量标签

    // 媒体组件（Qt6）
    QMediaPlayer *m_player;         // 媒体播放器
    QAudioOutput *m_audioOutput;    // 音频输出
    QList<QUrl> m_playlist;         // 播放列表
    int m_currentIndex;             // 当前播放索引

    // 状态
    PlayMode m_playMode;            // 当前播放模式
    QWidget* m_parent = nullptr;    // 父控件
    QString m_customAlbumArtPath;   // 自定义专辑封面路径
    QPixmap m_customAlbumArt;       // 自定义专辑封面

public:
    explicit AudioPlayer(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_currentIndex(-1)
    {
        // 初始化媒体播放器（Qt6）
        m_player = new QMediaPlayer(this);
        m_audioOutput = new QAudioOutput(this);
        m_player->setAudioOutput(m_audioOutput);
        
        // 设置音量（0.0 到 1.0，默认设置为 0.8）
        m_audioOutput->setVolume(0.8);
        
        // 调试信息：检查音频输出设备
        qDebug() << "=== 音频播放器初始化 ===";
        qDebug() << "音频输出设备:" << m_audioOutput->device().description();
        qDebug() << "初始音量:" << m_audioOutput->volume();
        qDebug() << "是否静音:" << m_audioOutput->isMuted();
        
        // 初始化歌词下载器
        m_lyricDownloader = new LyricDownloader(this);

        // 设置初始播放模式
        m_playMode = ListLoop;

        // 创建UI
        createUI();
        setupConnections();

        m_btnPlayPause->setIcon(QIcon("./assets/pause.png"));
        m_btnPlayPause->setIconSize(QSize(48, 48));

        m_parent = parent;
        auto layout = new QVBoxLayout(parent);
        layout->addWidget(this);
        parent->setLayout(layout);

        // 初始状态
        updatePlayModeUI();
        
        qDebug() << "=== 音频播放器初始化完成 ===";
    }

    // 添加文件到播放列表
    void addFiles(const QStringList &files)
    {
        foreach (const QString &file, files)
        {
            QFileInfo fileInfo(file);
            if (fileInfo.exists())
            {
                m_playlist.append(QUrl::fromLocalFile(file));
                m_playListWidget->addItem(fileInfo.fileName());
            }
        }

        if (m_playlist.isEmpty()) return;

        // 如果当前没有播放，自动播放第一首
        if (m_player->playbackState() != QMediaPlayer::PlayingState)
        {
            m_currentIndex = 0;
            play();
        }
    }

    // 暂停播放器
    void audioPause()
    {
        m_player->pause();
        m_btnPlayPause->setIcon(QIcon("./assets/play.png"));
        m_btnPlayPause->setIconSize(QSize(48, 48));
    }

private:
    // 创建UI界面
    void createUI()
    {
        // 主布局
        QHBoxLayout *mainLayout = new QHBoxLayout(this);
        mainLayout->setContentsMargins(10, 10, 10, 10);
        mainLayout->setSpacing(10);

        // 分割器
        QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

        // 左侧：图片和频谱可视化
        QWidget *leftPanel = new QWidget(this);
        QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
        leftLayout->setContentsMargins(0, 0, 0, 0);
        leftLayout->setSpacing(10);
        
        // 专辑封面容器
        QWidget *albumContainer = new QWidget(leftPanel);
        QVBoxLayout *albumLayout = new QVBoxLayout(albumContainer);
        albumLayout->setContentsMargins(0, 0, 0, 0);
        albumLayout->setSpacing(5);
        
        m_albumArt = new QLabel(albumContainer);
        m_albumArt->setMinimumSize(400, 300);
        m_albumArt->setAlignment(Qt::AlignCenter);
        m_albumArt->setStyleSheet(
            "QLabel { "
            "   background-color: #333; "
            "   border-radius: 10px; "
            "   border: 2px solid #555; "
            "}"
            "QLabel:hover { "
            "   border: 2px solid #0d47a1; "
            "   background-color: #3a3a3a; "
            "}"
        );
        m_albumArt->setCursor(Qt::PointingHandCursor);
        m_albumArt->setToolTip("点击更换专辑封面");
        setDefaultAlbumArt();
        
        m_albumArt->installEventFilter(this);
        
        albumLayout->addWidget(m_albumArt);
        
        // 添加更换封面按钮
        QPushButton *changeAlbumBtn = new QPushButton("🖼️ 更换专辑封面", albumContainer);
        changeAlbumBtn->setStyleSheet(
            "QPushButton { "
            "   background-color: #0d47a1; "
            "   color: white; "
            "   border: none; "
            "   padding: 8px; "
            "   border-radius: 5px; "
            "   font-weight: bold; "
            "}"
            "QPushButton:hover { "
            "   background-color: #1565c0; "
            "}"
            "QPushButton:pressed { "
            "   background-color: #0a3d91; "
            "}"
        );
        connect(changeAlbumBtn, &QPushButton::clicked, this, &AudioPlayer::changeAlbumArt);
        albumLayout->addWidget(changeAlbumBtn);
        
        leftLayout->addWidget(albumContainer);
        
        // 添加频谱可视化组件
        m_spectrumWidget = new SpectrumWidget(leftPanel);
        m_spectrumWidget->setMinimumHeight(150);
        leftLayout->addWidget(m_spectrumWidget);
        
        // 添加歌词显示组件
        m_lyricWidget = new LyricWidget(leftPanel);
        m_lyricWidget->setMinimumHeight(200);
        leftLayout->addWidget(m_lyricWidget);

        splitter->addWidget(leftPanel);

        // 右侧：播放列表和控制面板
        QWidget *rightPanel = new QWidget(this);
        QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
        rightLayout->setContentsMargins(0, 0, 0, 0);
        rightLayout->setSpacing(10);

        // 播放列表
        QGroupBox *playlistGroup = new QGroupBox("播放列表", rightPanel);
        QVBoxLayout *playlistLayout = new QVBoxLayout(playlistGroup);
        playlistLayout->setContentsMargins(5, 15, 5, 5);

        m_playListWidget = new QListWidget(playlistGroup);
        m_playListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_playListWidget, &QListWidget::customContextMenuRequested, 
                this, &AudioPlayer::showPlaylistContextMenu);
        playlistLayout->addWidget(m_playListWidget);

        // 添加文件按钮
        QHBoxLayout *addButtonLayout = new QHBoxLayout();
        
        QPushButton *addButton = new QPushButton("📁 添加本地音乐", playlistGroup);
        addButton->setStyleSheet(
            "QPushButton { "
            "   background-color: #0d47a1; "
            "   color: white; "
            "   border: none; "
            "   padding: 10px; "
            "   border-radius: 5px; "
            "   font-weight: bold; "
            "}"
            "QPushButton:hover { "
            "   background-color: #1565c0; "
            "}"
            "QPushButton:pressed { "
            "   background-color: #0a3d91; "
            "}"
        );
        connect(addButton, &QPushButton::clicked, this, &AudioPlayer::onAddFiles);
        
        QPushButton *searchButton = new QPushButton("🔍 在线搜索", playlistGroup);
        searchButton->setStyleSheet(
            "QPushButton { "
            "   background-color: #1565c0; "
            "   color: white; "
            "   border: none; "
            "   padding: 10px; "
            "   border-radius: 5px; "
            "   font-weight: bold; "
            "}"
            "QPushButton:hover { "
            "   background-color: #1976d2; "
            "}"
            "QPushButton:pressed { "
            "   background-color: #0d47a1; "
            "}"
        );
        connect(searchButton, &QPushButton::clicked, this, &AudioPlayer::onSearchOnline);
        
        addButtonLayout->addWidget(addButton);
        addButtonLayout->addWidget(searchButton);
        playlistLayout->addLayout(addButtonLayout);
        
        // 删除和测试按钮
        QHBoxLayout *actionButtonLayout = new QHBoxLayout();
        
        QPushButton *deleteButton = new QPushButton("🗑️ 删除选中", playlistGroup);
        deleteButton->setStyleSheet(
            "QPushButton { "
            "   background-color: #d32f2f; "
            "   color: white; "
            "   border: none; "
            "   padding: 8px; "
            "   border-radius: 5px; "
            "   font-weight: bold; "
            "   font-size: 9pt; "
            "}"
            "QPushButton:hover { "
            "   background-color: #f44336; "
            "}"
            "QPushButton:pressed { "
            "   background-color: #c62828; "
            "}"
        );
        connect(deleteButton, &QPushButton::clicked, this, &AudioPlayer::deleteSelectedSong);
        
        // 添加测试按钮
        QPushButton *testButton = new QPushButton("🔧 测试音频", playlistGroup);
        testButton->setStyleSheet(
            "QPushButton { "
            "   background-color: #f57c00; "
            "   color: white; "
            "   border: none; "
            "   padding: 8px; "
            "   border-radius: 5px; "
            "   font-weight: bold; "
            "   font-size: 9pt; "
            "}"
            "QPushButton:hover { "
            "   background-color: #fb8c00; "
            "}"
            "QPushButton:pressed { "
            "   background-color: #e65100; "
            "}"
        );
        connect(testButton, &QPushButton::clicked, this, &AudioPlayer::testAudio);
        
        actionButtonLayout->addWidget(deleteButton);
        actionButtonLayout->addWidget(testButton);
        playlistLayout->addLayout(actionButtonLayout);

        rightLayout->addWidget(playlistGroup);

        // 控制面板
        QGroupBox *controlGroup = new QGroupBox("播放控制", rightPanel);
        QVBoxLayout *controlLayout = new QVBoxLayout(controlGroup);
        controlLayout->setContentsMargins(10, 15, 10, 10);

        // 播放模式按钮组
        QHBoxLayout *modeLayout = new QHBoxLayout();
        m_btnLoopList = new QToolButton(controlGroup);
        m_btnLoopList->setText("列表循环");
        m_btnLoopList->setCheckable(true);
        m_btnLoopList->setToolTip("列表循环");

        m_btnLoopSingle = new QToolButton(controlGroup);
        m_btnLoopSingle->setText("单曲循环");
        m_btnLoopSingle->setCheckable(true);
        m_btnLoopSingle->setToolTip("单曲循环");

        m_btnRandom = new QToolButton(controlGroup);
        m_btnRandom->setText("随机播放");
        m_btnRandom->setCheckable(true);
        m_btnRandom->setToolTip("随机播放");

        modeLayout->addWidget(m_btnLoopList);
        modeLayout->addWidget(m_btnLoopSingle);
        modeLayout->addWidget(m_btnRandom);
        modeLayout->addStretch();
        controlLayout->addLayout(modeLayout);

        // 进度条和时间显示
        QHBoxLayout *progressLayout = new QHBoxLayout();
        m_currentTime = new QLabel("00:00", controlGroup);
        m_currentTime->setFixedWidth(50);
        m_currentTime->setAlignment(Qt::AlignCenter);

        m_progressSlider = new QSlider(Qt::Horizontal, controlGroup);
        m_progressSlider->setRange(0, 100);

        m_totalTime = new QLabel("00:00", controlGroup);
        m_totalTime->setFixedWidth(50);
        m_totalTime->setAlignment(Qt::AlignCenter);

        progressLayout->addWidget(m_currentTime);
        progressLayout->addWidget(m_progressSlider);
        progressLayout->addWidget(m_totalTime);
        controlLayout->addLayout(progressLayout);

        // 播放控制按钮
        QVBoxLayout *buttonContainerLayout = new QVBoxLayout();
        buttonContainerLayout->setSpacing(8);
        
        QHBoxLayout *buttonLayout = new QHBoxLayout();
        buttonLayout->setAlignment(Qt::AlignCenter);
        buttonLayout->setSpacing(20);

        // 上一首按钮
        QVBoxLayout *prevLayout = new QVBoxLayout();
        prevLayout->setAlignment(Qt::AlignCenter);
        prevLayout->setSpacing(5);
        
        m_btnPrev = new QPushButton(controlGroup);
        m_btnPrev->setIcon(QIcon("./assets/pre.png"));
        m_btnPrev->setIconSize(QSize(40, 40));
        m_btnPrev->setFixedSize(60, 60);
        m_btnPrev->setToolTip("上一首");
        m_btnPrev->setStyleSheet(
            "QPushButton { "
            "   background-color: #1565c0; "
            "   border: 2px solid #0d47a1; "
            "   border-radius: 30px; "
            "}"
            "QPushButton:hover { "
            "   background-color: #1976d2; "
            "   border: 2px solid #1565c0; "
            "}"
            "QPushButton:pressed { "
            "   background-color: #0d47a1; "
            "}"
        );
        
        QLabel *prevLabel = new QLabel("上一首", controlGroup);
        prevLabel->setAlignment(Qt::AlignCenter);
        prevLabel->setStyleSheet("color: #64b5f6; font-weight: bold; font-size: 10pt;");
        
        prevLayout->addWidget(m_btnPrev);
        prevLayout->addWidget(prevLabel);

        // 播放/暂停按钮
        QVBoxLayout *playLayout = new QVBoxLayout();
        playLayout->setAlignment(Qt::AlignCenter);
        playLayout->setSpacing(5);
        
        m_btnPlayPause = new QPushButton(controlGroup);
        m_btnPlayPause->setIcon(QIcon("./assets/play.png"));
        m_btnPlayPause->setIconSize(QSize(48, 48));
        m_btnPlayPause->setFixedSize(70, 70);
        m_btnPlayPause->setToolTip("播放/暂停");
        m_btnPlayPause->setStyleSheet(
            "QPushButton { "
            "   background-color: #0d47a1; "
            "   border: 3px solid #1565c0; "
            "   border-radius: 35px; "
            "}"
            "QPushButton:hover { "
            "   background-color: #1565c0; "
            "   border: 3px solid #1976d2; "
            "}"
            "QPushButton:pressed { "
            "   background-color: #0a3d91; "
            "}"
        );
        
        QLabel *playLabel = new QLabel("播放/暂停", controlGroup);
        playLabel->setAlignment(Qt::AlignCenter);
        playLabel->setStyleSheet("color: #64b5f6; font-weight: bold; font-size: 10pt;");
        
        playLayout->addWidget(m_btnPlayPause);
        playLayout->addWidget(playLabel);

        // 下一首按钮
        QVBoxLayout *nextLayout = new QVBoxLayout();
        nextLayout->setAlignment(Qt::AlignCenter);
        nextLayout->setSpacing(5);
        
        QPixmap pix = QPixmap("./assets/pre.png");
        QTransform tf;
        tf.scale(-1, 1);
        tf.translate(-pix.width(), 0);
        pix = pix.transformed(tf);

        m_btnNext = new QPushButton(controlGroup);
        m_btnNext->setIcon(QIcon(pix));
        m_btnNext->setIconSize(QSize(40, 40));
        m_btnNext->setFixedSize(60, 60);
        m_btnNext->setToolTip("下一首");
        m_btnNext->setStyleSheet(
            "QPushButton { "
            "   background-color: #1565c0; "
            "   border: 2px solid #0d47a1; "
            "   border-radius: 30px; "
            "}"
            "QPushButton:hover { "
            "   background-color: #1976d2; "
            "   border: 2px solid #1565c0; "
            "}"
            "QPushButton:pressed { "
            "   background-color: #0d47a1; "
            "}"
        );
        
        QLabel *nextLabel = new QLabel("下一首", controlGroup);
        nextLabel->setAlignment(Qt::AlignCenter);
        nextLabel->setStyleSheet("color: #64b5f6; font-weight: bold; font-size: 10pt;");
        
        nextLayout->addWidget(m_btnNext);
        nextLayout->addWidget(nextLabel);

        buttonLayout->addLayout(prevLayout);
        buttonLayout->addLayout(playLayout);
        buttonLayout->addLayout(nextLayout);
        
        buttonContainerLayout->addLayout(buttonLayout);
        controlLayout->addLayout(buttonContainerLayout);
        
        // 音量控制
        QHBoxLayout *volumeLayout = new QHBoxLayout();
        volumeLayout->setSpacing(10);
        
        QLabel *volumeIcon = new QLabel("🔊", controlGroup);
        volumeIcon->setObjectName("volumeIcon");  // 设置对象名称方便查找
        volumeIcon->setStyleSheet("font-size: 16pt;");
        volumeIcon->setFixedWidth(30);
        
        m_volumeSlider = new QSlider(Qt::Horizontal, controlGroup);
        m_volumeSlider->setRange(0, 100);
        m_volumeSlider->setValue(80);  // 默认音量80%
        m_volumeSlider->setToolTip("音量控制");
        m_volumeSlider->setStyleSheet(
            "QSlider::groove:horizontal { "
            "   border: 1px solid #444; "
            "   height: 8px; "
            "   background: #1e1e1e; "
            "   border-radius: 4px; "
            "}"
            "QSlider::handle:horizontal { "
            "   background: #64b5f6; "
            "   border: 2px solid #0d47a1; "
            "   width: 18px; "
            "   margin: -5px 0; "
            "   border-radius: 9px; "
            "}"
            "QSlider::handle:horizontal:hover { "
            "   background: #90caf9; "
            "}"
            "QSlider::sub-page:horizontal { "
            "   background: #0d47a1; "
            "   border-radius: 4px; "
            "}"
        );
        
        m_volumeLabel = new QLabel("80%", controlGroup);
        m_volumeLabel->setFixedWidth(45);
        m_volumeLabel->setAlignment(Qt::AlignCenter);
        m_volumeLabel->setStyleSheet(
            "color: #64b5f6; "
            "font-weight: bold; "
            "font-size: 10pt;"
        );
        
        volumeLayout->addWidget(volumeIcon);
        volumeLayout->addWidget(m_volumeSlider);
        volumeLayout->addWidget(m_volumeLabel);
        
        controlLayout->addLayout(volumeLayout);

        rightLayout->addWidget(controlGroup);

        splitter->addWidget(rightPanel);

        // 设置分割比例
        splitter->setStretchFactor(0, 2);
        splitter->setStretchFactor(1, 1);

        mainLayout->addWidget(splitter);
    }

    // 设置默认图片
    void setDefaultAlbumArt()
    {
        if (!m_customAlbumArt.isNull()) {
            QPixmap scaled = m_customAlbumArt.scaled(
                m_albumArt->size(), 
                Qt::KeepAspectRatio, 
                Qt::SmoothTransformation
            );
            m_albumArt->setPixmap(scaled);
            return;
        }
        
        QPixmap pixmap(400, 400);
        pixmap.fill(Qt::darkGray);

        QPainter painter(&pixmap);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 24));
        painter.drawText(pixmap.rect(), Qt::AlignCenter, "专辑封面");
        painter.drawImage(pixmap.rect(), QImage("./assets/disc.png"));

        m_albumArt->setPixmap(pixmap);
    }
    
    // 事件过滤器
    bool eventFilter(QObject *obj, QEvent *event) override
    {
        if (obj == m_albumArt && event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                changeAlbumArt();
                return true;
            }
        }
        return QWidget::eventFilter(obj, event);
    }

    // 连接信号槽
    void setupConnections()
    {
        // 播放控制按钮
        connect(m_btnPlayPause, &QPushButton::clicked, this, &AudioPlayer::togglePlay);
        connect(m_btnPrev, &QPushButton::clicked, this, &AudioPlayer::prev);
        connect(m_btnNext, &QPushButton::clicked, this, &AudioPlayer::next);

        // 播放模式按钮
        connect(m_btnLoopList, &QToolButton::clicked, [this](){ setPlayMode(ListLoop); });
        connect(m_btnLoopSingle, &QToolButton::clicked, [this](){ setPlayMode(SingleLoop); });
        connect(m_btnRandom, &QToolButton::clicked, [this](){ setPlayMode(Random); });
        
        // 音量控制
        connect(m_volumeSlider, &QSlider::valueChanged, this, &AudioPlayer::onVolumeChanged);

        // 播放器信号（Qt6）
        connect(m_player, &QMediaPlayer::positionChanged, this, &AudioPlayer::updatePosition);
        connect(m_player, &QMediaPlayer::durationChanged, this, &AudioPlayer::updateDuration);
        connect(m_player, &QMediaPlayer::playbackStateChanged, this, &AudioPlayer::updatePlayButton);
        connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &AudioPlayer::onMediaStatusChanged);
        
        // 错误处理
        connect(m_player, &QMediaPlayer::errorOccurred, this, &AudioPlayer::onPlayerError);

        // 连接频谱可视化
        m_spectrumWidget->setMediaPlayer(m_player);
        
        // 连接歌词同步
        connect(m_player, &QMediaPlayer::positionChanged, m_lyricWidget, &LyricWidget::updatePosition);

        // 播放列表选择
        connect(m_playListWidget, &QListWidget::itemDoubleClicked, [this](QListWidgetItem *item)
        {
            int row = m_playListWidget->row(item);
            m_currentIndex = row;
            play();
        });

        // 进度条拖动
        connect(m_progressSlider, &QSlider::sliderMoved, this, &AudioPlayer::seek);
    }

    // 格式化时间显示
    QString formatTime(qint64 milliseconds)
    {
        int seconds = milliseconds / 1000;
        int minutes = seconds / 60;
        seconds %= 60;
        return QString("%1:%2").arg(minutes, 2, 10, QLatin1Char('0'))
                              .arg(seconds, 2, 10, QLatin1Char('0'));
    }

private slots:
    // 更换专辑封面
    void changeAlbumArt()
    {
        QString fileName = QFileDialog::getOpenFileName(
            this,
            "选择专辑封面",
            QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
            "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"
        );
        
        if (!fileName.isEmpty()) {
            QPixmap pixmap(fileName);
            if (!pixmap.isNull()) {
                m_customAlbumArt = pixmap;
                m_customAlbumArtPath = fileName;
                
                QPixmap scaled = pixmap.scaled(
                    m_albumArt->size(), 
                    Qt::KeepAspectRatio, 
                    Qt::SmoothTransformation
                );
                m_albumArt->setPixmap(scaled);
                
                m_albumArt->setToolTip("专辑封面已更新\n点击可再次更换");
            } else {
                QMessageBox::warning(this, "错误", "无法加载图片文件！");
            }
        }
    }
    
    // 添加文件
    void onAddFiles()
    {
        QStringList files = QFileDialog::getOpenFileNames(this,
            "选择音频文件",
            QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
            "音频文件 (*.mp3 *.wav *.flac *.ogg *.m4a *.aac)");

        if (!files.isEmpty())
            addFiles(files);
    }
    
    // 在线搜索音乐
    void onSearchOnline()
    {
        OnlineMusicSearch* searchDialog = new OnlineMusicSearch(this);
        
        // 连接歌曲选择信号
        connect(searchDialog, &OnlineMusicSearch::songSelected, this, [this](const SongInfo& song) {
            // 添加在线歌曲到播放列表
            QUrl songUrl(song.url);
            
            if (!songUrl.isValid()) {
                QMessageBox::warning(this, "错误", "歌曲URL无效！");
                return;
            }
            
            // 添加到播放列表
            m_playlist.append(songUrl);
            
            // 显示歌曲信息
            QString displayName = QString("%1 - %2").arg(song.name).arg(song.artist);
            m_playListWidget->addItem(displayName);
            
            // 自动播放
            if (m_player->playbackState() != QMediaPlayer::PlayingState) {
                m_currentIndex = m_playlist.size() - 1;
                play();
            }
            
            QMessageBox::information(this, "成功", 
                QString("已添加：%1\n艺术家：%2\n\n提示：在线播放需要网络连接")
                .arg(song.name).arg(song.artist));
        });
        
        searchDialog->exec();
        delete searchDialog;
    }
    
    // 测试音频功能
    void testAudio()
    {
        QString info = "=== 音频系统诊断 ===\n\n";
        
        // 检查音频输出
        info += "【音频输出设备】\n";
        if (m_audioOutput) {
            info += QString("设备: %1\n").arg(m_audioOutput->device().description());
            info += QString("音量: %1%\n").arg(m_audioOutput->volume() * 100, 0, 'f', 0);
            info += QString("静音: %1\n\n").arg(m_audioOutput->isMuted() ? "是" : "否");
        } else {
            info += "错误：音频输出未初始化！\n\n";
        }
        
        // 检查播放器状态
        info += "【播放器状态】\n";
        info += QString("播放状态: ");
        switch (m_player->playbackState()) {
            case QMediaPlayer::StoppedState:
                info += "停止\n";
                break;
            case QMediaPlayer::PlayingState:
                info += "播放中\n";
                break;
            case QMediaPlayer::PausedState:
                info += "暂停\n";
                break;
        }
        
        info += QString("媒体状态: ");
        switch (m_player->mediaStatus()) {
            case QMediaPlayer::NoMedia:
                info += "无媒体\n";
                break;
            case QMediaPlayer::LoadingMedia:
                info += "加载中\n";
                break;
            case QMediaPlayer::LoadedMedia:
                info += "已加载\n";
                break;
            case QMediaPlayer::BufferingMedia:
                info += "缓冲中\n";
                break;
            case QMediaPlayer::BufferedMedia:
                info += "已缓冲\n";
                break;
            case QMediaPlayer::EndOfMedia:
                info += "播放结束\n";
                break;
            case QMediaPlayer::InvalidMedia:
                info += "无效媒体\n";
                break;
            default:
                info += "未知\n";
        }
        
        info += QString("当前源: %1\n").arg(m_player->source().toString());
        info += QString("时长: %1ms\n").arg(m_player->duration());
        info += QString("位置: %1ms\n\n").arg(m_player->position());
        
        // 检查播放列表
        info += "【播放列表】\n";
        info += QString("歌曲数量: %1\n").arg(m_playlist.size());
        info += QString("当前索引: %1\n\n").arg(m_currentIndex);
        
        // 检查错误
        if (m_player->error() != QMediaPlayer::NoError) {
            info += "【错误信息】\n";
            info += QString("错误代码: %1\n").arg(m_player->error());
            info += QString("错误描述: %1\n\n").arg(m_player->errorString());
        }
        
        // 建议
        info += "【建议】\n";
        if (m_audioOutput && m_audioOutput->volume() < 0.01) {
            info += "⚠️ 音量过低，请调高音量滑块\n";
        }
        if (m_audioOutput && m_audioOutput->isMuted()) {
            info += "⚠️ 音频已静音，请取消静音\n";
        }
        if (m_playlist.isEmpty()) {
            info += "⚠️ 播放列表为空，请添加音乐文件\n";
        }
        if (m_player->error() != QMediaPlayer::NoError) {
            info += "⚠️ 播放器出现错误，请检查文件格式\n";
        }
        
        QMessageBox::information(this, "音频系统诊断", info);
    }
    
    // 删除选中的歌曲
    void deleteSelectedSong()
    {
        int selectedRow = m_playListWidget->currentRow();
        
        if (selectedRow < 0) {
            QMessageBox::warning(this, "提示", "请先选择要删除的歌曲！");
            return;
        }
        
        // 确认删除
        QListWidgetItem* item = m_playListWidget->item(selectedRow);
        QString songName = item->text();
        
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, 
            "确认删除", 
            QString("确定要删除这首歌曲吗？\n\n%1").arg(songName),
            QMessageBox::Yes | QMessageBox::No
        );
        
        if (reply != QMessageBox::Yes) {
            return;
        }
        
        // 如果删除的是正在播放的歌曲
        bool wasPlaying = (selectedRow == m_currentIndex && 
                          m_player->playbackState() == QMediaPlayer::PlayingState);
        
        // 从播放列表中删除
        m_playlist.removeAt(selectedRow);
        delete m_playListWidget->takeItem(selectedRow);
        
        // 更新当前索引
        if (selectedRow < m_currentIndex) {
            // 删除的歌曲在当前播放歌曲之前，索引减1
            m_currentIndex--;
        } else if (selectedRow == m_currentIndex) {
            // 删除的是当前播放的歌曲
            m_player->stop();
            
            if (!m_playlist.isEmpty()) {
                // 如果还有歌曲，播放下一首
                if (m_currentIndex >= m_playlist.size()) {
                    m_currentIndex = 0;
                }
                
                if (wasPlaying) {
                    play();
                }
            } else {
                // 播放列表为空
                m_currentIndex = -1;
                m_lyricWidget->clear();
            }
        }
        
        qDebug() << "已删除歌曲，当前索引:" << m_currentIndex << "播放列表大小:" << m_playlist.size();
    }
    
    // 显示播放列表右键菜单
    void showPlaylistContextMenu(const QPoint& pos)
    {
        QListWidgetItem* item = m_playListWidget->itemAt(pos);
        if (!item) {
            return;
        }
        
        QMenu contextMenu(this);
        contextMenu.setStyleSheet(
            "QMenu { "
            "   background-color: #2b2b2b; "
            "   color: white; "
            "   border: 1px solid #444; "
            "}"
            "QMenu::item { "
            "   padding: 8px 25px; "
            "}"
            "QMenu::item:selected { "
            "   background-color: #0d47a1; "
            "}"
        );
        
        QAction* playAction = contextMenu.addAction("▶️ 播放");
        QAction* deleteAction = contextMenu.addAction("🗑️ 删除");
        contextMenu.addSeparator();
        QAction* clearAllAction = contextMenu.addAction("🗑️ 清空播放列表");
        
        QAction* selectedAction = contextMenu.exec(m_playListWidget->mapToGlobal(pos));
        
        if (selectedAction == playAction) {
            int row = m_playListWidget->row(item);
            m_currentIndex = row;
            play();
        } else if (selectedAction == deleteAction) {
            m_playListWidget->setCurrentItem(item);
            deleteSelectedSong();
        } else if (selectedAction == clearAllAction) {
            clearPlaylist();
        }
    }
    
    // 清空播放列表
    void clearPlaylist()
    {
        if (m_playlist.isEmpty()) {
            QMessageBox::information(this, "提示", "播放列表已经是空的！");
            return;
        }
        
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, 
            "确认清空", 
            QString("确定要清空整个播放列表吗？\n\n共 %1 首歌曲").arg(m_playlist.size()),
            QMessageBox::Yes | QMessageBox::No
        );
        
        if (reply != QMessageBox::Yes) {
            return;
        }
        
        // 停止播放
        m_player->stop();
        
        // 清空列表
        m_playlist.clear();
        m_playListWidget->clear();
        m_currentIndex = -1;
        m_lyricWidget->clear();
        
        qDebug() << "播放列表已清空";
    }

    // 切换播放/暂停
    void togglePlay()
    {
        if(m_player->playbackState() == QMediaPlayer::PlayingState)
        {
            pause();
        }
        else
        {
            play();
        }
    }

    // 播放
    void play()
    {
        if (m_playlist.isEmpty() || m_currentIndex < 0 || m_currentIndex >= m_playlist.size())
            return;
        
        // 只有当源不同时才重新设置源
        if (m_player->source() != m_playlist[m_currentIndex]) {
            m_player->setSource(m_playlist[m_currentIndex]);
            // 加载歌词
            loadLyrics();
        }
        
        // 确保音频输出已设置且音量正确
        if (m_player->audioOutput() == nullptr) {
            m_player->setAudioOutput(m_audioOutput);
            qDebug() << "重新设置音频输出";
        }
        
        // 确保音量不为0
        qreal currentVolume = m_audioOutput->volume();
        qDebug() << "当前音量:" << currentVolume;
        if (currentVolume < 0.01) {
            m_audioOutput->setVolume(0.8);
            m_volumeSlider->setValue(80);
            qDebug() << "音量过低，已重置为80%";
        }
        
        qDebug() << "开始播放:" << m_playlist[m_currentIndex].toString();
        qDebug() << "播放器状态:" << m_player->playbackState();
        qDebug() << "媒体状态:" << m_player->mediaStatus();
        
        m_player->play();
        m_spectrumWidget->setPlaying(true);
        m_playListWidget->setCurrentRow(m_currentIndex);
        
        m_btnPlayPause->setIcon(QIcon("./assets/pause.png"));
        m_btnPlayPause->setIconSize(QSize(48, 48));
    }
    
    // 加载歌词
    void loadLyrics()
    {
        if (m_currentIndex < 0 || m_currentIndex >= m_playlist.size()) {
            m_lyricWidget->clear();
            return;
        }
        
        QString audioPath = m_playlist[m_currentIndex].toLocalFile();
        
        // 首先尝试从本地加载歌词
        QList<LyricLine> lyrics = LyricParser::autoLoadLyrics(audioPath);
        
        if (lyrics.isEmpty()) {
            qDebug() << "本地未找到歌词文件，尝试在线下载...";
            
            // 显示下载提示
            m_lyricWidget->clear();
            
            // 在后台下载歌词
            QTimer::singleShot(100, this, [this, audioPath]() {
                bool success = m_lyricDownloader->autoDownloadLyric(audioPath);
                
                if (success) {
                    qDebug() << "歌词下载成功，重新加载";
                    // 重新加载歌词
                    QList<LyricLine> lyrics = LyricParser::autoLoadLyrics(audioPath);
                    m_lyricWidget->setLyrics(lyrics);
                    
                    // 可选：显示成功提示
                    QMessageBox::information(this, "提示", "歌词下载成功！");
                } else {
                    qDebug() << "歌词下载失败:" << m_lyricDownloader->lastError();
                    // 可选：显示失败提示
                    // QMessageBox::warning(this, "提示", "未找到歌词：" + m_lyricDownloader->lastError());
                }
            });
        } else {
            m_lyricWidget->setLyrics(lyrics);
        }
    }

    // 暂停
    void pause()
    {
        m_player->pause();
        m_spectrumWidget->setPlaying(false);
        m_btnPlayPause->setIcon(QIcon("./assets/play.png"));
        m_btnPlayPause->setIconSize(QSize(48, 48));
    }

    // 上一首
    void prev()
    {
        if (m_playlist.isEmpty()) return;
        
        if (m_currentIndex > 0) {
            m_currentIndex--;
        } else {
            m_currentIndex = m_playlist.size() - 1;
        }
        
        play();
    }

    // 下一首
    void next()
    {
        if (m_playlist.isEmpty()) return;
        
        if (m_playMode == Random) {
            m_currentIndex = QRandomGenerator::global()->bounded(m_playlist.size());
        } else {
            if (m_currentIndex < m_playlist.size() - 1) {
                m_currentIndex++;
            } else {
                m_currentIndex = 0;
            }
        }
        
        play();
    }

    // 设置播放模式
    void setPlayMode(PlayMode mode)
    {
        m_playMode = mode;
        updatePlayModeUI();
    }

    // 更新播放按钮状态
    void updatePlayButton(QMediaPlayer::PlaybackState state)
    {
        if (state == QMediaPlayer::PlayingState)
        {
            m_btnPlayPause->setIcon(QIcon("./assets/pause.png"));
            m_btnPlayPause->setToolTip("暂停");
        }
        else
        {
            m_btnPlayPause->setIcon(QIcon("./assets/play.png"));
            m_btnPlayPause->setToolTip("播放");
        }
    }

    // 更新播放位置
    void updatePosition(qint64 position)
    {
        m_currentTime->setText(formatTime(position));

        if (!m_progressSlider->isSliderDown())
        {
            m_progressSlider->blockSignals(true);
            m_progressSlider->setValue(position);
            m_progressSlider->blockSignals(false);
        }
    }

    // 更新总时长
    void updateDuration(qint64 duration)
    {
        m_totalTime->setText(formatTime(duration));
        m_progressSlider->setRange(0, duration);
    }

    // 跳转到指定位置
    void seek(int position)
    {
        m_player->setPosition(position);
    }
    
    // 音量改变
    void onVolumeChanged(int value)
    {
        qreal volume = value / 100.0;  // 转换为 0.0 到 1.0
        m_audioOutput->setVolume(volume);
        m_volumeLabel->setText(QString("%1%").arg(value));
        
        // 更新音量图标
        QLabel* volumeIcon = this->findChild<QLabel*>("volumeIcon");
        if (volumeIcon) {
            if (value == 0) {
                volumeIcon->setText("🔇");
            } else if (value < 30) {
                volumeIcon->setText("🔈");
            } else if (value < 70) {
                volumeIcon->setText("🔉");
            } else {
                volumeIcon->setText("🔊");
            }
        }
    }

    // 更新播放模式UI
    void updatePlayModeUI()
    {
        m_btnLoopList->setChecked(m_playMode == ListLoop);
        m_btnLoopSingle->setChecked(m_playMode == SingleLoop);
        m_btnRandom->setChecked(m_playMode == Random);
    }
    
    // 媒体状态变化（Qt6）
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status)
    {
        qDebug() << "媒体状态变化:" << status;
        
        if (status == QMediaPlayer::EndOfMedia) {
            // 根据播放模式决定下一步
            if (m_playMode == SingleLoop) {
                // 单曲循环：重置到开头并继续播放
                m_player->setPosition(0);
                m_player->play();
            } else {
                next(); // 播放下一首
            }
        } else if (status == QMediaPlayer::InvalidMedia) {
            QMessageBox::warning(this, "错误", "无效的媒体文件！\n请检查文件格式是否支持。");
        } else if (status == QMediaPlayer::LoadedMedia) {
            qDebug() << "媒体加载成功，时长:" << m_player->duration() << "ms";
        }
    }
    
    // 播放器错误处理
    void onPlayerError(QMediaPlayer::Error error, const QString &errorString)
    {
        qDebug() << "播放器错误:" << error << errorString;
        
        QString errorMsg;
        switch (error) {
            case QMediaPlayer::NoError:
                return;
            case QMediaPlayer::ResourceError:
                errorMsg = "资源错误：无法打开媒体文件\n" + errorString;
                break;
            case QMediaPlayer::FormatError:
                errorMsg = "格式错误：不支持的媒体格式\n" + errorString;
                break;
            case QMediaPlayer::NetworkError:
                errorMsg = "网络错误：无法访问网络资源\n" + errorString;
                break;
            case QMediaPlayer::AccessDeniedError:
                errorMsg = "访问被拒绝：没有权限访问该文件\n" + errorString;
                break;
            default:
                errorMsg = "未知错误：" + errorString;
                break;
        }
        
        QMessageBox::critical(this, "播放错误", errorMsg);
    }
};

#endif // AUDIOPLAYER_H
