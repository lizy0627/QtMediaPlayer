#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QString>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget* parent = nullptr);

    QString username() const;
    QString password() const;
    void setStatus(const QString& message, bool success = false);
    void clearPassword();

signals:
    void loginRequested(const QString& username, const QString& password);
    void registerRequested(const QString& username, const QString& password);

private slots:
    void onLoginClicked();
    void onRegisterClicked();

private:
    void createUI();
    void connectSignals();
    bool validateBasicInput();
    void loadRememberedUsername();
    void persistRememberedUsername();

    QLineEdit* m_usernameEdit = nullptr;
    QLineEdit* m_passwordEdit = nullptr;
    QPushButton* m_loginButton = nullptr;
    QPushButton* m_registerButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
    QCheckBox* m_rememberCheck = nullptr;
    QLabel* m_statusLabel = nullptr;
};

#endif // LOGINDIALOG_H
