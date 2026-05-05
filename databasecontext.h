#ifndef DATABASECONTEXT_H
#define DATABASECONTEXT_H

#include <QSqlDatabase>
#include <QString>

class IDatabaseProvider
{
public:
    virtual ~IDatabaseProvider() = default;

    virtual bool initialize() = 0;
    virtual bool isInitialized() const = 0;
    virtual QSqlDatabase database() const = 0;
    virtual QString lastError() const = 0;
};

class DatabaseContext
{
public:
    explicit DatabaseContext(IDatabaseProvider* provider = nullptr);

    void setProvider(IDatabaseProvider* provider);
    IDatabaseProvider* provider() const;

    bool initialize() const;
    bool isInitialized() const;
    QSqlDatabase database() const;
    QString lastError() const;

private:
    IDatabaseProvider* resolveProvider() const;

    IDatabaseProvider* m_provider = nullptr;
};

#endif // DATABASECONTEXT_H
