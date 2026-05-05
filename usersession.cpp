#include "usersession.h"

UserSession::UserSession(QObject* parent)
    : QObject(parent)
{
}

SessionState UserSession::state() const
{
    return m_state;
}

bool UserSession::isLoggedIn() const
{
    return m_state.loggedIn;
}

QString UserSession::currentUser() const
{
    return m_state.username;
}

int UserSession::loginCount() const
{
    return m_state.loginCount;
}

void UserSession::setState(const SessionState& state)
{
    SessionState normalizedState = state;
    normalizedState.username = normalizedState.username.trimmed();
    normalizedState.loggedIn = !normalizedState.username.isEmpty();
    if (!normalizedState.loggedIn) {
        normalizedState.loginCount = 0;
    }

    if (m_state.loggedIn == normalizedState.loggedIn
        && m_state.username == normalizedState.username
        && m_state.loginCount == normalizedState.loginCount) {
        return;
    }

    m_state = normalizedState;
    emit sessionChanged(m_state);
}

void UserSession::setCurrentUser(const QString& username)
{
    SessionState nextState = m_state;
    nextState.username = username;
    nextState.loggedIn = !username.trimmed().isEmpty();
    if (!nextState.loggedIn) {
        nextState.loginCount = 0;
    }
    setState(nextState);
}

void UserSession::clear()
{
    setState(SessionState{});
}
