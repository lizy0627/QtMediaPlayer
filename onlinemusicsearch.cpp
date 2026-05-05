#include "onlinemusicsearch.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

#include "audiostyle.h"

OnlineMusicSearch::OnlineMusicSearch(OnlineMusicService* service, QWidget* parent)
    : QDialog(parent)
    , m_service(service)
{
    setObjectName("OnlineMusicSearchDialog");
    setWindowTitle(QStringLiteral("在线音乐搜索"));
    setMinimumSize(800, 600);
    AudioStyle::apply(this);
    setupUI();

    connect(m_service,
            QOverload<const QList<SongInfo>&, const QString&>::of(&OnlineMusicService::searchFinished),
            this,
            &OnlineMusicSearch::onSearchFinished);
    connect(m_service, &OnlineMusicService::searchError,
            this, &OnlineMusicSearch::onSearchError);
}

SongInfo OnlineMusicSearch::getSelectedSong() const
{
    const int row = m_resultList->currentRow();
    if (row >= 0 && row < m_songs.size()) {
        return m_songs[row];
    }
    return SongInfo();
}

void OnlineMusicSearch::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    auto* titleLabel = new QLabel(QStringLiteral("在线音乐搜索"), this);
    titleLabel->setObjectName("accentLabel");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    auto* searchLayout = new QHBoxLayout();
    searchLayout->setSpacing(10);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("请输入歌曲名称或艺术家..."));
    searchLayout->addWidget(m_searchEdit, 1);

    m_searchButton = new QPushButton(QStringLiteral("搜索"), this);
    AudioStyle::setButtonRole(m_searchButton, "primary");
    m_searchButton->setFixedWidth(120);
    searchLayout->addWidget(m_searchButton);
    mainLayout->addLayout(searchLayout);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0);
    m_progressBar->hide();
    mainLayout->addWidget(m_progressBar);

    m_statusLabel = new QLabel(QStringLiteral("请输入关键词开始搜索"), this);
    m_statusLabel->setObjectName("accentLabel");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_statusLabel);

    auto* resultLabel = new QLabel(QStringLiteral("搜索结果（双击加入播放列表）："), this);
    resultLabel->setObjectName("accentLabel");
    mainLayout->addWidget(resultLabel);

    m_resultList = new QListWidget(this);
    mainLayout->addWidget(m_resultList);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    auto* playButton = new QPushButton(QStringLiteral("加入选中歌曲"), this);
    auto* closeButton = new QPushButton(QStringLiteral("关闭"), this);
    AudioStyle::setButtonRole(playButton, "primary");
    AudioStyle::setButtonRole(closeButton, "secondary");

    buttonLayout->addWidget(playButton);
    buttonLayout->addWidget(closeButton);
    mainLayout->addLayout(buttonLayout);

    connect(m_searchEdit, &QLineEdit::returnPressed, this, &OnlineMusicSearch::onSearch);
    connect(m_searchButton, &QPushButton::clicked, this, &OnlineMusicSearch::onSearch);
    connect(m_resultList, &QListWidget::itemDoubleClicked, this, &OnlineMusicSearch::onPlaySelected);
    connect(playButton, &QPushButton::clicked, this, &OnlineMusicSearch::onPlaySelected);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
}

void OnlineMusicSearch::onSearch()
{
    const QString keyword = m_searchEdit->text().trimmed();
    if (keyword.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入搜索关键词。"));
        return;
    }

    m_statusLabel->setText(QStringLiteral("正在搜索：%1").arg(keyword));
    m_progressBar->show();
    m_searchButton->setEnabled(false);
    m_resultList->clear();
    m_songs.clear();

    m_service->searchSongsAsync(keyword);
}

void OnlineMusicSearch::onSearchFinished(const QList<SongInfo>& songs, const QString& statusMessage)
{
    m_progressBar->hide();
    m_searchButton->setEnabled(true);
    m_statusLabel->setText(statusMessage);
    showSongs(songs);
}

void OnlineMusicSearch::onSearchError(const QString& message)
{
    m_progressBar->hide();
    m_searchButton->setEnabled(true);
    m_statusLabel->setText(message);
    m_resultList->clear();
    m_songs.clear();
}

void OnlineMusicSearch::onPlaySelected()
{
    const int row = m_resultList->currentRow();
    if (row < 0 || row >= m_songs.size()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择一首歌曲。"));
        return;
    }

    emit songSelected(m_songs[row]);
    accept();
}

void OnlineMusicSearch::showSongs(const QList<SongInfo>& songs)
{
    m_songs = songs;
    m_resultList->clear();

    for (const SongInfo& song : m_songs) {
        m_resultList->addItem(formatSongDisplay(song));
    }
}

QString OnlineMusicSearch::formatSongDisplay(const SongInfo& song) const
{
    return QStringLiteral("%1\n%2  |  %3  |  %4:%5")
        .arg(song.title.isEmpty() ? song.name : song.title)
        .arg(song.artist)
        .arg(song.album)
        .arg(song.duration / 60)
        .arg(song.duration % 60, 2, 10, QChar('0'));
}
