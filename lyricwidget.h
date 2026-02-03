#ifndef LYRICWIDGET_H
#define LYRICWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QPainter>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QTimer>
#include <QDebug>

// 单行歌词结构
struct LyricLine
{
    qint64 timestamp;  // 时间戳（毫秒）
    QString text;      // 歌词文本
    
    LyricLine(qint64 time = 0, const QString& lyric = "")
        : timestamp(time), text(lyric) {}
};

// 歌词显示组件
class LyricWidget : public QWidget
{
    Q_OBJECT
    
private:
    QList<LyricLine> m_lyrics;           // 歌词列表
    int m_currentLineIndex;              // 当前歌词行索引
    qint64 m_currentPosition;            // 当前播放位置（毫秒）
    
    // UI组件
    QLabel* m_prevLine;                  // 上一行歌词
    QLabel* m_currentLine;               // 当前行歌词
    QLabel* m_nextLine;                  // 下一行歌词
    QLabel* m_noLyricLabel;              // 无歌词提示
    
    QPropertyAnimation* m_fadeAnimation; // 淡入淡出动画
    
public:
    explicit LyricWidget(QWidget* parent = nullptr)
        : QWidget(parent)
        , m_currentLineIndex(-1)
        , m_currentPosition(0)
    {
        setupUI();
    }
    
    // 设置歌词列表
    void setLyrics(const QList<LyricLine>& lyrics)
    {
        m_lyrics = lyrics;
        m_currentLineIndex = -1;
        m_currentPosition = 0;
        
        if (m_lyrics.isEmpty()) {
            showNoLyric();
        } else {
            hideNoLyric();
            updateDisplay();
        }
    }
    
    // 清空歌词
    void clear()
    {
        m_lyrics.clear();
        m_currentLineIndex = -1;
        m_currentPosition = 0;
        showNoLyric();
    }
    
    // 更新播放位置
    void updatePosition(qint64 position)
    {
        m_currentPosition = position;
        
        if (m_lyrics.isEmpty()) return;
        
        // 查找当前应该显示的歌词行
        int newIndex = findCurrentLine(position);
        
        if (newIndex != m_currentLineIndex && newIndex >= 0) {
            m_currentLineIndex = newIndex;
            updateDisplay();
            animateCurrentLine();
        }
    }
    
private:
    void setupUI()
    {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        mainLayout->setSpacing(15);
        
        // 创建歌词显示区域
        QWidget* lyricContainer = new QWidget(this);
        lyricContainer->setStyleSheet(
            "QWidget { "
            "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
            "       stop:0 rgba(13, 71, 161, 0.1), "
            "       stop:1 rgba(21, 101, 192, 0.05)); "
            "   border-radius: 15px; "
            "   border: 2px solid rgba(100, 181, 246, 0.3); "
            "}"
        );
        
        QVBoxLayout* lyricLayout = new QVBoxLayout(lyricContainer);
        lyricLayout->setContentsMargins(30, 40, 30, 40);
        lyricLayout->setSpacing(20);
        lyricLayout->setAlignment(Qt::AlignCenter);
        
        // 上一行歌词
        m_prevLine = new QLabel(lyricContainer);
        m_prevLine->setAlignment(Qt::AlignCenter);
        m_prevLine->setWordWrap(true);
        m_prevLine->setStyleSheet(
            "QLabel { "
            "   color: rgba(255, 255, 255, 0.4); "
            "   font-size: 14pt; "
            "   font-weight: normal; "
            "   padding: 5px; "
            "}"
        );
        lyricLayout->addWidget(m_prevLine);
        
        // 当前行歌词（高亮）
        m_currentLine = new QLabel(lyricContainer);
        m_currentLine->setAlignment(Qt::AlignCenter);
        m_currentLine->setWordWrap(true);
        m_currentLine->setStyleSheet(
            "QLabel { "
            "   color: #64b5f6; "
            "   font-size: 20pt; "
            "   font-weight: bold; "
            "   padding: 10px; "
            "   background: rgba(100, 181, 246, 0.1); "
            "   border-radius: 10px; "
            "}"
        );
        
        // 添加透明度效果用于动画
        QGraphicsOpacityEffect* opacityEffect = new QGraphicsOpacityEffect(m_currentLine);
        m_currentLine->setGraphicsEffect(opacityEffect);
        
        m_fadeAnimation = new QPropertyAnimation(opacityEffect, "opacity", this);
        m_fadeAnimation->setDuration(300);
        m_fadeAnimation->setStartValue(0.3);
        m_fadeAnimation->setEndValue(1.0);
        
        lyricLayout->addWidget(m_currentLine);
        
        // 下一行歌词
        m_nextLine = new QLabel(lyricContainer);
        m_nextLine->setAlignment(Qt::AlignCenter);
        m_nextLine->setWordWrap(true);
        m_nextLine->setStyleSheet(
            "QLabel { "
            "   color: rgba(255, 255, 255, 0.4); "
            "   font-size: 14pt; "
            "   font-weight: normal; "
            "   padding: 5px; "
            "}"
        );
        lyricLayout->addWidget(m_nextLine);
        
        mainLayout->addWidget(lyricContainer);
        
        // 无歌词提示
        m_noLyricLabel = new QLabel("🎵 暂无歌词", this);
        m_noLyricLabel->setAlignment(Qt::AlignCenter);
        m_noLyricLabel->setStyleSheet(
            "QLabel { "
            "   color: rgba(255, 255, 255, 0.5); "
            "   font-size: 18pt; "
            "   font-weight: bold; "
            "   padding: 50px; "
            "}"
        );
        mainLayout->addWidget(m_noLyricLabel);
        
        showNoLyric();
    }
    
    // 查找当前时间对应的歌词行
    int findCurrentLine(qint64 position)
    {
        if (m_lyrics.isEmpty()) return -1;
        
        // 从后往前查找第一个时间戳小于等于当前位置的歌词
        for (int i = m_lyrics.size() - 1; i >= 0; --i) {
            if (m_lyrics[i].timestamp <= position) {
                return i;
            }
        }
        
        return -1;
    }
    
    // 更新显示内容
    void updateDisplay()
    {
        if (m_currentLineIndex < 0 || m_currentLineIndex >= m_lyrics.size()) {
            m_prevLine->clear();
            m_currentLine->clear();
            m_nextLine->clear();
            return;
        }
        
        // 上一行
        if (m_currentLineIndex > 0) {
            m_prevLine->setText(m_lyrics[m_currentLineIndex - 1].text);
        } else {
            m_prevLine->clear();
        }
        
        // 当前行
        m_currentLine->setText(m_lyrics[m_currentLineIndex].text);
        
        // 下一行
        if (m_currentLineIndex < m_lyrics.size() - 1) {
            m_nextLine->setText(m_lyrics[m_currentLineIndex + 1].text);
        } else {
            m_nextLine->clear();
        }
    }
    
    // 播放当前行动画
    void animateCurrentLine()
    {
        if (m_fadeAnimation->state() == QAbstractAnimation::Running) {
            m_fadeAnimation->stop();
        }
        m_fadeAnimation->start();
    }
    
    // 显示无歌词提示
    void showNoLyric()
    {
        m_prevLine->hide();
        m_currentLine->hide();
        m_nextLine->hide();
        m_noLyricLabel->show();
    }
    
    // 隐藏无歌词提示
    void hideNoLyric()
    {
        m_noLyricLabel->hide();
        m_prevLine->show();
        m_currentLine->show();
        m_nextLine->show();
    }
};

#endif // LYRICWIDGET_H
