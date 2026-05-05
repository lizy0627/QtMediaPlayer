#include "mydanmakudialog.h"

#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

MyDanmakuDialog::MyDanmakuDialog(const QString& username,
                                 const QString& currentVideoPath,
                                 const QVector<DanmakuItem>& records,
                                 QWidget* parent)
    : QDialog(parent)
    , m_username(username)
    , m_currentVideoPath(currentVideoPath)
    , m_records(records)
{
    setWindowTitle(QString("%1 的弹幕记录").arg(m_username));
    setMinimumSize(820, 560);
    setStyleSheet(
        "QDialog { background-color: #1f2937; }"
        "QLabel { color: #f9fafb; font-family: 'Microsoft YaHei'; }"
        "QListWidget { background-color: #111827; color: #f9fafb; border: 1px solid #374151; border-radius: 8px; padding: 6px; }"
        "QListWidget::item { padding: 10px; border-bottom: 1px solid #1f2937; }"
        "QListWidget::item:selected { background-color: rgba(102, 126, 234, 0.35); }"
        "QPushButton { background-color: #667eea; color: white; border: none; border-radius: 6px; padding: 8px 16px; font-family: 'Microsoft YaHei'; }"
        "QPushButton:hover { background-color: #7c8ff0; }"
        "QPushButton:disabled { background-color: #4b5563; color: #9ca3af; }"
        "QCheckBox { color: #e5e7eb; spacing: 8px; }"
    );

    QVBoxLayout* layout = new QVBoxLayout(this);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setStyleSheet("font-size: 16pt; font-weight: bold; color: #93c5fd;");
    layout->addWidget(m_titleLabel);

    m_currentVideoOnlyCheck = new QCheckBox("只看当前视频", this);
    m_currentVideoOnlyCheck->setChecked(!m_currentVideoPath.isEmpty());
    m_currentVideoOnlyCheck->setEnabled(!m_currentVideoPath.isEmpty());
    if (m_currentVideoPath.isEmpty()) {
        m_currentVideoOnlyCheck->setToolTip("当前未打开本地视频，无法按当前视频筛选");
    }
    layout->addWidget(m_currentVideoOnlyCheck);

    m_listWidget = new QListWidget(this);
    layout->addWidget(m_listWidget, 1);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* locateButton = new QPushButton("定位到该视频", this);
    QPushButton* closeButton = new QPushButton("关闭", this);
    buttonLayout->addWidget(locateButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);

    connect(m_currentVideoOnlyCheck, &QCheckBox::toggled, this, &MyDanmakuDialog::renderRecords);
    connect(locateButton, &QPushButton::clicked, this, &MyDanmakuDialog::emitLocateForCurrentItem);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        emitLocateForCurrentItem();
    });

    renderRecords();
}

void MyDanmakuDialog::renderRecords()
{
    m_listWidget->clear();

    QVector<DanmakuItem> visibleRecords;
    for (const DanmakuItem& itemData : m_records) {
        if (m_currentVideoOnlyCheck->isChecked() && itemData.videoPath != m_currentVideoPath) {
            continue;
        }
        visibleRecords.append(itemData);
    }

    m_titleLabel->setText(m_currentVideoOnlyCheck->isChecked()
        ? QString("当前视频弹幕记录（共 %1 条）").arg(visibleRecords.size())
        : QString("我的弹幕记录（共 %1 条）").arg(visibleRecords.size()));

    if (visibleRecords.isEmpty()) {
        QListWidgetItem* emptyItem = new QListWidgetItem(
            m_currentVideoOnlyCheck->isChecked() ? "当前视频暂无你的弹幕" : "暂无弹幕记录");
        emptyItem->setFlags(Qt::NoItemFlags);
        emptyItem->setTextAlignment(Qt::AlignCenter);
        m_listWidget->addItem(emptyItem);
        return;
    }

    for (const DanmakuItem& itemData : visibleRecords) {
        const QString fileName = QFileInfo(itemData.videoPath).fileName().isEmpty()
            ? itemData.videoPath
            : QFileInfo(itemData.videoPath).fileName();
        const QString text = QString("[%1] %2\n视频：%3\n类型：%4  ·  发送时间：%5")
            .arg(formatTime(itemData.timestamp))
            .arg(itemData.content)
            .arg(fileName)
            .arg(typeText(itemData.type))
            .arg(itemData.createTime.isValid() ? itemData.createTime.toString("yyyy-MM-dd hh:mm:ss") : "未知");

        QListWidgetItem* item = new QListWidgetItem(text);
        item->setForeground(QBrush(QColor(itemData.color)));
        item->setData(Qt::UserRole, itemData.videoPath);
        item->setData(Qt::UserRole + 1, itemData.timestamp);
        m_listWidget->addItem(item);
    }
}

void MyDanmakuDialog::emitLocateForCurrentItem()
{
    QListWidgetItem* currentItem = m_listWidget->currentItem();
    if (!currentItem || currentItem->flags() == Qt::NoItemFlags) {
        return;
    }

    const QString videoPath = currentItem->data(Qt::UserRole).toString();
    const qint64 timestamp = currentItem->data(Qt::UserRole + 1).toLongLong();
    if (videoPath.isEmpty()) {
        return;
    }

    emit locateRequested(videoPath, timestamp);
    accept();
}

QString MyDanmakuDialog::formatTime(qint64 milliseconds) const
{
    return QString("%1:%2")
        .arg(milliseconds / 60000, 2, 10, QLatin1Char('0'))
        .arg((milliseconds / 1000) % 60, 2, 10, QLatin1Char('0'));
}

QString MyDanmakuDialog::typeText(int type) const
{
    if (type == 1) {
        return "顶部";
    }
    if (type == 2) {
        return "底部";
    }
    return "滚动";
}
