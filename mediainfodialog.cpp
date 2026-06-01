#include "mediainfodialog.h"

#include "mediaprobeservice.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
QString displayText(const QString& value)
{
    const QString trimmed = value.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("-") : trimmed;
}

QString resolutionText(const MediaInfo& info)
{
    if (info.width <= 0 || info.height <= 0) {
        return QStringLiteral("-");
    }
    return QStringLiteral("%1 x %2").arg(info.width).arg(info.height);
}

QString fpsText(double fps)
{
    if (fps <= 0.0) {
        return QStringLiteral("-");
    }
    return QString::number(fps, 'f', 2);
}

QString bitRateText(qint64 bitRate)
{
    if (bitRate <= 0) {
        return QStringLiteral("-");
    }
    return QStringLiteral("%1 kbps").arg(QString::number(bitRate / 1000.0, 'f', 1));
}

QString durationText(qint64 durationMs)
{
    if (durationMs <= 0) {
        return QStringLiteral("-");
    }

    const qint64 totalSeconds = durationMs / 1000;
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;

    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours, 2, 10, QLatin1Char('0'))
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    }

    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}
}

MediaInfoDialog::MediaInfoDialog(const QString& filePath, QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("MediaInfoDialog"));
    setWindowTitle(QStringLiteral("\u5a92\u4f53\u4fe1\u606f"));
    resize(620, 360);

    const MediaInfo info = MediaProbeService::probeMediaInfo(filePath);

    auto* titleLabel = new QLabel(QStringLiteral("\u5a92\u4f53\u4fe1\u606f"), this);
    titleLabel->setProperty("role", "title");

    auto* grid = new QGridLayout();
    grid->setColumnStretch(0, 0);
    grid->setColumnStretch(1, 1);
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(10);

    int row = 0;
    addInfoRow(grid, row++, QStringLiteral("\u6587\u4ef6\u8def\u5f84"), info.filePath.isEmpty() ? filePath : info.filePath);
    addInfoRow(grid, row++, QStringLiteral("\u5bb9\u5668\u683c\u5f0f"), displayText(info.formatName));
    addInfoRow(grid, row++, QStringLiteral("\u89c6\u9891\u7f16\u7801"), displayText(info.videoCodec));
    addInfoRow(grid, row++, QStringLiteral("\u97f3\u9891\u7f16\u7801"), displayText(info.audioCodec));
    addInfoRow(grid, row++, QStringLiteral("\u5206\u8fa8\u7387"), resolutionText(info));
    addInfoRow(grid, row++, QStringLiteral("FPS"), fpsText(info.fps));
    addInfoRow(grid, row++, QStringLiteral("\u7801\u7387"), bitRateText(info.bitRate));
    addInfoRow(grid, row++, QStringLiteral("\u65f6\u957f"), durationText(info.durationMs));

    auto* statusLabel = new QLabel(this);
    statusLabel->setWordWrap(true);
    statusLabel->setProperty("role", info.valid ? "status" : "warning");
    statusLabel->setText(info.valid
        ? QStringLiteral("\u5df2\u901a\u8fc7 FFmpegProbe \u8bfb\u53d6\u5a92\u4f53\u4fe1\u606f\u3002")
        : displayText(info.errorMessage));

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    if (auto* closeButton = buttonBox->button(QDialogButtonBox::Close)) {
        closeButton->setProperty("role", "secondary");
        closeButton->setText(QStringLiteral("\u5173\u95ed"));
    }
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(16);
    layout->addWidget(titleLabel);
    layout->addLayout(grid);
    layout->addWidget(statusLabel);
    layout->addWidget(buttonBox);
}

void MediaInfoDialog::addInfoRow(QGridLayout* layout, int row, const QString& name, const QString& value)
{
    auto* nameLabel = new QLabel(name, this);
    nameLabel->setProperty("role", "fieldName");
    nameLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto* valueLabel = new QLabel(value, this);
    valueLabel->setProperty("role", "fieldValue");
    valueLabel->setWordWrap(true);
    valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    layout->addWidget(nameLabel, row, 0);
    layout->addWidget(valueLabel, row, 1);
}
