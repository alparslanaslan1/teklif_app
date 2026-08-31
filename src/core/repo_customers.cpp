#include "repo_customers.h"

#include "turkish.h"

#include <QSqlError>
#include <QSqlQuery>

#include <algorithm>
#include <utility>

namespace {


// Açık bir sorgunun MEVCUT satırını Customer nesnesine doldurur (çağrılmadan
// önce q.next() true dönmüş olmalıdır). Sütunlara adıyla erişilir.
// C++ tarafı camelCase kullanır: vergi_dairesi -> vergiDairesi, vergi_no -> vergiNo.
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

RepoCustomers::RepoCustomers(QSqlDatabase db) : m_db(std::move(db)) {}


// Yeni müşteri ekler. Başarılıysa customer.id, veritabanının verdiği id ile
// doldurulur — parametre bu yüzden referanstır.
// olusturma sütunu INSERT'e konmaz; şemadaki DEFAULT (datetime('now')) devreye girer.
bool RepoCustomers::add(Customer &customer, QString *errorOut)
{
    QSqlQuery q(m_db);
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


// Müşterileri unvana göre alfabetik sıralı döner; teklif ekranındaki müşteri
// açılır listesini besler.
//   includeInactive : false ise (varsayılan) pasif müşteriler listeye girmez
QVector<Customer> RepoCustomers::listAll(bool includeInactive, QString *errorOut) const
{
    QVector<Customer> sonuc;
    QSqlQuery q(m_db);

    // ORDER BY YOK — sıralama Türkçe alfabeye göre C++ tarafında yapılır
    // (bkz. RepoItems::listAll üzerindeki not).
    const QString sql = includeInactive
                             ? QStringLiteral("SELECT * FROM customers")
                             : QStringLiteral("SELECT * FROM customers WHERE aktif=1");
    if (!q.exec(sql)) {
        if (errorOut)
            *errorOut = q.lastError().text();
        return sonuc;
    }

    while (q.next())
        sonuc.append(customerFromQuery(q));

    std::sort(sonuc.begin(), sonuc.end(),
              turkishLessBy<Customer>([](const Customer &c) { return c.unvan; }));
    return sonuc;
}

bool RepoCustomers::update(const Customer &customer, QString *errorOut)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE customers SET unvan=:unvan, yetkili=:yetkili, telefon=:telefon, email=:email, "
        "adres=:adres, vergi_dairesi=:vd, vergi_no=:vn, notlar=:notlar, aktif=:aktif "
        "WHERE id=:id"));
    q.bindValue(QStringLiteral(":unvan"), customer.unvan);
    q.bindValue(QStringLiteral(":yetkili"), customer.yetkili);
    q.bindValue(QStringLiteral(":telefon"), customer.telefon);
    q.bindValue(QStringLiteral(":email"), customer.email);
    q.bindValue(QStringLiteral(":adres"), customer.adres);
    q.bindValue(QStringLiteral(":vd"), customer.vergiDairesi);
    q.bindValue(QStringLiteral(":vn"), customer.vergiNo);
    q.bindValue(QStringLiteral(":notlar"), customer.notlar);
    q.bindValue(QStringLiteral(":aktif"), customer.aktif ? 1 : 0);
    q.bindValue(QStringLiteral(":id"), customer.id);

    if (!q.exec()) {
        if (errorOut)
            *errorOut = q.lastError().text();
        return false;
    }
    if (q.numRowsAffected() == 0) {
        if (errorOut)
            *errorOut = QStringLiteral("Güncellenecek müşteri bulunamadı (id %1).").arg(customer.id);
        return false;
    }
    return true;
}

bool RepoCustomers::setActive(qint64 id, bool aktif, QString *errorOut)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE customers SET aktif=:aktif WHERE id=:id"));
    q.bindValue(QStringLiteral(":aktif"), aktif ? 1 : 0);
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec()) {
        if (errorOut)
            *errorOut = q.lastError().text();
        return false;
    }
    if (q.numRowsAffected() == 0) {
        if (errorOut)
            *errorOut = QStringLiteral("Müşteri bulunamadı (id %1).").arg(id);
        return false;
    }
    return true;
}

std::optional<Customer> RepoCustomers::get(qint64 id, QString *errorOut) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM customers WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec()) {
        if (errorOut)
            *errorOut = q.lastError().text();
        return std::nullopt;
    }
    if (!q.next()) {
        if (errorOut)
            *errorOut = QStringLiteral("Müşteri bulunamadı (id %1).").arg(id);
        return std::nullopt;
    }
    return customerFromQuery(q);
}

QVector<Customer> RepoCustomers::search(const QString &aranan, bool includeInactive,
                                         QString *errorOut) const
{
    const QVector<Customer> hepsi = listAll(includeInactive, errorOut);

    const QString anahtar = turkishSearchNormalize(aranan.trimmed());
    if (anahtar.isEmpty())
        return hepsi;

    QVector<Customer> sonuc;
    for (const Customer &c : hepsi) {
        // Unvan, yetkili, telefon ve vergi no birlikte taranır: kullanıcı
        // müşteriyi bazen firma adıyla, bazen yetkilinin adıyla hatırlar.
        const QString alan = turkishSearchNormalize(c.unvan + QLatin1Char(' ') + c.yetkili
                                                     + QLatin1Char(' ') + c.telefon
                                                     + QLatin1Char(' ') + c.vergiNo);
        if (alan.contains(anahtar))
            sonuc.append(c);
    }
    return sonuc;
}

int RepoCustomers::quoteCount(qint64 customerId, QString *errorOut) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM quotes WHERE customer_id = :id"));
    q.bindValue(QStringLiteral(":id"), customerId);

    if (!q.exec() || !q.next()) {
        if (errorOut)
            *errorOut = q.lastError().text();
        return -1;
    }
    return q.value(0).toInt();
}
