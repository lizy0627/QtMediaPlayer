#include "menu.h"

#include <QAction>
#include <QActionGroup>
#include <QIcon>
#include <QMenu>
#include <QToolBar>

Menu::Menu(QMenu* menu, QToolBar* toolBar)
    : QObject(menu)
    , m_menu(menu)
    , m_toolBar(toolBar)
    , m_actGroup(new QActionGroup(this))
{
}

bool Menu::createActionGroup(const QList<std::tuple<QString, QString, std::function<void()>>>& actions,
                             bool isChecked)
{
    if (actions.isEmpty()) {
        return false;
    }

    for (const auto& action : actions) {
        const auto& [name, icon, func] = action;
        QAction* act = createAction(name, icon, func);
        m_actGroup->addAction(act);
        if (isChecked) {
            act->setCheckable(true);
        }
    }

    return true;
}

QAction* Menu::createAction(const QString& name, const QString& imgPath, std::function<void()> func)
{
    QAction* act = new QAction(QIcon(imgPath), name);
    m_menu->addAction(act);

    if (m_toolBar) {
        m_toolBar->addAction(act);
    }

    connect(act, &QAction::triggered, this, [func]() {
        func();
    });

    return act;
}
