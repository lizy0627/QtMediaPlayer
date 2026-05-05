#include "audiostyle.h"

#include <QStyle>
#include <QtGlobal>
#include <QWidget>

namespace {
void refreshStyle(QWidget* widget)
{
    if (!widget) {
        return;
    }

    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}
}

void AudioStyle::apply(QWidget* widget)
{
    if (widget) {
        widget->setProperty("component", "audio");
        refreshStyle(widget);
    }
}

void AudioStyle::setButtonRole(QWidget* widget, const char* role)
{
    if (widget) {
        if (qstrcmp(role, "round") == 0) {
            widget->setProperty("role", "audioRound");
        } else if (qstrcmp(role, "roundMain") == 0) {
            widget->setProperty("role", "audioRoundMain");
        } else {
            widget->setProperty("role", role);
        }
        refreshStyle(widget);
    }
}

void AudioStyle::setModeButton(QWidget* widget)
{
    if (widget) {
        widget->setProperty("component", "audioModeButton");
        refreshStyle(widget);
    }
}
