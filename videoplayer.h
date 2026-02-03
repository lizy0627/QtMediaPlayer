#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QObject>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QAudioOutput>
#include <QDebug>
#include <QPushButton>
#include <QTimer>
#include <QSlider>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QComboBox>
#include <QWidget>
#include <QUrl>
#include <QIcon>

// Qt6 视频播放器
class VideoPlayer : public QObject
{
    Q_OBJECT
private:
    QWidget* m_parent = nullptr;
    QWidget* m_controlPanel = nullptr;  // 控制面板
    QMediaPlayer *m_player = nullptr;   // 播放器对象
    QVideoWidget *m_video = nullptr;    // 播放器控件
    QAudioOutput *m_audioOutput = nullptr; // Qt6 音频输出

    QSlider *m_slider = nullptr;        // 进度滑块
    QLabel *m_timeLabel = nullptr;      // 时间标签
    QTimer *m_updateTimer = nullptr;    // 更新定时器

    double m_playbackRate = 1.0;        // 当前播放倍速

    QVBoxLayout *m_mainLayout = nullptr;    // 主布局

    QComboBox* m_cbRate = nullptr;          // 倍速播放
    QSlider *m_volSlider = nullptr;         // 音量进度条

    QPushButton* m_btnCtr = nullptr;        // 原有的小控制按钮
    QPushButton* m_btnPlayPause = nullptr;  // 新增的大播放/暂停按钮

public:
    explicit VideoPlayer(QWidget* parent)
        : QObject(parent), m_parent(parent)
    {
        // 创建主布局
        m_mainLayout = new QVBoxLayout(parent);
        m_mainLayout->setContentsMargins(0, 0, 0, 0);
        m_mainLayout->setSpacing(0);

        // 创建播放器控件
        m_video = new QVideoWidget(parent);
        m_mainLayout->addWidget(m_video);
        m_video->setStyleSheet("background-color: rgba(0, 0, 0, 150);");

        // 创建播放器对象（Qt6）
        m_player = new QMediaPlayer(parent);
        m_audioOutput = new QAudioOutput(parent);
        m_player->setAudioOutput(m_audioOutput);
        m_player->setVideoOutput(m_video);
        m_audioOutput->setVolume(0.5); // Qt6 音量范围 0.0-1.0

        // 创建控制面板
        m_controlPanel = new QWidget(parent);
        m_controlPanel->setObjectName("controlPannel");
        m_controlPanel->setStyleSheet("#controlPannel { background-color: rgba(0, 0, 0, 150); }");
        m_controlPanel->setMaximumHeight(75);
        m_mainLayout->addWidget(m_controlPanel);

        parent->setLayout(m_mainLayout);

        // 创建进度条组件
        createProgressControls();

        // 连接信号和槽
        connectSignals();
    }

    ~VideoPlayer()
    {
        if(m_updateTimer) {
            m_updateTimer->stop();
            delete m_updateTimer;
        }
    }

private:
    // 创建进度控制组件
    void createProgressControls()
    {
        // 1.原有的小暂停/开始按钮
        m_btnCtr = new QPushButton(m_parent);
        m_btnCtr->setFixedSize(31, 31);
        m_btnCtr->setStyleSheet("border:none;");
        m_btnCtr->setIconSize(QSize(26,26));
        m_btnCtr->setIcon(QIcon("./assets/pause.png"));
        connect(m_btnCtr, &QPushButton::clicked, this, [=]() {
            if(m_player->playbackState() == QMediaPlayer::PlayingState) {
                m_player->pause();
                m_btnCtr->setIcon(QIcon("./assets/play.png"));
                m_btnPlayPause->setIcon(QIcon("./assets/play.png"));
            } else {
                m_player->play();
                m_btnCtr->setIcon(QIcon("./assets/pause.png"));
                m_btnPlayPause->setIcon(QIcon("./assets/pause.png"));
            }
        });

        // 2.新增的大播放/暂停按钮
        QVBoxLayout *playPauseLayout = new QVBoxLayout();
        playPauseLayout->setAlignment(Qt::AlignCenter);
        playPauseLayout->setSpacing(3);
        
        m_btnPlayPause = new QPushButton(m_controlPanel);
        m_btnPlayPause->setIcon(QIcon("./assets/play.png"));
        m_btnPlayPause->setIconSize(QSize(32, 32));
        m_btnPlayPause->setFixedSize(50, 50);
        m_btnPlayPause->setToolTip("播放/暂停 (Space)");
        m_btnPlayPause->setStyleSheet(
            "QPushButton { "
            "   background-color: #0d47a1; "
            "   border: 2px solid #1565c0; "
            "   border-radius: 25px; "
            "}"
            "QPushButton:hover { "
            "   background-color: #1565c0; "
            "   border: 2px solid #1976d2; "
            "}"
            "QPushButton:pressed { "
            "   background-color: #0a3d91; "
            "}"
        );
        connect(m_btnPlayPause, &QPushButton::clicked, this, [=]() {
            if(m_player->playbackState() == QMediaPlayer::PlayingState) {
                m_player->pause();
                m_btnCtr->setIcon(QIcon("./assets/play.png"));
                m_btnPlayPause->setIcon(QIcon("./assets/play.png"));
            } else {
                m_player->play();
                m_btnCtr->setIcon(QIcon("./assets/pause.png"));
                m_btnPlayPause->setIcon(QIcon("./assets/pause.png"));
            }
        });
        
        QLabel *playPauseLabel = new QLabel("播放/暂停", m_controlPanel);
        playPauseLabel->setAlignment(Qt::AlignCenter);
        playPauseLabel->setStyleSheet("color: #64b5f6; font-weight: bold; font-size: 9pt;");
        
        playPauseLayout->addWidget(m_btnPlayPause);
        playPauseLayout->addWidget(playPauseLabel);

        // 3.音量
        m_volSlider = new QSlider(Qt::Orientation::Horizontal, m_controlPanel);
        m_volSlider->setRange(0, 100);
        m_volSlider->setValue(50);
        m_volSlider->setStyleSheet(
            "QSlider::groove:horizontal {"
            "    height: 6px;"
            "    background: rgb(217, 217, 217);"
            "}"
            "QSlider::sub-page:horizontal {"
            "    background: #1E90FF;"
            "}"
            "QSlider::handle:horizontal {"
            "    width: 14px;"
            "    margin: -4px 0;"
            "    background: #FFF;"
            "    border-radius: 7px;"
            "}"
        );
        connect(m_volSlider, &QSlider::valueChanged, this, [=](int value){ 
            setVolume(value); 
        });

        // 4.倍速选择框
        m_cbRate = new QComboBox(m_controlPanel);
        m_cbRate->addItems({"0.5", "0.75", "1.0", "1.25", "1.5", "2.0"});
        m_cbRate->setCurrentText("1.0");
        connect(m_cbRate, &QComboBox::currentTextChanged, this, [=](const QString& text){ 
            setSpeed(text.toFloat()); 
        });
        m_cbRate->setStyleSheet(
            "QComboBox {"
            "    background-color: #2D2D2D;"
            "    color: white;"
            "    border: 1px solid #555;"
            "    border-radius: 3px;"
            "    padding: 5px;"
            "}"
            "QComboBox::drop-down {"
            "    subcontrol-origin: padding;"
            "    subcontrol-position: top right;"
            "    border-left-width: 1px;"
            "    border-left-color: #555;"
            "    border-left-style: solid;"
            "    border-top-right-radius: 3px;"
            "    border-bottom-right-radius: 3px;"
            "}"
            "QComboBox QAbstractItemView {"
            "    background-color: white;"
            "    color: black;"
            "    border: 1px solid #555;"
            "    border-radius: 3px;"
            "    selection-background-color: #2D2D2D;"
            "    selection-color: white;"
            "}"
            "QComboBox:hover {"
            "    background-color: #3D3D3D;"
            "}"
            "QComboBox:pressed {"
            "    background-color: #1D1D1D;"
            "}"
        );

        // 5.进度滑块
        m_slider = new QSlider(Qt::Horizontal, m_controlPanel);
        m_slider->setRange(0, 100);
        m_slider->setFixedHeight(20);
        m_slider->setStyleSheet(
            "QSlider::groove:horizontal {"
            "    height: 6px;"
            "    background: #555;"
            "}"
            "QSlider::sub-page:horizontal {"
            "    background: #1E90FF;"
            "}"
            "QSlider::handle:horizontal {"
            "    width: 14px;"
            "    margin: -4px 0;"
            "    background: #FFF;"
            "    border-radius: 7px;"
            "}"
        );

        // 6.时间标签
        m_timeLabel = new QLabel("00:00 / 00:00", m_controlPanel);
        m_timeLabel->setStyleSheet("color: white; font: 10pt;");
        m_timeLabel->setFixedWidth(120);
        m_timeLabel->setAlignment(Qt::AlignCenter);

        // 7.布局
        QHBoxLayout *layout = new QHBoxLayout(m_controlPanel);
        layout->addWidget(m_btnCtr);
        layout->addLayout(playPauseLayout);
        layout->addWidget(m_volSlider);
        layout->addWidget(m_cbRate);
        layout->addWidget(m_slider, 9);
        layout->addWidget(m_timeLabel, 1);
        layout->setContentsMargins(10, 5, 10, 5);
        m_controlPanel->setLayout(layout);

        // 更新定时器
        m_updateTimer = new QTimer(this);
        m_updateTimer->setInterval(500);
    }

    // 连接信号和槽
    void connectSignals()
    {
        // Qt6 媒体状态变化
        connect(m_player, &QMediaPlayer::mediaStatusChanged, [=](QMediaPlayer::MediaStatus status) {
            if (status == QMediaPlayer::EndOfMedia) {
                qDebug() << "播放结束";
                m_updateTimer->stop();
            } else if (status == QMediaPlayer::BufferedMedia) {
                qDebug() << "缓冲完成";
            } else if (status == QMediaPlayer::LoadedMedia) {
                qDebug() << "媒体已加载";
            }
        });

        // Qt6 播放状态变化
        connect(m_player, &QMediaPlayer::playbackStateChanged, [=](QMediaPlayer::PlaybackState state) {
            if (state == QMediaPlayer::PlayingState) {
                m_updateTimer->start();
            } else {
                m_updateTimer->stop();
            }
        });

        // 定时器超时更新进度
        connect(m_updateTimer, &QTimer::timeout, this, &VideoPlayer::updateProgress);

        // 滑块位置变化
        connect(m_slider, &QSlider::sliderMoved, this, &VideoPlayer::seekToPosition);

        // 媒体时长变化
        connect(m_player, &QMediaPlayer::durationChanged, this, [=](qint64 duration) {
            if (duration > 0) {
                m_slider->setRange(0, duration);
            }
        });
    }

private slots:
    // 更新播放进度
    void updateProgress()
    {
        if (m_player->duration() > 0) {
            m_slider->blockSignals(true);
            m_slider->setValue(m_player->position());
            m_slider->blockSignals(false);
            updateTimeLabel();
        }
    }

    // 跳转到指定位置
    void seekToPosition(int position)
    {
        if (!m_slider->isSliderDown()) return;
        m_player->setPosition(position);
        updateTimeLabel();
    }

    // 更新时间标签
    void updateTimeLabel()
    {
        qint64 current = m_player->position() / 1000;
        qint64 total = m_player->duration() / 1000;

        QString currentTime = QString("%1:%2")
            .arg(current / 60, 2, 10, QLatin1Char('0'))
            .arg(current % 60, 2, 10, QLatin1Char('0'));

        QString totalTime = QString("%1:%2")
            .arg(total / 60, 2, 10, QLatin1Char('0'))
            .arg(total % 60, 2, 10, QLatin1Char('0'));

        m_timeLabel->setText(currentTime + " / " + totalTime);
    }

public:
    // 打开视频文件
    void open(const QString &filepath, bool localFile = true)
    {
        if(localFile) {
            m_player->setSource(QUrl::fromLocalFile(filepath));
        } else {
            m_player->setSource(QUrl(filepath));
        }
        m_player->play();
    }

    // 播放/暂停切换
    void toggle()
    {
        if(m_player->playbackState() == QMediaPlayer::PlayingState) {
            m_player->pause();
        } else {
            m_player->play();
        }
    }

    // 跳转
    void jump(bool forward, int ms = 5000)
    {
        qint64 position = m_player->position();
        m_player->setPosition(forward ? position + ms : position - ms);
    }

    // 设置音量（Qt6 范围 0-100 转换为 0.0-1.0）
    void setVolume(int volume)
    {
        if (m_audioOutput) {
            m_audioOutput->setVolume(volume / 100.0);
        }
    }

    // 获取音量
    int volume() const
    {
        if (m_audioOutput) {
            return static_cast<int>(m_audioOutput->volume() * 100);
        }
        return 50;
    }

    // 是否正在播放
    bool isPlaying() const
    {
        return m_player->playbackState() == QMediaPlayer::PlayingState;
    }

    // 设置倍速播放
    void setSpeed(double speed = 1.0)
    {
        speed = qBound(0.25, speed, 4.0);
        m_playbackRate = speed;
        m_player->setPlaybackRate(speed);

        if (m_updateTimer) {
            int interval = static_cast<int>(500 / speed);
            m_updateTimer->setInterval(qMax(100, interval));
        }

        qDebug() << "设置播放倍速:" << speed;
    }

    // 获取当前倍速
    double speed() const
    {
        return m_playbackRate;
    }

    // 显示/隐藏控制面板
    void setControlsVisible(bool visible)
    {
        if (m_controlPanel) {
            m_controlPanel->setVisible(visible);
        }
    }

    void pause()
    {
        m_player->pause();
        m_btnCtr->setIcon(QIcon("./assets/play.png"));
        m_btnPlayPause->setIcon(QIcon("./assets/play.png"));
    }
    
    void play()
    {
        m_player->play();
        m_btnCtr->setIcon(QIcon("./assets/pause.png"));
        m_btnPlayPause->setIcon(QIcon("./assets/pause.png"));
    }
};

#endif // VIDEOPLAYER_H
