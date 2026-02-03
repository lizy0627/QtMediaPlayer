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
#include "spectrumwidget.h"
#include "lyricwidget.h"
#include "lyricparser.h"
#include "lyricdownloader.h"

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
        playlistLayout->addWidget(m_playListWidget);

        // 添加文件按钮
        QPushButton *addButton = new QPushButton("添加音乐", playlistGroup);
        connect(addButton, &QPushButton::clicked, this, &AudioPlayer::onAddFiles);
        playlistLayout->addWidget(addButton);

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

        // 播放器信号（Qt6）
        connect(m_player, &QMediaPlayer::positionChanged, this, &AudioPlayer::updatePosition);
        connect(m_player, &QMediaPlayer::durationChanged, this, &AudioPlayer::updateDuration);
        connect(m_player, &QMediaPlayer::playbackStateChanged, this, &AudioPlayer::updatePlayButton);
        connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &AudioPlayer::onMediaStatusChanged);

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
            
        m_player->setSource(m_playlist[m_currentIndex]);
        m_player->play();
        m_spectrumWidget->setPlaying(true);
        m_playListWidget->setCurrentRow(m_currentIndex);
        
        // 加载歌词
        loadLyrics();
        
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
        if (status == QMediaPlayer::EndOfMedia) {
            // 根据播放模式决定下一步
            if (m_playMode == SingleLoop) {
                play(); // 重新播放当前曲目
            } else {
                next(); // 播放下一首
            }
        }
    }
};

#endif // AUDIOPLAYER_H
