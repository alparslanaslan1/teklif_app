#include "teklif/core/transaction.h"

#include <QSqlError>
#include <QSqlQuery>

Transaction::Transaction(QSqlDatabase db) : m_db(std::move(db))
{
    QSqlQuery q(m_db);
    if (q.exec(QStringLiteral("BEGIN IMMEDIATE"))) {
        m_active = true;
    } else {
        // En sık sebep: bu bağlantıda zaten açık bir transaction var.
        // Sessizce devam etmek yerine isActive() false bırakılır ki çağıran
        // taraf işi hiç başlatmasın.
        m_error = q.lastError().text();
    }
}

Transaction::~Transaction()
{
    rollback();
}

bool Transaction::commit(QString *errorOut)
{
    if (!m_active) {
        if (errorOut)
            *errorOut = m_error.isEmpty() ? QStringLiteral("Açık bir işlem yok.") : m_error;
        return false;
    }

    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("COMMIT"))) {
        m_error = q.lastError().text();
        if (errorOut)
            *errorOut = m_error;
        return false; // m_active açık kalır -> yıkıcı ROLLBACK edecek
    }

    m_active = false;
    return true;
}

void Transaction::rollback()
{
    if (!m_active)
        return;
    m_active = false;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("ROLLBACK"));
}
