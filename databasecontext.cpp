#include "databasecontext.h"

#include "dbmanager.h"

DatabaseContext::DatabaseContext(IDatabaseProvider* provider)
    : m_provider(provider)
{
}

void DatabaseContext::setProvider(IDatabaseProvider* provider)
{
    m_provider = provider;
}

IDatabaseProvider* DatabaseContext::provider() const
{
    return resolveProvider();
}

bool DatabaseContext::initialize() const
{
    IDatabaseProvider* currentProvider = resolveProvider();
    return currentProvider ? currentProvider->initialize() : false;
}

bool DatabaseContext::isInitialized() const
{
    IDatabaseProvider* currentProvider = resolveProvider();
    return currentProvider ? currentProvider->isInitialized() : false;
}

QSqlDatabase DatabaseContext::database() const
{
    IDatabaseProvider* currentProvider = resolveProvider();
    return currentProvider ? currentProvider->database() : QSqlDatabase();
}

QString DatabaseContext::lastError() const
{
    IDatabaseProvider* currentProvider = resolveProvider();
    return currentProvider ? currentProvider->lastError() : QStringLiteral("database provider is not available");
}

IDatabaseProvider* DatabaseContext::resolveProvider() const
{
    return m_provider ? m_provider : &DatabaseManager::instance();
}
