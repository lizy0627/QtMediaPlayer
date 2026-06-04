#include "ffmpegvideowidget.h"

#include <QMetaObject>
#include <QMutexLocker>
#include <QPainter>
#include <QPaintEvent>
#include <QThread>

FFmpegVideoWidget::FFmpegVideoWidget(QWidget* parent)
    : QWidget(parent)
{
    setAutoFillBackground(false);
}

void FFmpegVideoWidget::setFrame(const QImage& frame)
{
    if (QThread::currentThread() != thread()) {
        const QImage copiedFrame = frame.copy();
        QMetaObject::invokeMethod(this, [this, copiedFrame]() {
            setFrame(copiedFrame);
        }, Qt::QueuedConnection);
        return;
    }

    {
        QMutexLocker<QMutex> locker(&m_frameMutex);
        m_frame = frame.copy();
    }

    update();
}

void FFmpegVideoWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    QImage frame;
    {
        QMutexLocker<QMutex> locker(&m_frameMutex);
        frame = m_frame;
    }

    if (frame.isNull()) {
        return;
    }

    QSize scaledSize = frame.size();
    scaledSize.scale(size(), Qt::KeepAspectRatio);

    const QRect targetRect(QPoint((width() - scaledSize.width()) / 2,
                                  (height() - scaledSize.height()) / 2),
                           scaledSize);
    painter.drawImage(targetRect, frame);
}
