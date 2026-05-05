#ifndef LYRICWIDGET_H
#define LYRICWIDGET_H

#include <QList>
#include <QString>
#include <QWidget>

class QLabel;
class QPropertyAnimation;

struct LyricLine
{
    qint64 timestamp;
    QString text;

    LyricLine(qint64 time = 0, const QString& lyric = QString());
};

enum class LyricDisplayState {
    NoLyric,
    DownloadFailed,
    ParseFailed,
    SavePermissionDenied
};

Q_DECLARE_METATYPE(LyricDisplayState)

class LyricWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LyricWidget(QWidget* parent = nullptr);

    void setLyrics(const QList<LyricLine>& lyrics);
    void showStatus(LyricDisplayState state, const QString& detail = QString());
    void clear();
    void updatePosition(qint64 position);

private:
    void setupUI();
    int findCurrentLine(qint64 position);
    void updateDisplay();
    void animateCurrentLine();
    void showNoLyric();
    void hideNoLyric();
    QString statusTitle(LyricDisplayState state) const;

    QList<LyricLine> m_lyrics;
    int m_currentLineIndex;
    qint64 m_currentPosition;

    QLabel* m_prevLine;
    QLabel* m_currentLine;
    QLabel* m_nextLine;
    QLabel* m_statusTitleLabel;
    QLabel* m_statusDetailLabel;
    QWidget* m_lyricContainer;

    QPropertyAnimation* m_fadeAnimation;
};

#endif // LYRICWIDGET_H
