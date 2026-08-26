#include "repo_customers.h"

#include <QSqlError>
#include <QSqlQuery>

namespace {

Customer customerFromQuery(const QSqlQuery &q)
{
    Customer c;
    c.id = q.value(QStringLiteral("id")).toLongLong();
    c.unvan = q.value(QStringLiteral("unvan")).toString();
    c.yetkili = q.value(QStringLiteral("yetkili")).toString();
    c.telefon = q.value(QStringLiteral("telefon")).toString();
    c.email = q.value(QStringLiteral("email")).toString();
    c.adres = q.value(QStringLiteral("adres")).toString();
    c.vergiDairesi = q.value(QStringLiteral("vergi_dairesi")).toString();
    c.vergiNo = q.value(QStringLiteral("vergi_no")).toString();
    c.notlar = q.value(QStringLiteral("notlar")).toString();
    c.aktif = q.value(QStringLiteral("aktif")).toInt() != 0;
    return c;
}

} // namespace

bool RepoCustomers::add(QSqlDatabase &db, Customer &customer, QString *errorOut)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO customers (unvan, yetkili, telefon, email, adres, vergi_dairesi, vergi_no, notlar, aktif) "
        "VALUES (:unvan, :yetkili, :telefon, :email, :adres, :vd, :vn, :notlar, :aktif)"));
    q.bindValue(QStringLiteral(":unvan"), customer.unvan);
    q.bindValue(QStringLiteral(":yetkili"), customer.yetkili);
    q.bindValue(QStringLiteral(":telefon"), customer.telefon);
    q.bindValue(QStringLiteral(":email"), customer.email);
    q.bindValue(QStringLiteral(":adres"), customer.adres);
    q.bindValue(QStringLiteral(":vd"), customer.vergiDairesi);
    q.bindValue(QStringLiteral(":vn"), customer.vergiNo);
    q.bindValue(QStringLiteral(":notlar"), customer.notlar);
    q.bindValue(QStringLiteral(":aktif"), customer.aktif ? 1 : 0);

    if (!q.exec()) {
        if (errorOut)
            *errorOut = q.lastError().text();
        return false;
    }

    customer.id = q.lastInsertId().toLongLong();
    return true;
}

QVector<Customer> RepoCustomers::listAll(QSqlDatabase &db, bool includeInactive)
{
    QVector<Customer> sonuc;
    QSqlQuery q(db);
    const QString sql = includeInactive
                             ? QStringLiteral("SELECT * FROM customers ORDER BY unvan")
                             : QStringLiteral("SELECT * FROM customers WHERE aktif=1 ORDER BY unvan");
    if (!q.exec(sql))
        return sonuc;

    while (q.next())
        sonuc.append(customerFromQuery(q));
    return sonuc;
}
