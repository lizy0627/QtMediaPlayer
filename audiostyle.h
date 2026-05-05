#ifndef AUDIOSTYLE_H
#define AUDIOSTYLE_H

class QWidget;

class AudioStyle
{
public:
    static void apply(QWidget* widget);
    static void setButtonRole(QWidget* widget, const char* role);
    static void setModeButton(QWidget* widget);
};

#endif // AUDIOSTYLE_H
