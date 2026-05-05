#ifndef AUDIOPLAYERWIDGET_H
#define AUDIOPLAYERWIDGET_H

#include <QPixmap>
#include <QSize>
#include <QWidget>

class AudioControlBar;
class QLabel;
class LyricPanel;
class PlaylistModel;
class PlaylistPanel;
class QPushButton;
class SpectrumPanel;

class AudioPlayerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AudioPlayerWidget(QWidget* parent = nullptr);

    PlaylistPanel* playlistPanel() const;
    AudioControlBar* controlBar() const;
    LyricPanel* lyricPanel() const;
    SpectrumPanel* spectrumPanel() const;
    QSize albumArtSize() const;
    void setPlaylistModel(PlaylistModel* model);
    void setAlbumArtPixmap(const QPixmap& pixmap);
    void setAlbumArtToolTip(const QString& text);

signals:
    void albumArtChangeRequested();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QLabel* m_albumArt = nullptr;
    PlaylistPanel* m_playlistPanel = nullptr;
    AudioControlBar* m_controlBar = nullptr;
    LyricPanel* m_lyricPanel = nullptr;
    SpectrumPanel* m_spectrumPanel = nullptr;
};

#endif // AUDIOPLAYERWIDGET_H
