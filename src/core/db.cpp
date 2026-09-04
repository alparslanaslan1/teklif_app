#include "teklif/core/db.h"

#include "teklif/core/log.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace {


// SQL çalıştırır; başarısız olursa SQLite'ın hata metnini errorOut'a yazıp
// false döner. Tekrar eden if/lastError kalıbını tek yere toplar.
//   errorOut : nullptr olabilir, o zaman hata metni yazılmaz
bool execOrFail(QSqlQuery &q, const QString &sql, QString *errorOut)
{
    if (!q.exec(sql)) {
        // Hangi ifadenin öldüğü kullanıcıya gösterilen mesajda yok; günlükte
        // olması migration sorunlarını uzaktan teşhis etmenin tek yolu.
        qCWarning(logDb) << "SQL başarısız:" << sql.simplified().left(120)
                          << "->" << q.lastError().text();
        if (errorOut)
            *errorOut = q.lastError().text();
        return false;
    }
    return true;
}

} // namespace


// Kullanıcının veri dizinindeki varsayılan .db yolunu üretir.
//   dir : AppDataLocation (Windows'ta %APPDATA%\<Org>\<Uygulama>). Qt bu yolu
//         organizasyon/uygulama adından türetir; bunlar ayarlanmamışsa boş
//         döner ve ~/.teklif kullanılır.
QString Db::defaultPath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) {
        // QCoreApplication::setOrganizationName/setApplicationName henüz
        // çağrılmamışsa Qt bu yolu türetemez. main.cpp'de bu ikisi mutlaka
        // QApplication oluşturulmadan hemen sonra ayarlanmalı.
        dir = QDir::homePath() + QStringLiteral("/.teklif");
    }
    return dir + QStringLiteral("/teklif.db");
}


// Dosyanın şema sürümünü okur. Ayrı bir sürüm tablosu tutulmaz; SQLite'ın
// yerleşik user_version alanı kullanılır. Okunamazsa 0 döner, yani dosya "v0"
// sayılır ve göç baştan uygulanır.
int Db::currentVersion(QSqlDatabase &db)
{
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA user_version")) || !q.next())
        return 0;
    return q.value(0).toInt();
}


// user_version alanını yazar; bir migration adımı başarılı olduktan sonra,
// COMMIT'ten önce çağrılır.
// PRAGMA'nın değeri bir sayı literalidir, parametre olarak bind edilemez
// ("PRAGMA user_version = ?" çalışmaz) — bu yüzden metin birleştirmesi
// kullanılır. Parametre int olduğu için enjeksiyon riski yoktur.
bool Db::setVersion(QSqlDatabase &db, int version)
{
    QSqlQuery q(db);
    // PRAGMA user_version bir tamsayı literalidir, parametre bind edilemez.
    return q.exec(QStringLiteral("PRAGMA user_version = %1").arg(version));
}


// Migration'dan önce veritabanının ham kopyasını alır.
//   stamp      : saniye çözünürlüklü zaman damgası
//   backupPath : "<yol>.bak-YYYYAAGG-SSDDss". Aynı adlı yedek zaten varsa
//                üzerine yazılmaz, hata döner — eski yedek sessizce
//                kaybolmasın diye.
// Çağıran taraf bağlantıyı önce kapatmış olmalıdır (dosya kilitli olmasın).
bool Db::backupFile(const QString &path, QString *errorOut)
{
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString backupPath = path + QStringLiteral(".bak-") + stamp;

    if (QFile::exists(backupPath)) {
        // Aynı saniyede iki migration denemesi gibi bir uç durum; üzerine
        // yazmak yerine reddet, sessizce eski yedeği kaybetme.
        if (errorOut)
            *errorOut = QStringLiteral("Yedek dosyası zaten var: %1").arg(backupPath);
        return false;
    }
    if (!QFile::copy(path, backupPath)) {
        if (errorOut)
            *errorOut = QStringLiteral("Yedek alınamadı: %1").arg(backupPath);
        return false;
    }

    // Yeni yedek alındıktan SONRA budanır: budama başarısız olsa bile
    // elimizde taze bir yedek vardır.
    pruneBackups(path);
    return true;
}

void Db::pruneBackups(const QString &path)
{
    const QFileInfo bilgi(path);
    QDir klasor = bilgi.absoluteDir();

    // Dosya adları "<ad>.bak-YYYYAAGG-SSDDss" biçiminde olduğu için
    // alfabetik sıra = kronolojik sıra; ayrıca dosya tarihine bakmaya
    // gerek yok (kopyalama tarihi yanıltıcı olabilirdi).
    QStringList yedekler =
        klasor.entryList(QStringList{bilgi.fileName() + QStringLiteral(".bak-*")}, QDir::Files);
    yedekler.sort();

    while (yedekler.size() > kSaklananYedek) {
        const QString eskisi = yedekler.takeFirst();
        // Silinemezse (dosya kilitli, izin yok) sessizce geçilir: budama
        // bir kolaylıktır, başarısızlığı migration'ı durdurmamalı.
        QFile::remove(klasor.filePath(eskisi));
    }
}


// v1 şemasını sıfırdan kurar; yalnızca migrateStep(fromVersion = 0) çağırır.
//   statements : tablo ve indeks tanımları. SIRAYLA çalıştırılır ve sıra
//                önemlidir — categories, items'tan önce gelmelidir (FK hedefi).
// Tablolar ve aralarındaki bağlar:
//   settings    key/value; teklif no sayacı burada tutulur
//   customers   müşteriler
//   categories  kalem kategorileri (ad UNIQUE)
//   items       katalog; category_id -> categories (ON DELETE SET NULL)
//   quotes      teklif başlığı; customer_id -> customers (ON DELETE RESTRICT),
//               yani teklifi olan bir müşteri silinemez
//   quote_lines teklif satırları; quote_id -> quotes (ON DELETE CASCADE),
//               yani teklif silinince satırları da silinir
// Bu FK davranışlarının gerçekten çalışması için bağlantıda
// "PRAGMA foreign_keys = ON" verilmiş olmalıdır (openAndMigrate bunu yapar).
bool Db::createV1Schema(QSqlDatabase &db, QString *errorOut)
{
    static const QStringList statements = {
        QStringLiteral(R"(
            CREATE TABLE settings (
                key   TEXT PRIMARY KEY,
                value TEXT
            )
        )"),
        QStringLiteral(R"(
            CREATE TABLE customers (
                id             INTEGER PRIMARY KEY,
                unvan          TEXT NOT NULL,
                yetkili        TEXT,
                telefon        TEXT,
                email          TEXT,
                adres          TEXT,
                vergi_dairesi  TEXT,
                vergi_no       TEXT,
                notlar         TEXT,
                aktif          INTEGER NOT NULL DEFAULT 1,
                olusturma      TEXT NOT NULL DEFAULT (datetime('now'))
            )
        )"),
        QStringLiteral(R"(
            CREATE TABLE categories (
                id INTEGER PRIMARY KEY,
                ad TEXT NOT NULL UNIQUE
            )
        )"),
        QStringLiteral(R"(
            CREATE TABLE items (
                id                INTEGER PRIMARY KEY,
                kod               TEXT NOT NULL UNIQUE,
                ad                TEXT NOT NULL,
                birim             TEXT NOT NULL,
                varsayilan_fiyat  INTEGER NOT NULL DEFAULT 0,
                category_id       INTEGER REFERENCES categories(id) ON DELETE SET NULL,
                aktif             INTEGER NOT NULL DEFAULT 1,
                guncelleme        TEXT NOT NULL DEFAULT (datetime('now'))
            )
        )"),
        QStringLiteral(R"(
            CREATE TABLE quotes (
                id              INTEGER PRIMARY KEY,
                teklif_no       TEXT NOT NULL UNIQUE,
                customer_id     INTEGER NOT NULL REFERENCES customers(id) ON DELETE RESTRICT,
                tarih           TEXT NOT NULL,
                gecerlilik_gun  INTEGER NOT NULL DEFAULT 15,
                proje_basligi   TEXT,
                proje_notu      TEXT,
                durum           TEXT NOT NULL DEFAULT 'Taslak',
                sartlar_metni   TEXT,
                ara_toplam      INTEGER NOT NULL DEFAULT 0,
                kdv_orani       INTEGER NOT NULL DEFAULT 0,
                kdv_tutari      INTEGER NOT NULL DEFAULT 0,
                genel_toplam    INTEGER NOT NULL DEFAULT 0,
                olusturma       TEXT NOT NULL DEFAULT (datetime('now')),
                guncelleme      TEXT NOT NULL DEFAULT (datetime('now'))
            )
        )"),
        QStringLiteral(R"(
            CREATE TABLE quote_lines (
                id           INTEGER PRIMARY KEY,
                quote_id     INTEGER NOT NULL REFERENCES quotes(id) ON DELETE CASCADE,
                sira         INTEGER NOT NULL,
                aciklama     TEXT NOT NULL,
                birim        TEXT NOT NULL,
                miktar       REAL NOT NULL,
                birim_fiyat  INTEGER NOT NULL,
                satir_notu   TEXT,
                tutar        INTEGER NOT NULL
            )
        )"),
        QStringLiteral("CREATE INDEX idx_quotes_customer ON quotes(customer_id)"),
        QStringLiteral("CREATE INDEX idx_quote_lines_quote ON quote_lines(quote_id)"),
        QStringLiteral("CREATE INDEX idx_items_category ON items(category_id)"),
    };

    QSqlQuery q(db);
    for (const QString &sql : statements) {
        if (!execOrFail(q, sql, errorOut))
            return false;
    }
    return true;
}


// Müşteri bilgisini ayrı bir tablodan teklifin İÇİNE taşır (v1 -> v2).
//
// NEDEN TABLO YENİDEN KURULUYOR: quotes.customer_id üzerinde customers'a
// bir yabancı anahtar var ve SQLite bir sütunun kısıtını ALTER TABLE ile
// kaldıramaz. Tek doğru yol, hedef şemayla yeni bir tablo kurup veriyi
// taşımaktır — SQLite belgelerinin de önerdiği yöntem budur.
//
// Veri kaybı yok: her teklif, o an bağlı olduğu müşterinin unvanını,
// adresini ve vergi bilgisini kendi satırına KOPYALAR. LEFT JOIN kullanılır,
// çünkü müşterisi bir şekilde kaybolmuş bir teklif de taşınmalıdır (unvanı
// boş kalır, teklif kaybolmaz).
//
// customers tablosu en sonda düşürülür; artık hiçbir yerden okunmuyor.
bool Db::migrateV1ToV2(QSqlDatabase &db, QString *errorOut)
{
    static const QStringList statements = {
        QStringLiteral(R"(
            CREATE TABLE quotes_v2 (
                id                     INTEGER PRIMARY KEY,
                teklif_no              TEXT NOT NULL UNIQUE,
                musteri_unvan          TEXT NOT NULL DEFAULT '',
                musteri_yetkili        TEXT,
                musteri_telefon        TEXT,
                musteri_email          TEXT,
                musteri_adres          TEXT,
                musteri_vergi_dairesi  TEXT,
                musteri_vergi_no       TEXT,
                tarih                  TEXT NOT NULL,
                gecerlilik_gun         INTEGER NOT NULL DEFAULT 15,
                proje_basligi          TEXT,
                proje_notu             TEXT,
                durum                  TEXT NOT NULL DEFAULT 'Taslak',
                sartlar_metni          TEXT,
                ara_toplam             INTEGER NOT NULL DEFAULT 0,
                kdv_orani              INTEGER NOT NULL DEFAULT 0,
                kdv_tutari             INTEGER NOT NULL DEFAULT 0,
                genel_toplam           INTEGER NOT NULL DEFAULT 0,
                olusturma              TEXT NOT NULL DEFAULT (datetime('now')),
                guncelleme             TEXT NOT NULL DEFAULT (datetime('now'))
            )
        )"),
        QStringLiteral(R"(
            INSERT INTO quotes_v2 (id, teklif_no, musteri_unvan, musteri_yetkili,
                                   musteri_telefon, musteri_email, musteri_adres,
                                   musteri_vergi_dairesi, musteri_vergi_no,
                                   tarih, gecerlilik_gun, proje_basligi, proje_notu,
                                   durum, sartlar_metni, ara_toplam, kdv_orani,
                                   kdv_tutari, genel_toplam, olusturma, guncelleme)
            SELECT q.id, q.teklif_no,
                   COALESCE(c.unvan, ''), c.yetkili, c.telefon, c.email, c.adres,
                   c.vergi_dairesi, c.vergi_no,
                   q.tarih, q.gecerlilik_gun, q.proje_basligi, q.proje_notu,
                   q.durum, q.sartlar_metni, q.ara_toplam, q.kdv_orani,
                   q.kdv_tutari, q.genel_toplam, q.olusturma, q.guncelleme
            FROM quotes q LEFT JOIN customers c ON c.id = q.customer_id
        )"),
        // quote_lines.quote_id -> quotes(id) yabancı anahtarı, tablo
        // yeniden adlandırıldığında SQLite tarafından yeni tabloyu
        // gösterecek şekilde taşınır (legacy_alter_table kapalıyken
        // varsayılan davranış budur), satırlar tekliflerine bağlı kalır.
        QStringLiteral("DROP TABLE quotes"),
        QStringLiteral("ALTER TABLE quotes_v2 RENAME TO quotes"),
        QStringLiteral("DROP TABLE customers"),
        // Artık müşteriye göre değil, unvan metnine göre aranıyor.
        QStringLiteral("CREATE INDEX idx_quotes_musteri ON quotes(musteri_unvan)"),
    };

    QSqlQuery q(db);
    for (const QString &sql : statements) {
        if (!execOrFail(q, sql, errorOut))
            return false;
    }
    return true;
}


// Tek bir sürüm sıçramasını uygular.
//   fromVersion : dosyanın MEVCUT sürümü. Başarılı olursa dosya
//                 fromVersion + 1 sürümüne geçmiş olur.
bool Db::migrateStep(QSqlDatabase &db, int fromVersion, QString *errorOut)
{
    switch (fromVersion) {
    case 0:
        return createV1Schema(db, errorOut);
    case 1:
        return migrateV1ToV2(db, errorOut);
    // Eski case'ler SİLİNMEZ: kullanıcıda hâlâ v0 ya da v1 dosya olabilir ve
    // göç zinciri sürümü birer birer yükselterek ilerler.
    default:
        if (errorOut)
            *errorOut = QStringLiteral("Bilinmeyen şema sürümü: %1").arg(fromVersion);
        return false;
    }
}


// Programın veritabanına giriş kapısı: klasörü oluşturur, bağlantıyı açar ve
// şema güncel değilse yedek alıp göçürür.
//   path           : .db dosyasının yolu
//   connectionName : Qt bağlantı adı. Aynı süreçte birden fazla bağımsız
//                    bağlantı açılabilsin diye parametredir (testlerde her
//                    test kendi adını kullanır, birbirine karışmazlar).
//   isNewFile      : dosya çağrıdan önce var mıydı. Yedek alınıp alınmayacağını
//                    belirler — yeni dosyada yedeklenecek bir şey yoktur.
//   openConnection : bağlantıyı açan ve "PRAGMA foreign_keys = ON" veren
//                    lambda. Bu pragma HER bağlantıda yeniden verilmelidir,
//                    SQLite'ta varsayılan olarak kapalıdır.
//   version        : dosyanın mevcut şema sürümü; döngüde birer birer artar
// Akış: klasörü oluştur -> bağlantıyı aç -> sürüm zaten güncelse çık ->
// (var olan dosyaysa) kapat, yedekle, yeniden aç -> her sürüm için
// BEGIN / migrateStep / setVersion / COMMIT. SQLite'ta DDL de transaction'a
// dahil olduğu için yarım kalan bir adım geri alınır ve dosya bozulmaz.
bool Db::openAndMigrate(const QString &path, QString *errorOut, const QString &connectionName)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    const bool isNewFile = !QFile::exists(path);

    auto openConnection = [&]() -> bool {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(path);
        if (!db.open()) {
            if (errorOut)
                *errorOut = db.lastError().text();
            return false;
        }
        QSqlQuery pragma(db);
        // SQLite'ta varsayılan kapalıdır; ON DELETE CASCADE/RESTRICT bu
        // açılmadan hiçbir etki yapmaz. Her bağlantıda yeniden ayarlanmalı.
        pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
        return true;
    };

    if (!openConnection())
        return false;

    {
        QSqlDatabase db = QSqlDatabase::database(connectionName);
        const int version = currentVersion(db);
        if (version >= kSchemaVersion)
            return true; // güncel: yedek alınmaz, migration çalışmaz
    }

    if (!isNewFile) {
        // Var olan bir dosyayı yükseltiyoruz: önce bağlantıyı kapat (dosya
        // kilitlenmemiş olsun), ham kopyasını al, sonra yeniden aç.
        QSqlDatabase::database(connectionName).close();
        QSqlDatabase::removeDatabase(connectionName);

        qCInfo(logDb) << "şema göçü öncesi yedek alınıyor:" << path;
        if (!backupFile(path, errorOut))
            return false;

        if (!openConnection())
            return false;
    }

    QSqlDatabase db = QSqlDatabase::database(connectionName);
    int version = currentVersion(db);

    // ---------------------------------------------------------------------
    // GÖÇ BOYUNCA YABANCI ANAHTARLAR KAPALI.
    //
    // Neden: SQLite'ta bir tablonun şemasını değiştirmenin tek yolu "yeni
    // tablo kur, veriyi taşı, eskisini düşür, yenisini yeniden adlandır"
    // adımlarıdır. Ama DROP TABLE, yabancı anahtarlar AÇIKKEN örtük bir
    // DELETE FROM çalıştırır ve çocuk tablolardaki ON DELETE CASCADE'i
    // tetikler — yani "quotes" tablosunu düşürmek, quote_lines'daki BÜTÜN
    // teklif satırlarını da silerdi. Bu, veriyi sessizce yok eden ve ancak
    // eski bir teklif açıldığında fark edilecek bir hatadır (v1 -> v2
    // göçünün testinde tam olarak böyle yakalandı).
    //
    // Pragma transaction'ın DIŞINDA verilmelidir; içeride etkisizdir.
    // Göç bittikten sonra tekrar açılır ve foreign_key_check ile hiçbir
    // bağın kopmadığı doğrulanır.
    // ---------------------------------------------------------------------
    if (version < kSchemaVersion) {
        QSqlQuery fk(db);
        if (!execOrFail(fk, QStringLiteral("PRAGMA foreign_keys = OFF"), errorOut))
            return false;
    }

    while (version < kSchemaVersion) {
        QSqlQuery tx(db);
        tx.exec(QStringLiteral("BEGIN IMMEDIATE"));

        if (!migrateStep(db, version, errorOut)) {
            QSqlQuery rb(db);
            rb.exec(QStringLiteral("ROLLBACK"));
            return false;
        }

        qCInfo(logDb) << "şema göçü:" << version << "->" << version + 1;
        const int nextVersion = version + 1;
        if (!setVersion(db, nextVersion)) {
            QSqlQuery rb(db);
            rb.exec(QStringLiteral("ROLLBACK"));
            if (errorOut)
                *errorOut = QStringLiteral("Şema sürümü yazılamadı");
            return false;
        }

        QSqlQuery commit(db);
        if (!execOrFail(commit, QStringLiteral("COMMIT"), errorOut))
            return false;

        version = nextVersion;
    }

    // Göç bitti: yabancı anahtarları geri aç ve hiçbir bağın kopmadığını
    // doğrula. foreign_key_check satır DÖNERSE bozuk bir referans var
    // demektir; bu noktada durmak, bozuk veriyle çalışmaya devam etmekten
    // iyidir — kullanıcının göç öncesi .bak yedeği zaten duruyor.
    {
        QSqlQuery fk(db);
        if (!execOrFail(fk, QStringLiteral("PRAGMA foreign_keys = ON"), errorOut))
            return false;

        QSqlQuery kontrol(db);
        if (kontrol.exec(QStringLiteral("PRAGMA foreign_key_check")) && kontrol.next()) {
            if (errorOut)
                *errorOut = QStringLiteral("Şema göçünden sonra bozuk yabancı anahtar bulundu "
                                            "(tablo: %1). Göç öncesi yedek .bak uzantısıyla "
                                            "duruyor.")
                                .arg(kontrol.value(0).toString());
            return false;
        }
    }

    return true;
}
