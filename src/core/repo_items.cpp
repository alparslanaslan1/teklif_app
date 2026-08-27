#include "repo_items.h"
#include "csv.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {


// Açık bir sorgunun MEVCUT satırını Item nesnesine doldurur (çağrılmadan önce
// q.next() true dönmüş olmalıdır).
// Sütunlara indeksle değil ADIYLA erişilir; böylece SELECT'teki sütun sırası
// değişse de kod bozulmaz.
//   catVar : category_id NULL olabilir. NULL ise 0'a çevrilir (0 = kategori yok).
// guncelleme için biçim elle verilir: SQLite'ın datetime('now') çıktısı
// "yyyy-MM-dd HH:mm:ss"tir, Qt::ISODate'in beklediği 'T' ayracını içermez.
Item itemFromQuery(const QSqlQuery &q)
{
    Item it;
    it.id = q.value(QStringLiteral("id")).toLongLong();
    it.kod = q.value(QStringLiteral("kod")).toString();
    it.ad = q.value(QStringLiteral("ad")).toString();
    it.birim = q.value(QStringLiteral("birim")).toString();
    it.varsayilanFiyat = Money(q.value(QStringLiteral("varsayilan_fiyat")).toLongLong());

    const QVariant catVar = q.value(QStringLiteral("category_id"));
    it.categoryId = catVar.isNull() ? 0 : catVar.toLongLong();
    it.aktif = q.value(QStringLiteral("aktif")).toInt() != 0;

    // SQLite'ın datetime('now') çıktısı "yyyy-MM-dd HH:mm:ss" biçimindedir
    // (Qt::ISODate'in beklediği 'T' ayracı değil, boşluk) — biçimi elle
    // veriyoruz, aksi halde ayrıştırma sessizce geçersiz bir QDateTime verir.
    it.guncelleme = QDateTime::fromString(q.value(QStringLiteral("guncelleme")).toString(),
                                           QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    return it;
}


// Hatanın "aynı kod zaten var" (UNIQUE ihlali) olup olmadığını anlar; böylece
// kullanıcıya ham SQLite metni yerine anlaşılır bir mesaj gösterilebilir.
// Hata METNİNDE "UNIQUE" aranır ("UNIQUE constraint failed: items.kod" gibi):
// Qt'nin QSQLITE sürücüsünde native hata kodu sürümden sürüme farklı
// yorumlanabildiği için sabit İngilizce metin daha güvenilir çıkıyor.
bool isUniqueViolation(const QSqlError &err)
{
    // Qt'nin QSQLITE sürücüsünde native error code sürümden sürüme farklı
    // yorumlanabiliyor; SQLite'ın sabit ingilizce metnini aramak daha
    // güvenilir: "UNIQUE constraint failed: items.kod" gibi.
    return err.text().contains(QStringLiteral("UNIQUE"), Qt::CaseInsensitive);
}

} // namespace


// Katalog'a yeni kalem ekler. Başarılıysa item.id, veritabanının verdiği id
// ile doldurulur — parametre bu yüzden referanstır.
//   :kategori  : categoryId 0 ise DB'ye NULL yazılır (boş QVariant = SQL NULL);
//                0 yazmak foreign key ihlali olurdu
//   guncelleme : C++ saatine güvenilmez, SQL tarafında datetime('now') ile yazılır
// kod alanı UNIQUE'tir; çakışma durumunda anlaşılır bir mesaj üretilir.
bool RepoItems::add(QSqlDatabase &db, Item &item, QString *errorOut)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO items (kod, ad, birim, varsayilan_fiyat, category_id, aktif, guncelleme) "
        "VALUES (:kod, :ad, :birim, :fiyat, :kategori, :aktif, datetime('now'))"));
    q.bindValue(QStringLiteral(":kod"), item.kod);
    q.bindValue(QStringLiteral(":ad"), item.ad);
    q.bindValue(QStringLiteral(":birim"), item.birim);
    q.bindValue(QStringLiteral(":fiyat"), item.varsayilanFiyat.kurus());
    q.bindValue(QStringLiteral(":kategori"), item.categoryId != 0 ? QVariant(item.categoryId) : QVariant());
    q.bindValue(QStringLiteral(":aktif"), item.aktif ? 1 : 0);

    if (!q.exec()) {
        if (errorOut) {
            *errorOut = isUniqueViolation(q.lastError())
                            ? QStringLiteral("Bu kod zaten kayıtlı: %1").arg(item.kod)
                            : q.lastError().text();
        }
        return false;
    }

    item.id = q.lastInsertId().toLongLong();
    return true;
}


// Var olan bir kalemi id'sine göre tamamen günceller (kısmi güncelleme yok,
// tüm alanlar yazılır). Bind mantığı add() ile aynıdır, üzerine WHERE id = :id.
bool RepoItems::update(QSqlDatabase &db, const Item &item, QString *errorOut)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE items SET kod=:kod, ad=:ad, birim=:birim, varsayilan_fiyat=:fiyat, "
        "category_id=:kategori, aktif=:aktif, guncelleme=datetime('now') WHERE id=:id"));
    q.bindValue(QStringLiteral(":kod"), item.kod);
    q.bindValue(QStringLiteral(":ad"), item.ad);
    q.bindValue(QStringLiteral(":birim"), item.birim);
    q.bindValue(QStringLiteral(":fiyat"), item.varsayilanFiyat.kurus());
    q.bindValue(QStringLiteral(":kategori"), item.categoryId != 0 ? QVariant(item.categoryId) : QVariant());
    q.bindValue(QStringLiteral(":aktif"), item.aktif ? 1 : 0);
    q.bindValue(QStringLiteral(":id"), item.id);

    if (!q.exec()) {
        if (errorOut) {
            *errorOut = isUniqueViolation(q.lastError())
                            ? QStringLiteral("Bu kod zaten kayıtlı: %1").arg(item.kod)
                            : q.lastError().text();
        }
        return false;
    }
    return true;
}


// Kalemi SİLMEZ, sadece aktif/pasif durumunu değiştirir. Programın hiçbir
// yerinde items üzerinde gerçek DELETE çağrılmaz: geçmiş tekliflerdeki satırlar
// fiyatı kopyaladığı için silme onları bozmaz, ama kalemin katalogdan
// kaybolması kullanıcıyı yanıltır.
bool RepoItems::setActive(QSqlDatabase &db, qint64 id, bool aktif, QString *errorOut)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE items SET aktif=:aktif, guncelleme=datetime('now') WHERE id=:id"));
    q.bindValue(QStringLiteral(":aktif"), aktif ? 1 : 0);
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec()) {
        if (errorOut)
            *errorOut = q.lastError().text();
        return false;
    }
    return true;
}


// Katalogdaki kalemleri ada göre alfabetik sıralı döner.
//   includeInactive : false ise (varsayılan) pasif kalemler listeye girmez
// Her satır itemFromQuery() ile Item'a çevrilir.
QVector<Item> RepoItems::listAll(QSqlDatabase &db, bool includeInactive)
{
    QVector<Item> sonuc;
    QSqlQuery q(db);
    const QString sql = includeInactive ? QStringLiteral("SELECT * FROM items ORDER BY ad")
                                         : QStringLiteral("SELECT * FROM items WHERE aktif=1 ORDER BY ad");
    if (!q.exec(sql))
        return sonuc;

    while (q.next())
        sonuc.append(itemFromQuery(q));
    return sonuc;
}


// categories tablosunun tamamını id -> ad sözlüğü olarak yükler. CSV dışa
// aktarımında her kalem için ayrı sorgu atmamak için tek seferde alınır.
QHash<qint64, QString> RepoItems::categoryNameMap(QSqlDatabase &db)
{
    QHash<qint64, QString> harita;
    QSqlQuery q(db);
    if (q.exec(QStringLiteral("SELECT id, ad FROM categories"))) {
        while (q.next())
            harita.insert(q.value(0).toLongLong(), q.value(1).toString());
    }
    return harita;
}


// Ada göre kategori bulur, yoksa oluşturur. CSV içe aktarımında kullanılır,
// çünkü CSV kategoriyi id ile değil ADIYLA taşır.
// Dönüş değeri üç anlamlıdır:
//   > 0   bulunan ya da yeni oluşturulan kategori id'si
//   == 0  ad boş -> "kategorisiz" (hata değildir)
//   -1    gerçek hata; çağıran taraf işlemi geri almalıdır
qint64 RepoItems::categoryIdForName(QSqlDatabase &db, const QString &ad, QString *errorOut)
{
    if (ad.trimmed().isEmpty())
        return 0;

    QSqlQuery bul(db);
    bul.prepare(QStringLiteral("SELECT id FROM categories WHERE ad = :ad"));
    bul.bindValue(QStringLiteral(":ad"), ad);
    if (bul.exec() && bul.next())
        return bul.value(0).toLongLong();

    QSqlQuery ekle(db);
    ekle.prepare(QStringLiteral("INSERT INTO categories (ad) VALUES (:ad)"));
    ekle.bindValue(QStringLiteral(":ad"), ad);
    if (!ekle.exec()) {
        if (errorOut)
            *errorOut = ekle.lastError().text();
        return -1;
    }
    return ekle.lastInsertId().toLongLong();
}


// CSV metnini katalog'a aktarır. Önce TÜM satırlar doğrulanır, sonra tek bir
// transaction içinde eklenir: bozuk bir satır ya da çakışan bir kod varsa tek
// satır bile eklenmeden hepsi geri alınır.
//   satirlar : doğrulanmış CSV satırları. Bu noktaya kadar veritabanına hiç
//              dokunulmamıştır.
//   catId    : satırın kategori id'si; kategori adı katalogda yoksa
//              categoryIdForName() tarafından otomatik oluşturulur
bool RepoItems::importCsv(QSqlDatabase &db, const QString &csvContent, QString *errorOut)
{
    QString parseErr;
    const QVector<CsvItemRow> satirlar = csvSatirlariniAyristir(csvContent, &parseErr);
    if (!parseErr.isEmpty()) {
        if (errorOut)
            *errorOut = parseErr;
        return false;
    }
    if (satirlar.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("İçe aktarılacak satır bulunamadı.");
        return false;
    }

    QSqlQuery tx(db);
    tx.exec(QStringLiteral("BEGIN IMMEDIATE"));

    for (const CsvItemRow &s : satirlar) {
        QString catErr;
        const qint64 catId = categoryIdForName(db, s.kategoriAdi, &catErr);
        if (catId < 0) {
            QSqlQuery rb(db);
            rb.exec(QStringLiteral("ROLLBACK"));
            if (errorOut)
                *errorOut = catErr;
            return false;
        }

        Item it;
        it.kod = s.kod;
        it.ad = s.ad;
        it.birim = s.birim;
        it.varsayilanFiyat = s.fiyat;
        it.categoryId = catId;

        QString addErr;
        if (!add(db, it, &addErr)) {
            QSqlQuery rb(db);
            rb.exec(QStringLiteral("ROLLBACK"));
            if (errorOut)
                *errorOut = QStringLiteral("\"%1\" satırı: %2").arg(s.kod, addErr);
            return false;
        }
    }

    QSqlQuery commit(db);
    if (!commit.exec(QStringLiteral("COMMIT"))) {
        if (errorOut)
            *errorOut = commit.lastError().text();
        return false;
    }
    return true;
}


// Aktif + pasif TÜM kalemleri CSV metnine döker. Pasifler de dışa aktarılır ki
// dosya tam bir yedek olsun.
//   kategoriler : id -> ad sözlüğü; kalem başına sorgu atılmasın diye önceden alınır
QString RepoItems::exportCsv(QSqlDatabase &db, QString *errorOut)
{
    if (!db.isOpen()) {
        if (errorOut)
            *errorOut = QStringLiteral("Veritabanı bağlantısı açık değil.");
        return QString();
    }
    const QVector<Item> kalemler = listAll(db, /*includeInactive=*/true);
    const QHash<qint64, QString> kategoriler = categoryNameMap(db);
    return csvOlustur(kalemler, kategoriler);
}
