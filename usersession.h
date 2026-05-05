#ifndef USERSESSION_H
#define USERSESSION_H

#include <QMetaType>
#include <QObject>
#include <QString>

struct SessionState
{
    bool loggedIn = false;
    QString username;
    int loginCount = 0;
};

Q_DECLARE_METATYPE(SessionState)

class UserSession : public QObject
{
    Q_OBJECT

public:
    explicit UserSession(QObject* parent = nullptr);

    SessionState state() const;
    bool isLoggedIn() const;
    QString currentUser() const;
    int loginCount() const;

public slots:
    void setState(const SessionState& state);
    void setCurrentUser(const QString& username);
    void clear();

signals:
    void sessionChanged(const SessionState& state);

private:
    SessionState m_state;
};

#endif // USERSESSION_H
