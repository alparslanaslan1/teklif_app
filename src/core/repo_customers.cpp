#include "repo_customers.h"

#include <QSqlError>
#include <QSqlQuery>

namespace {


// ═══ customerFromQuery() ══════════════════════════════════════════════════
// NE YAPAR : Açık sorgunun MEVCUT satırını Customer nesnesine doldurur.
//            Çağrılmadan önce q.next() true dönmüş olmalıdır.
//
// ADIM ADIM: Sütunlara ADIYLA erişilir. Alan adları DB sütunlarıyla birebir
//            eşleşmez, C++ tarafı camelCase kullanır:
//              vergi_dairesi -> vergiDairesi
//              vergi_no      -> vergiNo
//            aktif INTEGER'dan != 0 ile bool'a çevrilir.
//
// DEBUG    : Bir alan hep boş geliyorsa neredeyse her zaman sütun adı yanlış
//            yazılmıştır (Qt uyarı basar ama program çökmez):
//              qDebug() << q.record().fieldNames();
//
// EKSİK    : `olusturma` sütunu DB'de var ama Customer modelinde YOK, bu
//            yüzden okunmuyor. İhtiyaç olursa modele eklenmeli.
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


// ═══ RepoCustomers::add() ═════════════════════════════════════════════════
// NE YAPAR : Yeni müşteri ekler ve customer.id'yi DB'nin verdiği id ile
//            doldurur (bu yüzden parametre referanstır).
//
// ADIM ADIM:
//   1) INSERT hazırlanır. `olusturma` sütunu yazılmaz — DB'deki
//      DEFAULT (datetime('now')) devreye girer.
//   2) Tüm alanlar bind edilir; aktif bool -> 1/0.
//   3) exec() başarısızsa ham hata metni döner.
//      (RepoItems::add'deki gibi ÖZEL bir "zaten kayıtlı" mesajı YOK, çünkü
//       customers tablosunda UNIQUE kısıt yoktur — aynı unvanla iki müşteri
//       eklenebilir. Mükerrer kayıt uyarısı istiyorsanız burada kontrol
//       etmeniz gerekir.)
//   4) customer.id = lastInsertId()
//
// DEBUG    : Ekleme başarısızsa:
//              qDebug() << q.lastError().text() << q.boundValues();
//            En sık sebep: unvan boş -> "NOT NULL constraint failed:
//            customers.unvan" (unvan şemada NOT NULL'dur, diğer alanlar değil).
//            Başarılıysa:  qDebug() << customer.id;   0 ise INSERT olmamıştır.
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


// ═══ RepoCustomers::listAll() ═════════════════════════════════════════════
// NE YAPAR : Müşterileri okur. includeInactive false ise (varsayılan) pasif
//            müşteriler listeye GİRMEZ. PageQuote'un müşteri açılır listesini
//            besleyen fonksiyondur.
//
// ADIM ADIM: RepoItems::listAll ile aynı kalıp — SQL seçilir, exec edilir,
//            while(q.next()) ile Customer'lara çevrilir.
//
// TUZAK — GERÇEK KULLANICI ETKİSİ:
//   PageQuote::currentCustomer() müşteriyi BU LİSTEDE arar. Liste varsayılan
//   olarak pasifleri dışladığı için, PASİFE ALINMIŞ bir müşterinin ESKİ
//   TEKLİFİNİ açtığınızda müşteri alanı BOŞ gelir ve baskıda antet müşterisiz
//   çıkar. Böyle bir durumda:
//              qDebug() << quote.customerId << m_customers.size();
//   customerId dolu ama listede yoksa teşhis budur. Kalıcı çözüm
//   RepoCustomers::get(id) eklemektir (henüz yok).
//
// SIRALAMA TUZAĞI: ORDER BY unvan da BINARY collation kullanır — Çelik A.Ş.,
//   İnşaat Ltd. gibi unvanlar Z'DEN SONRAYA düşer. (Ayrıntı için
//   RepoItems::listAll üzerindeki nota bakın.)
//
// SESSİZ HATA: exec() başarısız olursa boş vektör döner, errorOut yok.
//   Müşteri listesi boş görünüyorsa geçici olarak:
//              if (!q.exec(sql)) qDebug() << q.lastError().text();
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
