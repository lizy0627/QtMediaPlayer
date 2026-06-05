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
        bool shouldQueueDelivery = false;
        {
            QMutexLocker<QMutex> locker(&m_frameMutex);
            m_pendingFrame = frame;
            if (!m_deliveryPending) {
                m_deliveryPending = true;
                shouldQueueDelivery = true;
            }
        }

        if (shouldQueueDelivery) {
            QMetaObject::invokeMethod(this, [this]() {
                deliverPendingFrame();
            }, Qt::QueuedConnection);
        }
        return;
    }

    storeFrame(frame);
}

void FFmpegVideoWidget::deliverPendingFrame()
{
    QImage frame;
    {
        QMutexLocker<QMutex> locker(&m_frameMutex);
        frame = m_pendingFrame;
        m_pendingFrame = QImage();
        m_deliveryPending = false;
    }

    if (!frame.isNull()) {
        storeFrame(frame);
    }
}

void FFmpegVideoWidget::storeFrame(const QImage& frame)
{
    {
        QMutexLocker<QMutex> locker(&m_frameMutex);
        m_frame = frame;
        if (m_updatePending) {
            return;
        }
        m_updatePending = true;
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
        m_updatePending = false;
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
