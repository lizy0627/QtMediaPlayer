#include "appbootstrapper.h"

#include "dbmanager.h"

#include <QCoreApplication>
#include <QDebug>

AppBootstrapper::AppBootstrapper(QCoreApplication* app)
    : QObject(app)
    , m_theme(app)
{
}

AppStartupState AppBootstrapper::initialize()
{
    QCoreApplication::setOrganizationName(QStringLiteral("QtMediaPlayer"));
    QCoreApplication::setApplicationName(QStringLiteral("QtMediaPlayer"));

    m_state = AppStartupState();
    m_state.themeLoaded = m_theme.loadTheme(QStringLiteral("app"));
    if (!m_state.themeLoaded) {
        m_state.themeError = QStringLiteral("UI theme failed to load; continuing with default widget styles.");
        qWarning() << m_state.themeError;
    }

    m_state.databaseAvailable = DatabaseManager::instance().initialize();
    if (!m_state.databaseAvailable) {
        m_state.databaseError = DatabaseManager::instance().lastError();
        qWarning() << "Database initialization failed:" << m_state.databaseError;
    }

    return m_state;
}

const AppStartupState& AppBootstrapper::startupState() const
{
    return m_state;
}
