#include "unifiedhistorydialog.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTabBar>
#include <QVBoxLayout>

namespace {
constexpr int kHistoryLimit = 100;
constexpr int kRecordIndexRole = Qt::UserRole;

bool isLocalHistoryPath(const QString& filePath)
{
    return !filePath.startsWith(QStringLiteral("online-audio:"))
        && !filePath.startsWith(QStringLiteral("http://"))
        && !filePath.startsWith(QStringLiteral("https://"));
}
}

UnifiedHistoryDialog::UnifiedHistoryDialog(MediaHistoryService* historyService,
                                           QWidget* parent,
                                           MediaKind initialFilter,
                                           bool lockFilter)
    : QDialog(parent)
    , m_historyService(historyService)
    , m_initialFilter(initialFilter)
    , m_lockFilter(lockFilter)
{
    setupUi();
    reload();
}

void UnifiedHistoryDialog::setupUi()
{
    setWindowTitle(QStringLiteral("播放历史"));
    setMinimumSize(760, 520);
    setStyleSheet(
        "QDialog { background-color: #2b2b2b; }"
        "QTabBar::tab { background: #333; color: #ddd; padding: 8px 20px; border: 1px solid #444; }"
        "QTabBar::tab:selected { background: #0d47a1; color: white; }"
        "QListWidget { background-color: #1e1e1e; color: #ffffff; border: 1px solid #444; border-radius: 5px; padding: 5px; }"
        "QListWidget::item { padding: 8px; border-bottom: 1px solid #333; }"
        "QListWidget::item:hover { background-color: #3a3a3a; }"
        "QListWidget::item:selected { background-color: #0d47a1; }"
        "QLabel { color: #ffffff; }");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    QLabel* titleLabel = new QLabel(QStringLiteral("最近播放记录"), this);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 16px; color: #64b5f6; font-weight: bold;"));
    mainLayout->addWidget(titleLabel);

    m_filterTabs = new QTabBar(this);
    m_filterTabs->addTab(QStringLiteral("全部"));
    m_filterTabs->setTabData(0, static_cast<int>(MediaKind::Unknown));
    m_filterTabs->addTab(QStringLiteral("音频"));
    m_filterTabs->setTabData(1, static_cast<int>(MediaKind::Audio));
    m_filterTabs->addTab(QStringLiteral("视频"));
    m_filterTabs->setTabData(2, static_cast<int>(MediaKind::Video));
    const int initialTab = tabIndexForFilter(m_initialFilter);
    if (initialTab >= 0) {
        m_filterTabs->setCurrentIndex(initialTab);
    }
    m_filterTabs->setVisible(!m_lockFilter);
    mainLayout->addWidget(m_filterTabs);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #90caf9; font-size: 12px;"));
    mainLayout->addWidget(m_statusLabel);

    m_listWidget = new QListWidget(this);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    mainLayout->addWidget(m_listWidget, 1);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_playButton = new QPushButton(QStringLiteral("播放选中"), this);
    m_removeButton = new QPushButton(QStringLiteral("删除记录"), this);
    m_clearButton = new QPushButton(QStringLiteral("清空当前"), this);
    QPushButton* closeButton = new QPushButton(QStringLiteral("关闭"), this);

    m_playButton->setProperty("role", "primary");
    m_removeButton->setProperty("role", "danger");
    m_clearButton->setProperty("role", "danger");
    closeButton->setProperty("role", "secondary");

    buttonLayout->addWidget(m_playButton);
    buttonLayout->addWidget(m_removeButton);
    buttonLayout->addWidget(m_clearButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    mainLayout->addLayout(buttonLayout);

    connect(m_filterTabs, &QTabBar::currentChanged, this, &UnifiedHistoryDialog::reload);
    connect(m_playButton, &QPushButton::clicked, this, &UnifiedHistoryDialog::playSelected);
    connect(m_removeButton, &QPushButton::clicked, this, &UnifiedHistoryDialog::removeSelected);
    connect(m_clearButton, &QPushButton::clicked, this, &UnifiedHistoryDialog::clearCurrentFilter);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        playSelected();
    });
}

void UnifiedHistoryDialog::reload()
{
    m_listWidget->clear();
    m_records = recordsForCurrentFilter();

    const MediaKind filter = currentFilter();
    const QString filterName = filter == MediaKind::Unknown
        ? QStringLiteral("全部")
        : displayNameForKind(filter);

    if (m_records.isEmpty()) {
        QListWidgetItem* item = new QListWidgetItem(QStringLiteral("暂无播放历史记录"));
        item->setTextAlignment(Qt::AlignCenter);
        item->setFlags(Qt::NoItemFlags);
        m_listWidget->addItem(item);
        m_statusLabel->setText(QStringLiteral("%1历史为空").arg(filterName));
        return;
    }

    for (int i = 0; i < m_records.size(); ++i) {
        addRecordItem(m_records.at(i), i);
    }
    m_statusLabel->setText(QStringLiteral("%1历史：%2 条").arg(filterName).arg(m_records.size()));
}

void UnifiedHistoryDialog::playSelected()
{
    QListWidgetItem* item = currentPlayableItem();
    if (!item) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一条播放历史。"));
        return;
    }

    const int index = item->data(kRecordIndexRole).toInt();
    if (index < 0 || index >= m_records.size()) {
        return;
    }

    const MediaHistoryRecord record = m_records.at(index);
    const MediaKind kind = mediaKindFromString(record.fileType);
    if (kind == MediaKind::Unknown) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("无法识别该历史记录的媒体类型。"));
        return;
    }

    if (isLocalHistoryPath(record.filePath) && !QFileInfo::exists(record.filePath)) {
        if (confirmMissingFileRemoval(record.filePath)) {
            m_historyService->removeRecord(record.filePath, kind);
            reload();
        }
        return;
    }

    if (kind == MediaKind::Audio) {
        emit playRequested(record);
        emit playAudio(record.filePath);
    } else if (kind == MediaKind::Video) {
        emit playRequested(record);
        emit playVideo(record.filePath, record.isCompleted ? qint64(0) : record.lastPosition);
    }
    accept();
}

void UnifiedHistoryDialog::removeSelected()
{
    QListWidgetItem* item = currentPlayableItem();
    if (!item) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一条播放历史。"));
        return;
    }

    const int index = item->data(kRecordIndexRole).toInt();
    if (index < 0 || index >= m_records.size()) {
        return;
    }

    const MediaHistoryRecord record = m_records.at(index);
    const MediaKind kind = mediaKindFromString(record.fileType);
    const QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        QStringLiteral("确认删除"),
        QStringLiteral("确定要删除这条播放历史吗？\n\n%1").arg(record.fileName),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes && m_historyService->removeRecord(record.filePath, kind)) {
        reload();
    }
}

void UnifiedHistoryDialog::clearCurrentFilter()
{
    if (m_records.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("没有可清空的播放历史。"));
        return;
    }

    const MediaKind filter = currentFilter();
    const QString target = filter == MediaKind::Unknown
        ? QStringLiteral("全部")
        : displayNameForKind(filter);
    const QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        QStringLiteral("确认清空"),
        QStringLiteral("确定要清空%1播放历史吗？").arg(target),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    const bool ok = filter == MediaKind::Unknown
        ? m_historyService->clearHistory()
        : m_historyService->clearHistory(filter);
    if (ok) {
        reload();
    }
}

MediaKind UnifiedHistoryDialog::currentFilter() const
{
    if (!m_filterTabs) {
        return MediaKind::Unknown;
    }

    return static_cast<MediaKind>(m_filterTabs->tabData(m_filterTabs->currentIndex()).toInt());
}

QVector<MediaHistoryRecord> UnifiedHistoryDialog::recordsForCurrentFilter() const
{
    if (!m_historyService) {
        return {};
    }

    const MediaKind filter = currentFilter();
    if (filter == MediaKind::Audio || filter == MediaKind::Video) {
        return m_historyService->recentHistory(kHistoryLimit, filter);
    }

    return m_historyService->recentHistory(kHistoryLimit);
}

void UnifiedHistoryDialog::addRecordItem(const MediaHistoryRecord& record, int index)
{
    QListWidgetItem* item = new QListWidgetItem(displayTextForRecord(record));
    item->setData(kRecordIndexRole, index);
    m_listWidget->addItem(item);
}

QString UnifiedHistoryDialog::displayNameForKind(MediaKind kind) const
{
    switch (kind) {
    case MediaKind::Audio:
        return QStringLiteral("音频");
    case MediaKind::Video:
        return QStringLiteral("视频");
    case MediaKind::Unknown:
        break;
    }

    return QStringLiteral("未知");
}

QString UnifiedHistoryDialog::displayTextForRecord(const MediaHistoryRecord& record) const
{
    const MediaKind kind = mediaKindFromString(record.fileType);
    const QString fileName = record.fileName.trimmed().isEmpty()
        ? record.filePath
        : record.fileName.trimmed();

    QString progressText;
    if (record.duration > 0) {
        progressText = QStringLiteral(" | 进度: %1 / %2 (%3%)")
            .arg(record.positionText())
            .arg(record.durationText())
            .arg(record.progressPercent());
    }

    return QStringLiteral("%1\n类型: %2 | 播放次数: %3 | 最后播放: %4%5\n%6")
        .arg(fileName)
        .arg(displayNameForKind(kind))
        .arg(record.playCount)
        .arg(record.lastPlayTime.toString(QStringLiteral("yyyy-MM-dd hh:mm")))
        .arg(progressText)
        .arg(record.filePath);
}

bool UnifiedHistoryDialog::confirmMissingFileRemoval(const QString& filePath)
{
    const QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        QStringLiteral("文件不存在"),
        QStringLiteral("文件不存在或已被删除：\n\n%1\n\n是否删除此历史记录？").arg(filePath),
        QMessageBox::Yes | QMessageBox::No);

    return reply == QMessageBox::Yes;
}

QListWidgetItem* UnifiedHistoryDialog::currentPlayableItem() const
{
    QListWidgetItem* item = m_listWidget ? m_listWidget->currentItem() : nullptr;
    if (!item || item->flags() == Qt::NoItemFlags) {
        return nullptr;
    }

    return item;
}

int UnifiedHistoryDialog::tabIndexForFilter(MediaKind kind) const
{
    if (!m_filterTabs) {
        return -1;
    }

    for (int i = 0; i < m_filterTabs->count(); ++i) {
        if (static_cast<MediaKind>(m_filterTabs->tabData(i).toInt()) == kind) {
            return i;
        }
    }

    return -1;
}
