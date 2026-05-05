#ifndef FRAMECAPTURESERVICE_H
#define FRAMECAPTURESERVICE_H

#include <QObject>
#include <QPixmap>

class QWidget;

class FrameCaptureService : public QObject
{
    Q_OBJECT

public:
    explicit FrameCaptureService(QWidget* sourceWidget = nullptr, QObject* parent = nullptr);

    void setSourceWidget(QWidget* widget);
    QWidget* sourceWidget() const;

    QPixmap captureCurrentFrame() const;
    static QPixmap captureWidgetFrame(QWidget* widget);

private:
    QWidget* m_sourceWidget = nullptr;
};

#endif // FRAMECAPTURESERVICE_H
