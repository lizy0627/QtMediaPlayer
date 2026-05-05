#include "audioplayerwidget.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

#include "audiocontrolbar.h"
#include "audiostyle.h"
#include "lyricpanel.h"
#include "playlistpanel.h"
#include "spectrumpanel.h"

AudioPlayerWidget::AudioPlayerWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("AudioPlayerWidget");
    AudioStyle::apply(this);

    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    auto* splitter = new QSplitter(Qt::Horizontal, this);

    auto* leftPanel = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(10);

    auto* albumContainer = new QWidget(leftPanel);
    auto* albumLayout = new QVBoxLayout(albumContainer);
    albumLayout->setContentsMargins(0, 0, 0, 0);
    albumLayout->setSpacing(6);

    m_albumArt = new QLabel(albumContainer);
    m_albumArt->setObjectName("albumArt");
    m_albumArt->setMinimumSize(400, 300);
    m_albumArt->setAlignment(Qt::AlignCenter);
    m_albumArt->setCursor(Qt::PointingHandCursor);
    m_albumArt->setToolTip(QStringLiteral("点击更换专辑封面"));
    m_albumArt->installEventFilter(this);
    albumLayout->addWidget(m_albumArt);

    auto* changeAlbumButton = new QPushButton(QStringLiteral("更换专辑封面"), albumContainer);
    AudioStyle::setButtonRole(changeAlbumButton, "primary");
    connect(changeAlbumButton, &QPushButton::clicked,
            this, &AudioPlayerWidget::albumArtChangeRequested);
    albumLayout->addWidget(changeAlbumButton);
    leftLayout->addWidget(albumContainer);

    m_spectrumPanel = new SpectrumPanel(leftPanel);
    leftLayout->addWidget(m_spectrumPanel);

    m_lyricPanel = new LyricPanel(leftPanel);
    leftLayout->addWidget(m_lyricPanel);

    splitter->addWidget(leftPanel);

    auto* rightPanel = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);

    m_playlistPanel = new PlaylistPanel(rightPanel);
    rightLayout->addWidget(m_playlistPanel);

    m_controlBar = new AudioControlBar(rightPanel);
    rightLayout->addWidget(m_controlBar);

    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter);
}

PlaylistPanel* AudioPlayerWidget::playlistPanel() const
{
    return m_playlistPanel;
}

AudioControlBar* AudioPlayerWidget::controlBar() const
{
    return m_controlBar;
}

LyricPanel* AudioPlayerWidget::lyricPanel() const
{
    return m_lyricPanel;
}

SpectrumPanel* AudioPlayerWidget::spectrumPanel() const
{
    return m_spectrumPanel;
}

QSize AudioPlayerWidget::albumArtSize() const
{
    return m_albumArt->size();
}

void AudioPlayerWidget::setPlaylistModel(PlaylistModel* model)
{
    m_playlistPanel->setModel(model);
}

void AudioPlayerWidget::setAlbumArtPixmap(const QPixmap& pixmap)
{
    m_albumArt->setPixmap(pixmap);
}

void AudioPlayerWidget::setAlbumArtToolTip(const QString& text)
{
    m_albumArt->setToolTip(text);
}

bool AudioPlayerWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_albumArt && event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            emit albumArtChangeRequested();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}
