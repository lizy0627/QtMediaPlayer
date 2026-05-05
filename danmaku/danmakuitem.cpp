#include "danmakuitem.h"

#include <utility>

DanmakuItem::DanmakuItem(QString text, int time)
    : content(std::move(text))
    , timestamp(time)
{
}

QString DanmakuItem::text() const
{
    return content;
}

int DanmakuItem::time() const
{
    return static_cast<int>(timestamp);
}
