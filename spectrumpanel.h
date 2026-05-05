#ifndef SPECTRUMPANEL_H
#define SPECTRUMPANEL_H

#include <QGroupBox>

class QMediaPlayer;
class SpectrumWidget;

class SpectrumPanel : public QGroupBox
{
    Q_OBJECT

public:
    explicit SpectrumPanel(QWidget* parent = nullptr);

    void setMediaPlayer(QMediaPlayer* player);
    void setPlaying(bool playing);

private:
    SpectrumWidget* m_spectrumWidget = nullptr;
};

#endif // SPECTRUMPANEL_H
