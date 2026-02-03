#ifndef SPECTRUMWIDGET_H
#define SPECTRUMWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <QRandomGenerator>
#include <QLinearGradient>
#include <QtMath>
#include <QMediaPlayer>
#include <QAudioSink>

// 音频频谱可视化组件（Qt6 版本）
class SpectrumWidget : public QWidget
{
    Q_OBJECT

private:
    static const int BAR_COUNT = 64;        // 频谱条数量
    QVector<double> m_barHeights;           // 每个频谱条的高度
    QVector<double> m_targetHeights;        // 目标高度（用于平滑动画）
    QVector<double> m_peakHeights;          // 峰值高度
    QVector<int> m_peakHoldTime;            // 峰值保持时间
    
    QTimer *m_updateTimer;                  // 更新定时器
    QMediaPlayer *m_player;                 // 关联的播放器
    
    bool m_isPlaying;                       // 是否正在播放
    int m_colorOffset;                      // 颜色偏移（用于渐变动画）
    
    // 样式配置
    int m_barWidth;                         // 频谱条宽度
    int m_barSpacing;                       // 频谱条间距
    QColor m_backgroundColor;               // 背景色
    
public:
    explicit SpectrumWidget(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_isPlaying(false)
        , m_colorOffset(0)
        , m_barWidth(8)
        , m_barSpacing(2)
        , m_backgroundColor(QColor(20, 20, 30))
        , m_player(nullptr)
    {
        // 初始化频谱数据，设置初始高度避免完全为0
        m_barHeights.resize(BAR_COUNT);
        m_targetHeights.resize(BAR_COUNT);
        m_peakHeights.resize(BAR_COUNT);
        m_peakHoldTime.resize(BAR_COUNT);
        
        for (int i = 0; i < BAR_COUNT; ++i) {
            m_barHeights[i] = 0.12;
            m_targetHeights[i] = 0.12;
            m_peakHeights[i] = 0.12;
            m_peakHoldTime[i] = 0;
        }
        
        // 设置定时器
        m_updateTimer = new QTimer(this);
        connect(m_updateTimer, &QTimer::timeout, this, &SpectrumWidget::updateSpectrum);
        m_updateTimer->start(50); // 20fps
        
        // 设置最小尺寸
        setMinimumHeight(150);
        
        // 设置背景
        setAutoFillBackground(true);
        QPalette pal = palette();
        pal.setColor(QPalette::Window, m_backgroundColor);
        setPalette(pal);
    }
    
    ~SpectrumWidget()
    {
    }
    
    // 关联媒体播放器
    void setMediaPlayer(QMediaPlayer *player)
    {
        m_player = player;
        
        // Qt6 中音频探针已被移除，这里使用简化的模拟频谱
        // 实际项目中可以使用 QAudioSink 和自定义音频处理
        if (m_player) {
            connect(m_player, &QMediaPlayer::playbackStateChanged, this, 
                    [this](QMediaPlayer::PlaybackState state) {
                m_isPlaying = (state == QMediaPlayer::PlayingState);
                if (!m_isPlaying) {
                    resetSpectrum();
                }
            });
        }
    }
    
    // 设置播放状态
    void setPlaying(bool playing)
    {
        m_isPlaying = playing;
        if (!playing) {
            resetSpectrum();
        }
    }
    
protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        
        // 绘制背景
        painter.fillRect(rect(), m_backgroundColor);
        
        // 计算频谱条的总宽度
        int totalWidth = BAR_COUNT * (m_barWidth + m_barSpacing) - m_barSpacing;
        int startX = (width() - totalWidth) / 2;
        
        // 绘制频谱条
        for (int i = 0; i < BAR_COUNT; ++i) {
            int x = startX + i * (m_barWidth + m_barSpacing);
            
            // 强制确保最小高度
            double displayHeight = qMax(0.12, m_barHeights[i]);
            int barHeight = static_cast<int>(displayHeight * (height() - 20));
            barHeight = qMax(8, barHeight);
            int y = height() - barHeight - 10;
            
            // 创建渐变色
            double hue = fmod((i * 360.0 / BAR_COUNT + m_colorOffset), 360.0) / 360.0;
            QColor color1 = QColor::fromHsvF(hue, 0.95, 1.0);
            QColor color2 = QColor::fromHsvF(hue, 0.75, 0.8);
            
            QLinearGradient gradient(x, y + barHeight, x, y);
            gradient.setColorAt(0, color1);
            gradient.setColorAt(1, color2);
            
            // 绘制频谱条
            painter.setBrush(gradient);
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(x, y, m_barWidth, barHeight, 2, 2);
            
            // 绘制峰值指示器
            double peakDisplayHeight = qMax(0.12, m_peakHeights[i]);
            if (peakDisplayHeight > 0.12) {
                int peakY = height() - static_cast<int>(peakDisplayHeight * (height() - 20)) - 10;
                QColor peakColor = QColor::fromHsvF(hue, 1.0, 1.0);
                painter.setBrush(peakColor);
                painter.drawRoundedRect(x, peakY - 2, m_barWidth, 3, 1, 1);
            }
        }
        
        // 绘制标题
        if (!m_isPlaying) {
            painter.setPen(QColor(150, 150, 170, 180));
            painter.setFont(QFont("Microsoft YaHei", 11));
            painter.drawText(rect().adjusted(0, -30, 0, 0), Qt::AlignCenter, "♪ 音频频谱可视化 ♪");
        }
    }
    
private slots:
    // 更新频谱显示
    void updateSpectrum()
    {
        m_colorOffset = (m_colorOffset + 3) % 360;
        
        if (m_isPlaying) {
            // 模拟音频频谱（实际项目中应该从音频数据计算）
            for (int i = 0; i < BAR_COUNT; ++i) {
                // 生成随机目标高度（模拟音频波动）
                double randomValue = QRandomGenerator::global()->bounded(100) / 100.0;
                double baseHeight = 0.2 + randomValue * 0.6;
                
                // 添加一些频率相关的变化
                double freqFactor = 1.0 - (i / (double)BAR_COUNT) * 0.5;
                m_targetHeights[i] = baseHeight * freqFactor;
                
                // 平滑过渡
                double diff = m_targetHeights[i] - m_barHeights[i];
                m_barHeights[i] += diff * 0.3;
                
                if (m_barHeights[i] < 0.12) {
                    m_barHeights[i] = 0.12;
                }
                
                // 更新峰值
                if (m_barHeights[i] > m_peakHeights[i]) {
                    m_peakHeights[i] = m_barHeights[i];
                    m_peakHoldTime[i] = 20;
                } else {
                    if (m_peakHoldTime[i] > 0) {
                        m_peakHoldTime[i]--;
                    } else {
                        m_peakHeights[i] *= 0.95;
                        if (m_peakHeights[i] < 0.12) {
                            m_peakHeights[i] = 0.12;
                        }
                    }
                }
            }
        } else {
            // 呼吸灯效果
            static int breathCounter = 0;
            breathCounter++;
            double breathValue = 0.15 + 0.08 * qSin(breathCounter * 0.05);
            
            for (int i = 0; i < BAR_COUNT; ++i) {
                m_barHeights[i] *= 0.92;
                if (m_barHeights[i] < breathValue) {
                    m_barHeights[i] = breathValue;
                }
                
                m_peakHeights[i] *= 0.92;
                if (m_peakHeights[i] < breathValue) {
                    m_peakHeights[i] = breathValue;
                }
            }
        }
        
        update();
    }
    
    // 重置频谱
    void resetSpectrum()
    {
        for (int i = 0; i < BAR_COUNT; ++i) {
            m_targetHeights[i] = 0.0;
        }
    }
};

#endif // SPECTRUMWIDGET_H
