#ifndef MENU_H
#define MENU_H

#include <QObject>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QAction>
#include <QActionGroup>
#include <QIcon>
#include <QString>
#include <QList>
#include <QDebug>
#include <functional>
#include <tuple>

// 菜单类
class Menu : public QObject
{
    Q_OBJECT
private:
    QMenu *m_menu = nullptr;            // 菜单栏
    QToolBar *m_toolBar = nullptr;      // 工具栏
    QActionGroup *m_actGroup = nullptr; // 按钮组

public:
    explicit Menu(QMenu* menu, QToolBar* toolBar = nullptr) 
        : QObject(nullptr), m_menu(menu), m_toolBar(toolBar)
    {
        m_actGroup = new QActionGroup(this);
    }

    ///
    /// \brief createActionGroup 创建菜单按钮组
    /// \param actions 按钮组
    /// \param isChecked 是否可选中
    /// \return 结果
    ///
    bool createActionGroup(const QList<std::tuple<QString, QString, std::function<void()>>>& actions, bool isChecked = false)
    {
        if(actions.isEmpty())
            return false;

        for (const auto& action : actions)
        {
            const auto& [name, icon, func] = action;
            QAction* act = createAction(name, icon, func);
            m_actGroup->addAction(act);
            if(isChecked)
                act->setCheckable(true);
        }
        return true;
    }

    ///
    /// \brief createAction 创建菜单项
    /// \param name 菜单项名称
    /// \param imgPath 图标
    /// \param func 回调函数
    /// \return 菜单项指针
    ///
    QAction* createAction(const QString& name, const QString& imgPath, std::function<void()> func)
    {
        QAction *act = new QAction(QIcon(imgPath), name);
        m_menu->addAction(act);

        // 添加到工具栏
        if(m_toolBar)
        {
            m_toolBar->addAction(act);
        }
        connect(act, &QAction::triggered, this, [=](){ func(); });
        return act;
    }
};

#endif // MENU_H
