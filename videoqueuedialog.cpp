#include "videoqueuedialog.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

#include "videoplayercontroller.h"

namespace {
constexpr int kQueueIndexRole = Qt::UserRole + 1;
}

VideoQueueDialog::VideoQueueDialog(VideoPlayerController* controller, QWidget* parent)
    : QDialog(parent)
    , m_controller(controller)
{
    setWindowTitle(QStringLiteral("Video Queue"));
    resize(460, 360);

    m_listWidget = new QListWidget(this);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setAlternatingRowColors(true);

    m_emptyLabel = new QLabel(QStringLiteral("No videos in queue."), this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setVisible(false);

    m_removeButton = new QPushButton(QStringLiteral("Remove"), this);
    m_removeButton->setEnabled(false);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);

    QHBoxLayout* actionLayout = new QHBoxLayout();
    actionLayout->addWidget(m_removeButton);
    actionLayout->addStretch();
    actionLayout->addWidget(buttonBox);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_listWidget);
    layout->addWidget(m_emptyLabel);
    layout->addLayout(actionLayout);

    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &VideoQueueDialog::playSelectedItem);
    connect(m_listWidget, &QListWidget::itemSelectionChanged, this, &VideoQueueDialog::updateActions);
    connect(m_removeButton, &QPushButton::clicked, this, &VideoQueueDialog::removeSelectedItem);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &VideoQueueDialog::hide);

    if (m_controller) {
        connect(m_controller,
                &VideoPlayerController::videoQueueChanged,
                this,
                &VideoQueueDialog::refreshQueue);
        refreshQueue(m_controller->videoQueue(), m_controller->currentVideoQueueIndex());
    }
}

void VideoQueueDialog::refreshQueue(const QStringList& filePaths, int currentIndex)
{
    const int previousSelection = selectedQueueIndex();

    m_listWidget->clear();
    for (int i = 0; i < filePaths.size(); ++i) {
        const QFileInfo fileInfo(filePaths.at(i));
        QString title = fileInfo.fileName();
        if (title.isEmpty()) {
            title = filePaths.at(i);
        }

        QListWidgetItem* item = new QListWidgetItem(title);
        item->setToolTip(filePaths.at(i));
        item->setData(kQueueIndexRole, i);
        m_listWidget->addItem(item);
    }

    markCurrentItem(currentIndex);

    const int selectionToRestore =
        previousSelection >= 0 && previousSelection < m_listWidget->count()
            ? previousSelection
            : currentIndex;
    if (selectionToRestore >= 0 && selectionToRestore < m_listWidget->count()) {
        m_listWidget->setCurrentRow(selectionToRestore);
    }

    const bool empty = filePaths.isEmpty();
    m_listWidget->setVisible(!empty);
    m_emptyLabel->setVisible(empty);
    updateActions();
}

void VideoQueueDialog::playSelectedItem(QListWidgetItem* item)
{
    if (!m_controller || !item) {
        return;
    }

    m_controller->playQueuedVideo(item->data(kQueueIndexRole).toInt());
}

void VideoQueueDialog::removeSelectedItem()
{
    if (!m_controller) {
        return;
    }

    const int index = selectedQueueIndex();
    if (index < 0) {
        return;
    }

    m_controller->removeQueuedVideo(index);
}

void VideoQueueDialog::updateActions()
{
    m_removeButton->setEnabled(selectedQueueIndex() >= 0);
}

int VideoQueueDialog::selectedQueueIndex() const
{
    QListWidgetItem* item = m_listWidget ? m_listWidget->currentItem() : nullptr;
    return item ? item->data(kQueueIndexRole).toInt() : -1;
}

void VideoQueueDialog::markCurrentItem(int currentIndex)
{
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem* item = m_listWidget->item(i);
        const bool current = i == currentIndex;
        QFont font = item->font();
        font.setBold(current);
        item->setFont(font);
        item->setText(current ? QStringLiteral("Now: %1").arg(item->text()) : item->text());
    }
}
