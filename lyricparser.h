#ifndef LYRICPARSER_H
#define LYRICPARSER_H

#include <QList>
#include <QString>

#include "lyricwidget.h"

class LyricParser
{
public:
    static QList<LyricLine> parseLrcText(const QString& lrcText);
    static QList<LyricLine> parseLrcFile(const QString& filePath);
    static QString findLyricFile(const QString& audioFilePath);
    static QList<LyricLine> autoLoadLyrics(const QString& audioFilePath);
    static bool createSampleLyric(const QString& audioFilePath);
};

#endif // LYRICPARSER_H
