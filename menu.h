#ifndef MENU_H
#define MENU_H

#include <QList>
#include <QObject>
#include <QString>
#include <functional>
#include <tuple>

class QAction;
class QActionGroup;
class QMenu;
class QToolBar;

class Menu : public QObject
{
    Q_OBJECT

public:
    explicit Menu(QMenu* menu, QToolBar* toolBar = nullptr);

    bool createActionGroup(const QList<std::tuple<QString, QString, std::function<void()>>>& actions,
                           bool isChecked = false);
    QAction* createAction(const QString& name, const QString& imgPath, std::function<void()> func);

private:
    QMenu* m_menu = nullptr;
    QToolBar* m_toolBar = nullptr;
    QActionGroup* m_actGroup = nullptr;
};

#endif // MENU_H
