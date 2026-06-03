#ifndef FRAMECAPTURESERVICE_H
#define FRAMECAPTURESERVICE_H

#include <QImage>
#include <QObject>
#include <QPixmap>
#include <QString>

class QWidget;

class FrameCaptureService : public QObject
{
    Q_OBJECT

public:
    explicit FrameCaptureService(QWidget* sourceWidget = nullptr, QObject* parent = nullptr);

    void setSourceWidget(QWidget* widget);
    QWidget* sourceWidget() const;

    QPixmap captureCurrentFrame() const;
    QImage captureVideoFrame(const QString& filePath, qint64 positionMs);
    QString lastError() const;
    static QPixmap captureWidgetFrame(QWidget* widget);
    static QImage captureVideoFrame(const QString& filePath,
                                    qint64 positionMs,
                                    QString* errorMessage);

private:
    QWidget* m_sourceWidget = nullptr;
    QString m_lastError;
};

#endif // FRAMECAPTURESERVICE_H
