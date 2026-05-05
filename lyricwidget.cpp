#include "lyricwidget.h"

#include <QAbstractAnimation>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPropertyAnimation>
#include <QSizePolicy>
#include <QVBoxLayout>

LyricLine::LyricLine(qint64 time, const QString& lyric)
    : timestamp(time)
    , text(lyric)
{
}

LyricWidget::LyricWidget(QWidget* parent)
    : QWidget(parent)
    , m_currentLineIndex(-1)
    , m_currentPosition(0)
    , m_prevLine(nullptr)
    , m_currentLine(nullptr)
    , m_nextLine(nullptr)
    , m_statusTitleLabel(nullptr)
    , m_statusDetailLabel(nullptr)
    , m_lyricContainer(nullptr)
    , m_fadeAnimation(nullptr)
{
    setupUI();
}

void LyricWidget::setLyrics(const QList<LyricLine>& lyrics)
{
    m_lyrics = lyrics;
    m_currentLineIndex = -1;
    m_currentPosition = 0;

    if (m_lyrics.isEmpty()) {
        showNoLyric();
        return;
    }

    hideNoLyric();
    updateDisplay();
}

void LyricWidget::showStatus(LyricDisplayState state, const QString& detail)
{
    m_lyrics.clear();
    m_currentLineIndex = -1;
    m_currentPosition = 0;

    m_prevLine->hide();
    m_currentLine->hide();
    m_nextLine->hide();

    m_statusTitleLabel->setText(statusTitle(state));
    m_statusDetailLabel->setText(detail.trimmed());
    m_statusDetailLabel->setVisible(!detail.trimmed().isEmpty());
    m_statusTitleLabel->show();
}

void LyricWidget::clear()
{
    showNoLyric();
}

void LyricWidget::updatePosition(qint64 position)
{
    m_currentPosition = position;

    if (m_lyrics.isEmpty()) {
        return;
    }

    const int newIndex = findCurrentLine(position);

    if (newIndex != m_currentLineIndex && newIndex >= 0) {
        m_currentLineIndex = newIndex;
        updateDisplay();
        animateCurrentLine();
    }
}

void LyricWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(30, 40, 30, 40);
    mainLayout->setSpacing(30);

    m_lyricContainer = new QWidget(this);
    m_lyricContainer->setObjectName("lyricContainer");
    m_lyricContainer->setMinimumHeight(450);
    m_lyricContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_lyricContainer->setStyleSheet(
        "QWidget#lyricContainer { "
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "       stop:0 rgba(13, 71, 161, 0.15), "
        "       stop:1 rgba(21, 101, 192, 0.08)); "
        "   border-radius: 20px; "
        "   border: 2px solid rgba(100, 181, 246, 0.4); "
        "}"
    );

    auto* lyricLayout = new QVBoxLayout(m_lyricContainer);
    lyricLayout->setContentsMargins(36, 52, 36, 52);
    lyricLayout->setSpacing(28);
    lyricLayout->setAlignment(Qt::AlignCenter);

    m_prevLine = new QLabel(m_lyricContainer);
    m_prevLine->setAlignment(Qt::AlignCenter);
    m_prevLine->setWordWrap(true);
    m_prevLine->setMinimumHeight(70);
    m_prevLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_prevLine->setStyleSheet(
        "QLabel { "
        "   color: rgba(255, 255, 255, 0.5); "
        "   font-size: 18pt; "
        "   font-weight: normal; "
        "   padding: 15px; "
        "   line-height: 1.8; "
        "   background: transparent; "
        "   border: none; "
        "}"
    );
    lyricLayout->addWidget(m_prevLine);

    m_currentLine = new QLabel(m_lyricContainer);
    m_currentLine->setAlignment(Qt::AlignCenter);
    m_currentLine->setWordWrap(true);
    m_currentLine->setMinimumHeight(100);
    m_currentLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_currentLine->setStyleSheet(
        "QLabel { "
        "   color: #64b5f6; "
        "   font-size: 28pt; "
        "   font-weight: bold; "
        "   padding: 25px; "
        "   background: rgba(100, 181, 246, 0.18); "
        "   border-radius: 15px; "
        "   line-height: 2.0; "
        "}"
    );

    auto* opacityEffect = new QGraphicsOpacityEffect(m_currentLine);
    m_currentLine->setGraphicsEffect(opacityEffect);

    m_fadeAnimation = new QPropertyAnimation(opacityEffect, "opacity", this);
    m_fadeAnimation->setDuration(300);
    m_fadeAnimation->setStartValue(0.3);
    m_fadeAnimation->setEndValue(1.0);

    lyricLayout->addWidget(m_currentLine);

    m_nextLine = new QLabel(m_lyricContainer);
    m_nextLine->setAlignment(Qt::AlignCenter);
    m_nextLine->setWordWrap(true);
    m_nextLine->setMinimumHeight(70);
    m_nextLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_nextLine->setStyleSheet(
        "QLabel { "
        "   color: rgba(255, 255, 255, 0.5); "
        "   font-size: 18pt; "
        "   font-weight: normal; "
        "   padding: 15px; "
        "   line-height: 1.8; "
        "   background: transparent; "
        "   border: none; "
        "}"
    );
    lyricLayout->addWidget(m_nextLine);

    m_statusTitleLabel = new QLabel(QStringLiteral("暂无歌词"), m_lyricContainer);
    m_statusTitleLabel->setAlignment(Qt::AlignCenter);
    m_statusTitleLabel->setWordWrap(true);
    m_statusTitleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_statusTitleLabel->setStyleSheet(
        "QLabel { "
        "   color: rgba(255, 255, 255, 0.82); "
        "   font-size: 28pt; "
        "   font-weight: bold; "
        "   padding: 18px; "
        "   background: transparent; "
        "   border: none; "
        "}"
    );
    lyricLayout->addWidget(m_statusTitleLabel);

    m_statusDetailLabel = new QLabel(m_lyricContainer);
    m_statusDetailLabel->setAlignment(Qt::AlignCenter);
    m_statusDetailLabel->setWordWrap(true);
    m_statusDetailLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_statusDetailLabel->setStyleSheet(
        "QLabel { "
        "   color: rgba(255, 255, 255, 0.62); "
        "   font-size: 13pt; "
        "   font-weight: normal; "
        "   padding: 0 20px; "
        "   background: transparent; "
        "   border: none; "
        "}"
    );
    lyricLayout->addWidget(m_statusDetailLabel);

    mainLayout->addWidget(m_lyricContainer);

    showNoLyric();
}

int LyricWidget::findCurrentLine(qint64 position)
{
    if (m_lyrics.isEmpty()) {
        return -1;
    }

    for (int i = m_lyrics.size() - 1; i >= 0; --i) {
        if (m_lyrics[i].timestamp <= position) {
            return i;
        }
    }

    return -1;
}

void LyricWidget::updateDisplay()
{
    if (m_currentLineIndex < 0 || m_currentLineIndex >= m_lyrics.size()) {
        m_prevLine->clear();
        m_currentLine->clear();
        m_nextLine->clear();
        return;
    }

    m_prevLine->setText(m_currentLineIndex > 0 ? m_lyrics[m_currentLineIndex - 1].text : QString());
    m_currentLine->setText(m_lyrics[m_currentLineIndex].text);
    m_nextLine->setText(m_currentLineIndex < m_lyrics.size() - 1
                            ? m_lyrics[m_currentLineIndex + 1].text
                            : QString());
}

void LyricWidget::animateCurrentLine()
{
    if (m_fadeAnimation->state() == QAbstractAnimation::Running) {
        m_fadeAnimation->stop();
    }
    m_fadeAnimation->start();
}

void LyricWidget::showNoLyric()
{
    showStatus(LyricDisplayState::NoLyric);
}

void LyricWidget::hideNoLyric()
{
    m_statusTitleLabel->hide();
    m_statusDetailLabel->hide();
    m_prevLine->show();
    m_currentLine->show();
    m_nextLine->show();
}

QString LyricWidget::statusTitle(LyricDisplayState state) const
{
    switch (state) {
    case LyricDisplayState::NoLyric:
        return QStringLiteral("暂无歌词");
    case LyricDisplayState::DownloadFailed:
        return QStringLiteral("歌词下载失败");
    case LyricDisplayState::ParseFailed:
        return QStringLiteral("歌词解析失败");
    case LyricDisplayState::SavePermissionDenied:
        return QStringLiteral("无权限保存歌词");
    }

    return QStringLiteral("暂无歌词");
}
