#include "onlinevideosearch.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include "onlinevideoservice.h"

OnlineVideoSearch::OnlineVideoSearch(OnlineVideoService* service, QWidget* parent)
    : QDialog(parent)
    , m_service(service)
{
    setWindowTitle(QStringLiteral("在线视频搜索 - Bilibili"));
    setMinimumSize(1000, 700);
    setupUI();

    if (m_service) {
        connect(m_service, &OnlineVideoService::searchStarted, this, [this](const QString& keyword) {
            m_statusLabel->setText(QStringLiteral("正在搜索：%1").arg(keyword));
            m_progressBar->show();
            m_searchButton->setEnabled(false);
        });
        connect(m_service,
                &OnlineVideoService::searchFinished,
                this,
                &OnlineVideoSearch::onSearchFinished);
        connect(m_service,
                qOverload<const QString&>(&OnlineVideoService::searchFailed),
                this,
                &OnlineVideoSearch::onSearchFailed);
    }
}

VideoInfo OnlineVideoSearch::getSelectedVideo() const
{
    const int row = m_resultList->currentRow();
    if (row >= 0 && row < m_videos.size()) {
        return m_videos[row];
    }
    return VideoInfo();
}

void OnlineVideoSearch::setupUI()
{
    setObjectName("OnlineVideoSearchDialog");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    auto* titleLabel = new QLabel(QStringLiteral("Bilibili 视频搜索"), this);
    titleLabel->setProperty("role", "title");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    auto* searchLayout = new QHBoxLayout();
    searchLayout->setSpacing(15);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索 Bilibili 视频..."));
    searchLayout->addWidget(m_searchEdit, 1);

    m_searchButton = new QPushButton(QStringLiteral("搜索"), this);
    m_searchButton->setFixedWidth(120);
    searchLayout->addWidget(m_searchButton);

    mainLayout->addLayout(searchLayout);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0);
    m_progressBar->hide();
    mainLayout->addWidget(m_progressBar);

    m_statusLabel = new QLabel(QStringLiteral("输入关键词后开始搜索"), this);
    m_statusLabel->setProperty("role", "status");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_statusLabel);

    auto* resultLabel = new QLabel(QStringLiteral("搜索结果（双击发送到播放器）："), this);
    resultLabel->setProperty("role", "sectionTitle");
    mainLayout->addWidget(resultLabel);

    m_resultList = new QListWidget(this);
    mainLayout->addWidget(m_resultList);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    auto* playButton = new QPushButton(QStringLiteral("播放选中"), this);
    playButton->setProperty("role", "primary");

    auto* openPageButton = new QPushButton(QStringLiteral("在浏览器打开"), this);
    openPageButton->setProperty("role", "secondary");

    auto* closeButton = new QPushButton(QStringLiteral("关闭"), this);
    closeButton->setProperty("role", "neutral");

    buttonLayout->addWidget(playButton);
    buttonLayout->addWidget(openPageButton);
    buttonLayout->addWidget(closeButton);
    mainLayout->addLayout(buttonLayout);

    connect(m_searchEdit, &QLineEdit::returnPressed, this, &OnlineVideoSearch::onSearch);
    connect(m_searchButton, &QPushButton::clicked, this, &OnlineVideoSearch::onSearch);
    connect(m_resultList, &QListWidget::itemDoubleClicked, this, &OnlineVideoSearch::onPlaySelected);
    connect(playButton, &QPushButton::clicked, this, &OnlineVideoSearch::onPlaySelected);
    connect(openPageButton, &QPushButton::clicked, this, &OnlineVideoSearch::onOpenPageSelected);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
}

QString OnlineVideoSearch::formatVideoDisplay(const VideoInfo& video) const
{
    const QString description = video.description.length() > 80
        ? video.description.left(80) + QStringLiteral("...")
        : video.description;

    return QStringLiteral("%1\nUP 主: %2  |  时长: %3\n%4")
        .arg(video.title)
        .arg(video.author)
        .arg(video.duration)
        .arg(description);
}

void OnlineVideoSearch::onSearch()
{
    const QString keyword = m_searchEdit->text().trimmed();
    if (keyword.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入搜索关键词。"));
        return;
    }

    m_resultList->clear();
    m_videos.clear();

    if (m_service) {
        m_service->searchVideo(keyword);
    }
}

void OnlineVideoSearch::onSearchFinished(const QList<VideoInfo>& videos,
                                         const QString& statusMessage)
{
    m_progressBar->hide();
    m_searchButton->setEnabled(true);
    m_statusLabel->setText(statusMessage);

    m_videos = videos;
    m_resultList->clear();
    for (const VideoInfo& video : m_videos) {
        m_resultList->addItem(formatVideoDisplay(video));
    }
}

void OnlineVideoSearch::onSearchFailed(const QString& message)
{
    m_progressBar->hide();
    m_searchButton->setEnabled(true);
    m_statusLabel->setText(QStringLiteral("搜索失败：%1").arg(message));
    QMessageBox::warning(this, QStringLiteral("搜索失败"), message);
}

void OnlineVideoSearch::onPlaySelected()
{
    const int row = m_resultList->currentRow();
    if (row < 0 || row >= m_videos.size()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择一个视频。"));
        return;
    }

    const VideoInfo& video = m_videos[row];
    m_statusLabel->setText(QStringLiteral("已发送到播放器：%1").arg(video.title));
    emit videoSelected(video);
    accept();
}

void OnlineVideoSearch::onOpenPageSelected()
{
    const int row = m_resultList->currentRow();
    if (row < 0 || row >= m_videos.size()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择一个视频。"));
        return;
    }

    const VideoInfo& video = m_videos[row];
    const QString pageUrl = video.pageUrl.isEmpty()
        ? QStringLiteral("https://www.bilibili.com/video/%1").arg(video.bvid)
        : video.pageUrl;

    if (!QDesktopServices::openUrl(QUrl(pageUrl))) {
        QMessageBox::warning(this,
                             QStringLiteral("打开失败"),
                             QStringLiteral("无法打开该视频网页，请稍后重试。"));
        m_statusLabel->setText(QStringLiteral("打开网页失败"));
        return;
    }

    m_statusLabel->setText(QStringLiteral("已在浏览器中打开：%1").arg(video.title));
    accept();
}
