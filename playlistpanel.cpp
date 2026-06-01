#include "playlistpanel.h"

#include <QAction>
#include <QBrush>
#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPoint>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include "audiostyle.h"
#include "audiotrack.h"
#include "playlistmodel.h"

namespace {
QString statusSuffix(AudioTrackPlaybackStatus status)
{
    switch (status) {
    case AudioTrackPlaybackStatus::PendingValidation:
        return QStringLiteral(" [待验证]");
    case AudioTrackPlaybackStatus::Loading:
        return QStringLiteral(" [正在验证]");
    case AudioTrackPlaybackStatus::Playing:
        return QStringLiteral(" [播放中]");
    case AudioTrackPlaybackStatus::Failed:
        return QStringLiteral(" [播放失败]");
    case AudioTrackPlaybackStatus::Idle:
        break;
    }

    return QString();
}

QColor statusColor(AudioTrackPlaybackStatus status)
{
    switch (status) {
    case AudioTrackPlaybackStatus::PendingValidation:
    case AudioTrackPlaybackStatus::Loading:
        return QColor(QStringLiteral("#90caf9"));
    case AudioTrackPlaybackStatus::Playing:
        return QColor(QStringLiteral("#a5d6a7"));
    case AudioTrackPlaybackStatus::Failed:
        return QColor(QStringLiteral("#ef9a9a"));
    case AudioTrackPlaybackStatus::Idle:
        break;
    }

    return QColor();
}
}

PlaylistPanel::PlaylistPanel(QWidget* parent)
    : QGroupBox(QStringLiteral("播放列表"), parent)
{
    setObjectName("audioGroup");

    auto* playlistLayout = new QVBoxLayout(this);
    playlistLayout->setContentsMargins(8, 18, 8, 8);
    playlistLayout->setSpacing(10);

    auto* userLayout = new QHBoxLayout();
    userLayout->setSpacing(10);

    m_userLabel = new QLabel(QStringLiteral("未登录"), this);
    m_userLabel->setObjectName("mutedLabel");

    m_loginButton = createActionButton(QStringLiteral("登录"), "secondary");
    m_loginButton->setFixedHeight(35);
    connect(m_loginButton, &QPushButton::clicked, this, &PlaylistPanel::loginRequested);

    userLayout->addWidget(m_userLabel);
    userLayout->addStretch();
    userLayout->addWidget(m_loginButton);
    playlistLayout->addLayout(userLayout);

    m_listWidget = new QListWidget(this);
    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    playlistLayout->addWidget(m_listWidget);

    auto* addButtonLayout = new QHBoxLayout();
    auto* addButton = createActionButton(QStringLiteral("添加本地音乐"), "primary");
    auto* searchButton = createActionButton(QStringLiteral("在线搜索"), "secondary");
    connect(addButton, &QPushButton::clicked, this, &PlaylistPanel::addFilesRequested);
    connect(searchButton, &QPushButton::clicked, this, &PlaylistPanel::onlineSearchRequested);
    addButtonLayout->addWidget(addButton);
    addButtonLayout->addWidget(searchButton);
    playlistLayout->addLayout(addButtonLayout);

    auto* actionButtonLayout = new QHBoxLayout();
    auto* retryButton = createActionButton(QStringLiteral("\u91cd\u8bd5\u64ad\u653e"), "warning");
    auto* deleteButton = createActionButton(QStringLiteral("删除选中"), "danger");
    auto* testButton = createActionButton(QStringLiteral("测试音频"), "warning");
    connect(deleteButton, &QPushButton::clicked, this, &PlaylistPanel::deleteSelectedRequested);
    connect(retryButton, &QPushButton::clicked, this, &PlaylistPanel::retrySelectedRequested);
    connect(testButton, &QPushButton::clicked, this, &PlaylistPanel::testAudioRequested);
    actionButtonLayout->addWidget(deleteButton);
    actionButtonLayout->addWidget(retryButton);
    actionButtonLayout->addWidget(testButton);
    playlistLayout->addLayout(actionButtonLayout);

    connect(m_listWidget, &QListWidget::customContextMenuRequested,
            this, &PlaylistPanel::showContextMenu);
    connect(m_listWidget, &QListWidget::itemDoubleClicked,
            this, &PlaylistPanel::onItemDoubleClicked);
}

void PlaylistPanel::setModel(PlaylistModel* model)
{
    if (m_model == model) {
        return;
    }

    if (m_model) {
        disconnect(m_model, nullptr, this, nullptr);
    }

    m_model = model;
    if (!m_model) {
        m_listWidget->clear();
        return;
    }

    connect(m_model, &PlaylistModel::changed,
            this, &PlaylistPanel::refreshFromModel);
    connect(m_model, &PlaylistModel::currentIndexChanged,
            this, &PlaylistPanel::setCurrentRow);
    refreshFromModel();
}

void PlaylistPanel::setCurrentRow(int row)
{
    m_listWidget->setCurrentRow(row);
}

int PlaylistPanel::currentRow() const
{
    return m_listWidget->currentRow();
}

QString PlaylistPanel::currentSongName() const
{
    QListWidgetItem* item = m_listWidget->currentItem();
    return item ? item->text() : QString();
}

void PlaylistPanel::setLoggedInUser(const QString& username)
{
    if (username.isEmpty()) {
        m_userLabel->setText(QStringLiteral("未登录"));
        m_userLabel->setObjectName("mutedLabel");
        m_loginButton->setText(QStringLiteral("登录"));
    } else {
        m_userLabel->setText(QStringLiteral("用户：%1").arg(username));
        m_userLabel->setObjectName("accentLabel");
        m_loginButton->setText(username);
    }

    style()->unpolish(m_userLabel);
    style()->polish(m_userLabel);
}

void PlaylistPanel::refreshFromModel()
{
    const int selectedRow = m_listWidget->currentRow();

    m_listWidget->clear();
    if (!m_model) {
        return;
    }

    for (int i = 0; i < m_model->count(); ++i) {
        const AudioTrack track = m_model->at(i);
        auto* item = new QListWidgetItem(track.displayText() + statusSuffix(track.playbackStatus));
        const QColor color = statusColor(track.playbackStatus);
        if (color.isValid()) {
            item->setData(Qt::ForegroundRole, QBrush(color));
        }
        if (!track.statusMessage.trimmed().isEmpty()) {
            item->setToolTip(track.statusMessage.trimmed());
        } else if (!track.url.isEmpty()) {
            item->setToolTip(track.url.toString());
        }
        m_listWidget->addItem(item);
    }

    int rowToSelect = m_model->currentIndex();
    if (rowToSelect < 0 && selectedRow >= 0 && selectedRow < m_model->count()) {
        rowToSelect = selectedRow;
    }

    m_listWidget->setCurrentRow(rowToSelect);
}

void PlaylistPanel::showContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = m_listWidget->itemAt(pos);
    if (!item && m_listWidget->count() == 0) {
        return;
    }

    QMenu contextMenu(this);
    AudioStyle::apply(&contextMenu);

    QAction* playAction = nullptr;
    QAction* retryAction = nullptr;
    QAction* deleteAction = nullptr;
    if (item) {
        retryAction = contextMenu.addAction(QStringLiteral("\u91cd\u65b0\u89e3\u6790/\u91cd\u8bd5\u64ad\u653e"));
        playAction = contextMenu.addAction(QStringLiteral("播放"));
        deleteAction = contextMenu.addAction(QStringLiteral("删除"));
        contextMenu.addSeparator();
    }
    QAction* clearAllAction = contextMenu.addAction(QStringLiteral("清空播放列表"));

    QAction* selectedAction = contextMenu.exec(m_listWidget->mapToGlobal(pos));
    if (!selectedAction) {
        return;
    }

    if (selectedAction == playAction) {
        emit songActivated(m_listWidget->row(item));
    } else if (selectedAction == retryAction) {
        m_listWidget->setCurrentItem(item);
        emit retrySelectedRequested();
    } else if (selectedAction == deleteAction) {
        m_listWidget->setCurrentItem(item);
        emit deleteSelectedRequested();
    } else if (selectedAction == clearAllAction) {
        emit clearRequested();
    }
}

void PlaylistPanel::onItemDoubleClicked(QListWidgetItem* item)
{
    emit songActivated(m_listWidget->row(item));
}

QPushButton* PlaylistPanel::createActionButton(const QString& text, const char* role)
{
    auto* button = new QPushButton(text, this);
    AudioStyle::setButtonRole(button, role);
    return button;
}
