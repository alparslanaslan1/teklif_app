#include "repo_items.h"
#include "csv.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {


// ═══ itemFromQuery() ══════════════════════════════════════════════════════
// NE YAPAR : Açık bir sorgunun MEVCUT satırını Item nesnesine doldurur.
//            Çağrılmadan önce q.next() true dönmüş olmalıdır.
//
// ADIM ADIM (her alan için dikkat edilecekler):
//   1) Sütunlara ADIYLA erişilir (q.value("kod")). İNDEKSLE değil — böylece
//      SELECT'teki sütun sırası değişse bile kod bozulmaz.
//      TUZAK: ad yanlış yazılırsa Qt uyarı basar ve GEÇERSİZ QVariant döner;
//             alan sessizce boş/0 kalır. Boş gelen bir alan varsa İLK
//             şüpheli budur.
//   2) varsayilan_fiyat DB'de KURUŞ tam sayısıdır -> doğrudan Money(...)
//   3) category_id NULL olabilir -> isNull() ile 0'a çevrilir
//      (0 = "kategori yok"; geçerli rowid'ler 1'den başladığı için güvenli).
//   4) aktif INTEGER -> != 0 ile bool
//   5) guncelleme: SQLite'ın datetime('now') çıktısı "yyyy-MM-dd HH:mm:ss"
//      biçimindedir; Qt::ISODate'in beklediği 'T' ayracı YOKTUR. Bu yüzden
//      biçim ELLE verilir. Verilmezse QDateTime sessizce GEÇERSİZ olur.
//
// DEBUG    : Bir alan hep boş geliyorsa sorgunun gerçekten o sütunu getirip
//            getirmediğine bakın:
//              qDebug() << q.record().count() << q.record().fieldNames();
//            guncelleme geçersizse:  qDebug() << it.guncelleme.isValid();
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


// ═══ isUniqueViolation() ══════════════════════════════════════════════════
// NE YAPAR : Hatanın "aynı kod zaten var" (UNIQUE ihlali) olup olmadığını
//            anlar; böylece kullanıcıya ham SQLite metni yerine anlaşılır bir
//            mesaj gösterilebilir.
//
// NASIL    : Hata METNİNDE "UNIQUE" geçiyor mu diye bakar. SQLite tipik olarak
//            "UNIQUE constraint failed: items.kod" yazar.
//
// NEDEN metin: Qt'nin QSQLITE sürücüsünde native hata kodu sürümden sürüme
//            farklı yorumlanabiliyor; SQLite'ın sabit İNGİLİZCE metni daha
//            güvenilir çıkıyor.
//
// DEBUG    : Kullanıcıya "Bu kod zaten kayıtlı" yerine ham SQL hatası
//            gösteriliyorsa buradan başlayın:
//              qDebug() << err.text() << err.nativeErrorCode() << err.type();
//            Daha sağlamı nativeErrorCode() == "2067" (SQLITE_CONSTRAINT_UNIQUE)
//            kontrolüdür — metne bağlı kalmaz.
bool isUniqueViolation(const QSqlError &err)
{
    // Qt'nin QSQLITE sürücüsünde native error code sürümden sürüme farklı
    // yorumlanabiliyor; SQLite'ın sabit ingilizce metnini aramak daha
    // güvenilir: "UNIQUE constraint failed: items.kod" gibi.
    return err.text().contains(QStringLiteral("UNIQUE"), Qt::CaseInsensitive);
}

} // namespace


// ═══ RepoItems::add() ═════════════════════════════════════════════════════
// NE YAPAR : Katalog'a yeni kalem ekler. Başarılıysa item.id'yi DB'nin verdiği
//            id ile DOLDURUR (bu yüzden item referansla alınır).
//
// ADIM ADIM:
//   1) INSERT hazırlanır. guncelleme sütunu SQL tarafında datetime('now')
//      ile doldurulur — C++ saatine güvenilmez, tüm satırlar aynı kaynaktan.
//   2) Değerler bind edilir. ÖZEL DURUM: categoryId == 0 ise DB'ye NULL yazılır
//      (QVariant() boş varyant = SQL NULL). 0 yazmak FK ihlali olurdu.
//   3) exec() başarısızsa: UNIQUE ihlali mi diye bakılır ->
//      "Bu kod zaten kayıtlı: X" veya ham hata metni.
//   4) item.id = lastInsertId()
//
// DEBUG    : Ekleme başarısızsa önce hangi dalda olduğunuza bakın:
//              qDebug() << q.lastError().text() << q.executedQuery();
//            • "UNIQUE constraint failed: items.kod" -> kod tekrar ediyor
//            • "FOREIGN KEY constraint failed"       -> categoryId var olmayan
//              bir kategoriyi gösteriyor (2. adımdaki NULL dönüşümüne bakın)
//            • "NOT NULL constraint failed"          -> kod/ad/birim boş
//            Başarılıysa: qDebug() << item.id;  0 ise INSERT çalışmamıştır.
//
// PERFORMANS: Her çağrıda YENİ bir QSqlQuery + prepare() yapılır. Tek tek
//            ekleme için sorun değil; ama importCsv binlerce kez çağırdığı
//            için orada belirgin bir maliyet oluşturur.
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


// ═══ RepoItems::update() ══════════════════════════════════════════════════
// NE YAPAR : Var olan kalemi id'sine göre TAMAMEN günceller (kısmi güncelleme
//            yok — tüm alanlar yazılır).
//
// ADIM ADIM: add() ile aynı bind mantığı + WHERE id = :id.
//            categoryId == 0 -> NULL dönüşümü burada da geçerli.
//
// DEBUG / ÖNEMLİ TUZAK:
//   Bu fonksiyon ETKİLENEN SATIR SAYISINI KONTROL ETMEZ. Var olmayan bir id
//   verirseniz SQL başarıyla çalışır, 0 satır günceller ve fonksiyon TRUE
//   döner. Çağıran taraf kaydettiğini sanır.
//   Bunu test etmek için:
//              q.exec(); qDebug() << "etkilenen satır:" << q.numRowsAffected();
//   0 görüyorsanız ya item.id yanlış, ya kayıt silinmiş demektir.
//   Kalıcı düzeltme: numRowsAffected() == 0 durumunda "Kayıt bulunamadı" hatası.
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


// ═══ RepoItems::setActive() ═══════════════════════════════════════════════
// NE YAPAR : Kalemi SİLMEZ, sadece aktif/pasif yapar.
//
// NEDEN SİLME YOK: Geçmiş tekliflerdeki satırlar fiyatı/adı KOPYALADIĞI için
//   silme onları teknik olarak bozmaz — ama katalogdan aniden kaybolması
//   kullanıcıyı yanıltır. Programda hiçbir yerde items üzerinde DELETE yoktur.
//
// ADIM ADIM: tek bir UPDATE; aktif alanı ve guncelleme damgası yazılır.
//
// DEBUG    : Pasife aldığınız kalem hâlâ aramada çıkıyorsa sıra şu:
//            1) DB'de gerçekten değişti mi:  SELECT aktif FROM items WHERE id=?
//            2) Değiştiyse arayüz katalogu yenilemedi demektir
//               (PageQuote::reloadCatalog çağrılmalı) — ItemSearch katalogun
//               BELLEKTEKİ KOPYASI üzerinde arar, DB'yi tekrar okumaz.
//
// TUZAK    : update() ile aynı — numRowsAffected() kontrol edilmiyor, olmayan
//            id sessizce "başarılı" sayılıyor.
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


// ═══ RepoItems::listAll() ═════════════════════════════════════════════════
// NE YAPAR : Katalogdaki kalemleri okur. includeInactive false ise (varsayılan)
//            pasif kalemler LİSTEYE GİRMEZ.
//
// ADIM ADIM:
//   1) includeInactive'e göre iki SQL'den biri seçilir (WHERE aktif=1 var/yok).
//   2) exec() başarısızsa BOŞ VEKTÖR döner.
//   3) while(q.next()) döngüsünde her satır itemFromQuery ile Item'a çevrilir.
//
// ÖNEMLİ TUZAK — SESSİZ HATA:
//   2. adımda hata YUTULUR. errorOut parametresi bile yok. Yani
//   "katalog gerçekten boş" ile "SQL patladı" AYIRT EDİLEMEZ.
//   Arama kutusu hiç sonuç vermiyorsa buraya geçici bir çıktı koyun:
//              if (!q.exec(sql)) qDebug() << "listAll FAIL:" << q.lastError().text();
//   En sık sebep: bağlantı kapanmış ya da migration hiç çalışmamış
//   ("no such table: items").
//
// SIRALAMA TUZAĞI: ORDER BY ad, SQLite'ın varsayılan BINARY (UTF-8 bayt)
//   collation'ını kullanır. Türkçede YANLIŞ sonuç verir — Ç, ç, İ, ı, Ş, Ğ,
//   Ü, Ö ile başlayan kalemler Z'DEN SONRAYA düşer:
//     Ahşap, Alçıpan, Beton, Zeytin, Çimento, çıta, İşçilik, ısı yalıtımı
//   Doğrusu için sıralamayı SQL'de değil, C++ tarafında
//   QCollator(QLocale(QLocale::Turkish)) ile yapmak gerekir.
//
// PERFORMANS: SELECT * tüm sütunları çeker ve TÜM katalog belleğe alınır.
//   ~50.000 kaleme kadar sorun değil; ötesinde FTS5 + sayfalama gerekir.
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


// ═══ RepoItems::categoryNameMap() ═════════════════════════════════════════
// NE YAPAR : categories tablosunun tamamını id -> ad sözlüğü olarak yükler.
//            CSV dışa aktarımında her kalem için ayrı sorgu atmamak için
//            (N+1 sorgu probleminden kaçınma).
//
// ADIM ADIM: tek SELECT, sonuçlar QHash'e doldurulur. Hata olursa BOŞ HARİTA
//            döner — yine sessizdir.
//
// DEBUG    : CSV çıktısında kategori sütunu boş geliyorsa:
//              qDebug() << harita.size() << harita;
//            • 0 ise sorgu patlamış ya da hiç kategori yok
//            • Dolu ama çıktı boşsa kalemlerin categoryId'si 0'dır
//              (yani gerçekten kategorisizler)
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


// ═══ RepoItems::categoryIdForName() ═══════════════════════════════════════
// NE YAPAR : Ada göre kategori bulur; YOKSA OLUŞTURUR ("get or create").
//            CSV içe aktarımında kullanılır.
//
// DÖNÜŞ DEĞERİ ÜÇ ANLAMLIDIR — karıştırmayın:
//    > 0  bulunan ya da yeni oluşturulan kategori id'si
//    == 0 ad boş -> "kategorisiz" (HATA DEĞİL)
//    < 0  (-1) gerçek hata; çağıran taraf ROLLBACK etmeli
//
// ADIM ADIM:
//   1) ad.trimmed().isEmpty() ise 0 döner.
//   2) SELECT id FROM categories WHERE ad = :ad
//   3) Bulunursa id döner.
//   4) Bulunamazsa INSERT edilir; başarısızsa -1.
//
// TUZAK 1 (GERÇEK HATA): 1. adımda trimmed() SADECE boşluk kontrolü için
//   kullanılıyor; 2. ve 4. adımlar ad'ın HAM halini kullanıyor. Bu yüzden
//   " Boya" ve "Boya" İKİ AYRI kategori olur. CSV'de fazladan boşluk varsa
//   kategoriler ikizlenir.  Kontrol:  qDebug() << "[" << ad << "]";
//
// TUZAK 2: Karşılaştırma '=' ile yapılıyor, yani BÜYÜK/KÜÇÜK HARF DUYARLI.
//   "boya" ve "Boya" da ayrı kategoriler olur.
//
// TUZAK 3: 2. adımda exec() başarısız olursa (bul.exec() false) kod bunu
//   "bulunamadı" sayıp INSERT'e geçer. Gerçek bir DB hatası, yanlışlıkla
//   yeni kayıt denemesine dönüşür.
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


// ═══ RepoItems::importCsv() ═══════════════════════════════════════════════
// NE YAPAR : CSV metnini katalog'a aktarır. "HEPSİ YA DA HİÇBİRİ" garantisi:
//            tek satır bile başarısız olursa HİÇBİR satır eklenmez.
//
// İKİ AŞAMALI TASARIM (sıra kritik):
//   AŞAMA 1 — DOĞRULA (DB'ye hiç dokunmadan)
//     1) csvSatirlariniAyristir(): biçim/fiyat hatası varsa BURADA çıkılır.
//        DB'ye tek bir yazma bile yapılmamış olur.        [ÇIKIŞ 1]
//     2) Sonuç boşsa "İçe aktarılacak satır bulunamadı."  [ÇIKIŞ 2]
//   AŞAMA 2 — YAZ (tek transaction içinde)
//     3) BEGIN IMMEDIATE  (dönüş değeri KONTROL EDİLMİYOR!)
//     4) Her satır için:
//        a) categoryIdForName(): kategori yoksa oluşturulur.
//           -1 dönerse ROLLBACK + çık.                    [ÇIKIŞ 3]
//        b) add(): kod çakışırsa ROLLBACK + çık. Hata mesajının başına
//           hangi kodun patladığı eklenir.                [ÇIKIŞ 4]
//     5) COMMIT
//
// DEBUG    : İçe aktarma başarısızsa hata mesajı zaten satır numarasını ya da
//            kodu içerir. Yine de takılırsanız:
//              qDebug() << "ayrıştırılan satır:" << satirlar.size();
//            Döngü içinde:  qDebug() << s.kod << s.kategoriAdi << catId;
//            İşlem "başarılı" göründüğü halde tablo boşsa 5. adımdaki COMMIT'e
//            bakın — BEGIN başarısız olduysa her şey otomatik commit edilmiş
//            ya da kaybolmuş olabilir.
//
// TUZAK    : 3. adımdaki BEGIN'in sonucu okunmuyor. Bu fonksiyonu DIŞARIDAN
//            açılmış bir transaction içine koyarsanız BEGIN sessizce başarısız
//            olur ve 4a/4b'deki ROLLBACK DIŞTAKİ transaction'ı geri alarak
//            ilgisiz verileri siler.
//
// PERFORMANS: add() her satırda yeni QSqlQuery + prepare() yapar. 5.000 satırlık
//            bir CSV = 5.000 prepare. Döngü dışında bir kez hazırlanıp sadece
//            bind edilse kat kat hızlanır.
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


// ═══ RepoItems::exportCsv() ═══════════════════════════════════════════════
// NE YAPAR : TÜM kalemleri (pasifler DAHİL) CSV metnine döker.
//
// ADIM ADIM:
//   1) db.isOpen() kontrolü -> kapalıysa hata + boş metin.
//      (listAll hatayı yutacağı için bu ön kontrol tek erken uyarıdır.)
//   2) listAll(includeInactive = TRUE) -> pasifler de dışa aktarılır ki
//      yedek/geri yükleme tam olsun.
//   3) categoryNameMap() ile id -> ad sözlüğü alınır.
//   4) csvOlustur() metni üretir.
//
// DEBUG    : Boş metin dönüyorsa sırayla bakın:
//              qDebug() << db.isOpen() << kalemler.size() << kategoriler.size();
//            • isOpen false  -> bağlantı kapanmış (1. adımda çıkıldı)
//            • kalemler 0    -> listAll boş döndü; hata yuttuğunu unutmayın,
//                               tabloyu doğrudan sorgulayıp doğrulayın
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
