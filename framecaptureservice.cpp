#include "framecaptureservice.h"

#include "ffmpeg/ffmpegframeextractor.h"

#include <QGuiApplication>
#include <QPoint>
#include <QRect>
#include <QScreen>
#include <QWidget>

FrameCaptureService::FrameCaptureService(QWidget* sourceWidget, QObject* parent)
    : QObject(parent)
    , m_sourceWidget(sourceWidget)
{
}

void FrameCaptureService::setSourceWidget(QWidget* widget)
{
    m_sourceWidget = widget;
}

QWidget* FrameCaptureService::sourceWidget() const
{
    return m_sourceWidget;
}

QPixmap FrameCaptureService::captureCurrentFrame() const
{
    return captureWidgetFrame(m_sourceWidget);
}

QImage FrameCaptureService::captureVideoFrame(const QString& filePath, qint64 positionMs)
{
    m_lastError.clear();
    QImage image = captureVideoFrame(filePath, positionMs, &m_lastError);
    if (!image.isNull()) {
        m_lastError.clear();
    }
    return image;
}

QString FrameCaptureService::lastError() const
{
    return m_lastError;
}

QPixmap FrameCaptureService::captureWidgetFrame(QWidget* widget)
{
    if (!widget) {
        return QPixmap();
    }

    const QPoint globalPosition = widget->mapToGlobal(QPoint(0, 0));
    const QRect captureRect(globalPosition, widget->size());

    QScreen* screen = widget->screen();
    if (!screen) {
        screen = QGuiApplication::screenAt(captureRect.center());
    }
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }

    QPixmap pixmap;
    if (screen) {
        pixmap = screen->grabWindow(0,
                                    captureRect.x(),
                                    captureRect.y(),
                                    captureRect.width(),
                                    captureRect.height());
    }

    if (pixmap.isNull() || pixmap.width() == 0 || pixmap.height() == 0) {
        pixmap = widget->grab();
    }

    return pixmap;
}

QImage FrameCaptureService::captureVideoFrame(const QString& filePath,
                                              qint64 positionMs,
                                              QString* errorMessage)
{
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }

    FFmpegFrameExtractor extractor;
    QImage image = extractor.extractFrame(filePath, positionMs);
    if (image.isNull() && errorMessage != nullptr) {
        *errorMessage = extractor.lastError();
    }
    return image;
}
