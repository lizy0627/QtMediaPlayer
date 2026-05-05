#ifndef AUTHDIALOGCONTROLLER_H
#define AUTHDIALOGCONTROLLER_H

#include <QObject>

class AuthService;
class QPoint;
class QWidget;

class AuthDialogController : public QObject
{
    Q_OBJECT

public:
    explicit AuthDialogController(AuthService* authService, QObject* parent = nullptr);

    AuthService* authService() const;

public slots:
    void showLoginDialog(QWidget* parent);
    void showUserMenu(QWidget* parent, const QPoint& pos);
    void showChangePasswordDialog(QWidget* parent);

private:
    AuthService* m_authService = nullptr;
};

#endif // AUTHDIALOGCONTROLLER_H
