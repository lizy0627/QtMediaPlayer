#ifndef FFMPEGVIDEOWIDGET_H
#define FFMPEGVIDEOWIDGET_H

#include <QImage>
#include <QMutex>
#include <QWidget>

class FFmpegVideoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FFmpegVideoWidget(QWidget* parent = nullptr);

public slots:
    void setFrame(const QImage& frame);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    mutable QMutex m_frameMutex;
    QImage m_frame;
};

#endif // FFMPEGVIDEOWIDGET_H
