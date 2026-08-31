#include "repo_items.h"

#include "csv.h"
#include "transaction.h"
#include "turkish.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <algorithm>
#include <utility>

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

RepoItems::RepoItems(QSqlDatabase db) : m_db(std::move(db)) {}


// Katalog'a yeni kalem ekler. Başarılıysa item.id, veritabanının verdiği id
// ile doldurulur — parametre bu yüzden referanstır.
//   :kategori  : categoryId 0 ise DB'ye NULL yazılır (boş QVariant = SQL NULL);
//                0 yazmak foreign key ihlali olurdu
//   guncelleme : C++ saatine güvenilmez, SQL tarafında datetime('now') ile yazılır
// kod alanı UNIQUE'tir; çakışma durumunda anlaşılır bir mesaj üretilir.
bool RepoItems::add(Item &item, QString *errorOut)
{
    QSqlQuery q(m_db);
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
bool RepoItems::update(const Item &item, QString *errorOut)
{
    QSqlQuery q(m_db);
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

    // Var olmayan bir id ile UPDATE hata vermez, 0 satır etkiler. Kontrol
    // edilmezse çağıran taraf kaydettiğini sanır.
    if (q.numRowsAffected() == 0) {
        if (errorOut)
            *errorOut = QStringLiteral("Güncellenecek kalem bulunamadı (id %1).").arg(item.id);
        return false;
    }
    return true;
}


// Kalemi SİLMEZ, sadece aktif/pasif durumunu değiştirir. Programın hiçbir
// yerinde items üzerinde gerçek DELETE çağrılmaz: geçmiş tekliflerdeki satırlar
// fiyatı kopyaladığı için silme onları bozmaz, ama kalemin katalogdan
// kaybolması kullanıcıyı yanıltır.
bool RepoItems::setActive(qint64 id, bool aktif, QString *errorOut)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE items SET aktif=:aktif, guncelleme=datetime('now') WHERE id=:id"));
    q.bindValue(QStringLiteral(":aktif"), aktif ? 1 : 0);
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec()) {
        if (errorOut)
            *errorOut = q.lastError().text();
        return false;
    }
    if (q.numRowsAffected() == 0) {
        if (errorOut)
            *errorOut = QStringLiteral("Kalem bulunamadı (id %1).").arg(id);
        return false;
    }
    return true;
}


// Katalogdaki kalemleri ada göre alfabetik sıralı döner.
//   includeInactive : false ise (varsayılan) pasif kalemler listeye girmez
// Her satır itemFromQuery() ile Item'a çevrilir.
QVector<Item> RepoItems::list(const ItemFilter &filtre, QString *errorOut) const
{
    QVector<Item> sonuc;
    QSqlQuery q(m_db);

    // ORDER BY YOK — sıralama C++ tarafında yapılır (bkz. turkish.h).
    QString sql = QStringLiteral("SELECT * FROM items WHERE 1=1");
    if (!filtre.includeInactive)
        sql += QStringLiteral(" AND aktif=1");
    if (filtre.categoryId != 0)
        sql += QStringLiteral(" AND category_id = :cat");

    q.prepare(sql);
    if (filtre.categoryId != 0)
        q.bindValue(QStringLiteral(":cat"), filtre.categoryId);

    if (!q.exec()) {
        // Hata yutulmuyor: "katalog boş" ile "sorgu patladı" ayırt edilebilmeli.
        if (errorOut)
            *errorOut = q.lastError().text();
        return sonuc;
    }

    // Metin araması SQL'de DEĞİL burada: LIKE Türkçe harfleri katlayamaz,
    // "iscilik" yazan kullanıcı "İşçilik"i bulamazdı (bkz. core/turkish.h).
    const QString anahtar = turkishSearchNormalize(filtre.aranan.trimmed());

    while (q.next()) {
        const Item it = itemFromQuery(q);
        if (!anahtar.isEmpty()) {
            const QString alan = turkishSearchNormalize(it.ad + QLatin1Char(' ') + it.kod);
            if (!alan.contains(anahtar))
                continue;
        }
        sonuc.append(it);
    }

    std::sort(sonuc.begin(), sonuc.end(),
              turkishLessBy<Item>([](const Item &i) { return i.ad; }));
    return sonuc;
}

QVector<Item> RepoItems::listAll(bool includeInactive, QString *errorOut) const
{
    ItemFilter f;
    f.includeInactive = includeInactive;
    return list(f, errorOut);
}

std::optional<Item> RepoItems::get(qint64 id, QString *errorOut) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM items WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec()) {
        if (errorOut)
            *errorOut = q.lastError().text();
        return std::nullopt;
    }
    if (!q.next()) {
        if (errorOut)
            *errorOut = QStringLiteral("Kalem bulunamadı (id %1).").arg(id);
        return std::nullopt;
    }
    return itemFromQuery(q);
}

QVector<Category> RepoItems::listCategories(QString *errorOut) const
{
    QVector<Category> sonuc;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT id, ad FROM categories"))) {
        if (errorOut)
            *errorOut = q.lastError().text();
        return sonuc;
    }
    while (q.next())
        sonuc.append(Category{q.value(0).toLongLong(), q.value(1).toString()});

    std::sort(sonuc.begin(), sonuc.end(),
              turkishLessBy<Category>([](const Category &c) { return c.ad; }));
    return sonuc;
}

QHash<qint64, QString> RepoItems::categoryNameMap() const
{
    QHash<qint64, QString> harita;
    QSqlQuery q(m_db);
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
qint64 RepoItems::ensureCategory(const QString &ad, QString *errorOut)
{
    // Baştaki/sondaki boşluklar HER YERDE atılır. Daha önce yalnızca boşluk
    // kontrolünde trimmed() kullanılıp SELECT/INSERT ham metinle yapılıyordu;
    // bu yüzden " Boya" ve "Boya" iki ayrı kategori oluyordu.
    const QString temiz = ad.trimmed();
    if (temiz.isEmpty())
        return 0; // kategorisiz — hata değil

    QSqlQuery bul(m_db);
    // COLLATE NOCASE: "boya" ile "Boya" aynı kategoridir. Kullanıcı CSV'yi
    // elle düzenlediği için büyük/küçük harf tutarlılığı beklenemez.
    bul.prepare(QStringLiteral("SELECT id FROM categories WHERE ad = :ad COLLATE NOCASE"));
    bul.bindValue(QStringLiteral(":ad"), temiz);
    if (!bul.exec()) {
        // Sorgu hatasını "bulunamadı" sanıp INSERT denemek yanlış olurdu.
        if (errorOut)
            *errorOut = bul.lastError().text();
        return -1;
    }
    if (bul.next())
        return bul.value(0).toLongLong();

    QSqlQuery ekle(m_db);
    ekle.prepare(QStringLiteral("INSERT INTO categories (ad) VALUES (:ad)"));
    ekle.bindValue(QStringLiteral(":ad"), temiz);
    if (!ekle.exec()) {
        if (errorOut)
            *errorOut = ekle.lastError().text();
        return -1;
    }
    return ekle.lastInsertId().toLongLong();
}

bool RepoItems::importCsv(const QString &csvContent, QString *errorOut)
{
    // AŞAMA 1 — doğrula. Veritabanına henüz hiç dokunulmaz.
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

    // AŞAMA 2 — yaz. Transaction RAII: her erken return ROLLBACK eder,
    // tek satır bile yarım kalmaz.
    Transaction tx(m_db);
    if (!tx.isActive()) {
        if (errorOut)
            *errorOut = tx.lastError();
        return false;
    }

    for (const CsvItemRow &s : satirlar) {
        const qint64 catId = ensureCategory(s.kategoriAdi, errorOut);
        if (catId < 0)
            return false;

        Item it;
        it.kod = s.kod;
        it.ad = s.ad;
        it.birim = s.birim;
        it.varsayilanFiyat = s.fiyat;
        it.categoryId = catId;

        QString addErr;
        if (!add(it, &addErr)) {
            if (errorOut)
                *errorOut = QStringLiteral("\"%1\" satırı: %2").arg(s.kod, addErr);
            return false;
        }
    }

    return tx.commit(errorOut);
}

QString RepoItems::exportCsv(QString *errorOut) const
{
    if (!m_db.isOpen()) {
        if (errorOut)
            *errorOut = QStringLiteral("Veritabanı bağlantısı açık değil.");
        return QString();
    }
    const QVector<Item> kalemler = listAll(/*includeInactive=*/true);
    const QHash<qint64, QString> kategoriler = categoryNameMap();
    return csvOlustur(kalemler, kategoriler);
}
