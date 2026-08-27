#include "db.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace {


// ═══ execOrFail() ═════════════════════════════════════════════════════════
// NE YAPAR : SQL çalıştırır; başarısızsa SQLite'ın hata METNİNİ errorOut'a
//            yazıp false döner. Tekrar eden if/lastError kalıbını tek yere alır.
//
// ADIM ADIM:
//   1) q.exec(sql) -> true ise hiçbir şey yapmadan true döner.
//   2) false ise q.lastError().text() alınır (sürücü + veritabanı mesajı
//      birleşik gelir) ve errorOut != nullptr ise oraya yazılır.
//
// DEBUG    : Migration patlıyorsa ilk bakılacak yer burasıdır. Geçici olarak
//            şunu ekleyin, hangi ifadenin öldüğünü anında görürsünüz:
//              qDebug() << "SQL FAIL:" << sql << "->" << q.lastError().text();
//            Sık görülen mesajlar:
//              "table X already exists"  -> migration iki kez çalışıyor
//              "no such table: X"        -> şema sürümü yanlış hesaplanmış
//              "database is locked"      -> başka bir bağlantı açık kalmış
bool execOrFail(QSqlQuery &q, const QString &sql, QString *errorOut)
{
    if (!q.exec(sql)) {
        if (errorOut)
            *errorOut = q.lastError().text();
        return false;
    }
    return true;
}

} // namespace


// ═══ Db::defaultPath() ════════════════════════════════════════════════════
// NE YAPAR : Kullanıcının veri klasöründeki varsayılan .db yolunu üretir.
//            Programın kurulu olduğu klasöre YAZILMAZ — güncellemede exe
//            değişse bile veri yerinde kalsın diye.
//
// ADIM ADIM:
//   1) QStandardPaths::writableLocation(AppDataLocation) sorulur.
//      Windows: C:/Users/<ad>/AppData/Roaming/<Org>/<Uygulama>
//      Linux  : ~/.local/share/<Org>/<Uygulama>
//   2) Boş dönerse ~/.teklif klasörüne düşülür.
//   3) Sonuna "/teklif.db" eklenir.
//
// ÖN KOŞUL : 1. adımın çalışması için QCoreApplication::setOrganizationName()
//            ve setApplicationName() DAHA ÖNCE çağrılmış olmalıdır. main.cpp
//            bunları QApplication'dan bile önce ayarlar.
//
// DEBUG    : "Veritabanı açılamadı" hatası alıyorsanız İLK İŞ yolu bastırmaktır:
//              qDebug() << Db::defaultPath();
//            • Yol ~/.teklif ile başlıyorsa 1. adım boş dönmüştür -> org/app
//              adı ayarlanmamış demektir.
//            • Yol doğru ama dosya oluşmuyorsa klasör yazma izni yoktur.
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


// ═══ Db::currentVersion() ═════════════════════════════════════════════════
// NE YAPAR : Dosyanın şema sürümünü okur (SQLite'ın yerleşik user_version
//            alanı — ayrı bir sürüm tablosu tutulmuyor).
//
// ADIM ADIM:
//   1) "PRAGMA user_version" çalıştırılır.
//   2) exec() başarısızsa VEYA next() satır vermezse 0 döner.
//      DİKKAT: burada hata ile "sürüm gerçekten 0" AYIRT EDİLMEZ. Her iki
//      durumda da 0 dönünce openAndMigrate v0 sanıp migration başlatır.
//   3) Değilse ilk sütun int olarak döner.
//
// DEBUG    : Migration hiç çalışmıyorsa ya da her açılışta yeniden çalışıyorsa:
//              qDebug() << "user_version =" << currentVersion(db)
//                       << "beklenen:" << kSchemaVersion;
//            Yeni oluşturulmuş bir dosyada 0, migration sonrası 1 olmalı.
int Db::currentVersion(QSqlDatabase &db)
{
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA user_version")) || !q.next())
        return 0;
    return q.value(0).toInt();
}


// ═══ Db::setVersion() ═════════════════════════════════════════════════════
// NE YAPAR : user_version alanını yazar. Migration adımı BAŞARILI olduktan
//            sonra, COMMIT'ten ÖNCE çağrılır.
//
// NEDEN arg() : PRAGMA'nın değeri bir SAYI LİTERALİDİR; bind edilemez
//            ("PRAGMA user_version = ?" SQLite'ta çalışmaz). Bu yüzden metin
//            birleştirmesi zorunludur. Parametre int olduğu için SQL enjeksiyon
//            riski YOKTUR.
//
// DEBUG    : Bu false dönerse openAndMigrate ROLLBACK yapar ve dosya eski
//            haliyle kalır. Sürüm ilerlemiyorsa buraya breakpoint koyun:
//              qDebug() << version << q.lastError().text();
bool Db::setVersion(QSqlDatabase &db, int version)
{
    QSqlQuery q(db);
    // PRAGMA user_version bir tamsayı literalidir, parametre bind edilemez.
    return q.exec(QStringLiteral("PRAGMA user_version = %1").arg(version));
}


// ═══ Db::backupFile() ═════════════════════════════════════════════════════
// NE YAPAR : Migration'dan ÖNCE veritabanının ham kopyasını alır:
//            <yol>/teklif.db.bak-YYYYAAGG-SSDDss
//
// ADIM ADIM:
//   1) Zaman damgası üretilir (saniye çözünürlüğünde).
//   2) Aynı adlı yedek ZATEN VARSA hata döner ve migration DURUR.
//      Üzerine yazıp eski yedeği kaybetmemek için bilinçli bir seçim.
//      (Aynı saniyede iki migration denemesi gibi uç bir durum.)
//   3) QFile::copy ile kopyalanır.
//
// ÖN KOŞUL : Çağıran taraf bağlantıyı KAPATMIŞ olmalıdır; açık bir bağlantıda
//            kopya tutarsız olabilir. openAndMigrate bunu doğru sırayla yapar:
//            close -> removeDatabase -> backupFile -> tekrar aç.
//
// DEBUG    : Migration "Yedek alınamadı" ile duruyorsa:
//              qDebug() << path << QFileInfo(path).dir().absolutePath()
//                       << QFileInfo(path).dir().exists();
//            Genelde klasör yazma izni yoktur ya da disk dolmuştur.
//
// EKSİK    : (a) .bak dosyaları HİÇ TEMİZLENMEZ, her migration'da birikir.
//            (b) Sadece .db kopyalanır; -wal / -shm / -journal kopyalanmaz.
//                Şu an WAL kapalı olduğu için sorun değil, ama WAL'ı açarsanız
//                bu yedek EKSİK olur.
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
    return true;
}


// ═══ Db::createV1Schema() ═════════════════════════════════════════════════
// NE YAPAR : v1 şemasını sıfırdan kurar (6 tablo + 3 indeks). SADECE
//            migrateStep(fromVersion = 0) tarafından çağrılır.
//
// TABLOLAR ve ARALARINDAKİ BAĞ:
//   settings    key/value — teklif no sayacı burada tutulur
//   customers   müşteriler
//   categories  kalem kategorileri (ad UNIQUE)
//   items       katalog; category_id -> categories  (ON DELETE SET NULL)
//   quotes      teklif başlığı; customer_id -> customers (ON DELETE RESTRICT)
//               yani müşterisi olan teklif varken müşteri SİLİNEMEZ
//   quote_lines teklif satırları; quote_id -> quotes (ON DELETE CASCADE)
//               yani teklif silinince satırları da otomatik silinir
//
// ADIM ADIM: `statements` listesi SIRAYLA çalıştırılır. Sıra önemlidir —
//   categories, items'tan ÖNCE gelmeli (FK hedefi). Biri patlarsa hemen
//   false döner; çağıran (openAndMigrate) ROLLBACK eder.
//
// DEBUG    : Şema kurulmuyorsa hangi ifadede kaldığını görmek için döngüye
//            geçici bir sayaç/çıktı koyun:
//              qDebug() << "stmt" << statements.indexOf(sql) << sql.left(60);
//            Kurulduktan sonra doğrulama:
//              SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;
//            Beklenen: categories, customers, items, quote_lines, quotes, settings
//
// NOT      : FK'lerin GERÇEKTEN uygulanması için her bağlantıda
//            "PRAGMA foreign_keys = ON" gerekir — openAndMigrate bunu yapar.
//            Unutulursa CASCADE/RESTRICT sessizce ETKİSİZ kalır.
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


// ═══ Db::migrateStep() ════════════════════════════════════════════════════
// NE YAPAR : TEK bir sürüm sıçramasını uygular. fromVersion = MEVCUT sürümdür;
//            işlem başarılıysa dosya fromVersion + 1 olur.
//
// ŞU ANKİ HARİTA:
//   case 0 -> createV1Schema()   (v0/boş dosya -> v1)
//   default-> "Bilinmeyen şema sürümü" hatası
//
// YENİ MİGRATION EKLERKEN (3 adım, sırayı bozmayın):
//   1) db.h içindeki kSchemaVersion'ı BİR artırın (1 -> 2)
//   2) Buraya "case 1:" ekleyin (v1'den v2'ye taşıyan kod)
//   3) ESKİ CASE'LERİ ASLA SİLMEYİN — kullanıcıda hâlâ o sürümden dosya olabilir
//
// DEBUG    : "Bilinmeyen şema sürümü: N" hatası iki anlama gelir:
//            • N > kSchemaVersion  -> dosya programdan YENİ (kullanıcı eski
//              sürüme geri dönmüş). Bu durumda migration yapılmamalı, kullanıcı
//              uyarılmalı.
//            • N için case yazılmamış -> yukarıdaki 2. adım atlanmış.
//            Kontrol:  qDebug() << fromVersion << Db::kSchemaVersion;
bool Db::migrateStep(QSqlDatabase &db, int fromVersion, QString *errorOut)
{
    switch (fromVersion) {
    case 0:
        return createV1Schema(db, errorOut);
    // v1 -> v2 gerektiğinde buraya yeni bir "case 1:" eklenecek.
    // Yukarıdaki case 0 SİLİNMEYECEK: kullanıcıda hâlâ v0 dosya olabilir.
    default:
        if (errorOut)
            *errorOut = QStringLiteral("Bilinmeyen şema sürümü: %1").arg(fromVersion);
        return false;
    }
}


// ═══ Db::openAndMigrate() ═════════════════════════════════════════════════
// NE YAPAR : Programın veritabanına giriş kapısı. Klasörü oluşturur, bağlantıyı
//            açar, gerekiyorsa yedek alıp şemayı göçürür.
//            main.cpp'de ilk çağrılan iş mantığı fonksiyonudur.
//
// ADIM ADIM (hata ayıklarken bu 6 aşamayı sırayla kontrol edin):
//
//   [1] KLASÖR    : QDir().mkpath(...) ile üst klasör garantiye alınır.
//                   isNewFile = dosya HENÜZ YOK MU? (yedek kararı buna bağlı)
//
//   [2] AÇ        : openConnection() lambda'sı
//                   a) addDatabase("QSQLITE", connectionName)
//                      DİKKAT: aynı ad zaten kayıtlıysa Qt uyarı basıp eskisini
//                      DEĞİŞTİRİR. "duplicate connection name" uyarısı
//                      görüyorsanız openAndMigrate iki kez çağrılmıştır.
//                   b) db.open() -> başarısızsa lastError ile çıkılır.
//                   c) PRAGMA foreign_keys = ON  <-- HER bağlantıda ŞART.
//                      SQLite'ta varsayılan KAPALIDIR; bu satır olmadan
//                      CASCADE/RESTRICT hiçbir iş yapmaz.
//                      (Bu exec'in sonucu KONTROL EDİLMİYOR.)
//
//   [3] SÜRÜM     : currentVersion(db) >= kSchemaVersion ise HEMEN true döner.
//                   Yedek alınmaz, migration çalışmaz. Normal açılışta akış
//                   BURADA biter — 4-6. adımlara hiç girmez. Breakpoint'iniz
//                   tetiklenmiyorsa sebebi budur.
//
//   [4] YEDEK     : Sadece isNewFile == false ise (var olan dosya yükseltiliyor):
//                   close -> removeDatabase -> backupFile -> TEKRAR AÇ.
//                   Bu sıra önemli: dosya kilitliyken kopyalamamak için.
//
//   [5] DÖNGÜ     : version < kSchemaVersion olduğu sürece her tur:
//                   a) BEGIN IMMEDIATE   (dönüş değeri KONTROL EDİLMİYOR!)
//                   b) migrateStep(version) -> başarısızsa ROLLBACK + çık
//                   c) setVersion(version + 1) -> başarısızsa ROLLBACK + çık
//                   d) COMMIT
//                   e) version = version + 1
//                   SQLite'ta DDL de transaction'a dahildir; yani yarım kalan
//                   bir CREATE TABLE geri alınır ve dosya bozulmaz.
//
//   [6] BİTİŞ     : version == kSchemaVersion -> true.
//
// DEBUG    : Aşamaları görmek için başa şunu koyun:
//              qDebug() << "path" << path << "yeni mi" << !QFile::exists(path);
//            Döngü içinde:
//              qDebug() << "migrate" << version << "->" << version + 1;
//            Testlerde her test kendi connectionName'ini kullanır; iki test
//            aynı adı kullanırsa biri diğerinin bağlantısını kapatır —
//            "database is locked" veya "driver not loaded" hatalarının en sık
//            sebebi budur.
//
// TUZAK    : [5a]'daki BEGIN'in sonucu okunmuyor. Dışarıda AÇIK bir
//            transaction varken BEGIN sessizce başarısız olur; sonra hata
//            durumunda çalışan ROLLBACK DIŞTAKİ transaction'ı geri alır.
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

        if (!backupFile(path, errorOut))
            return false;

        if (!openConnection())
            return false;
    }

    QSqlDatabase db = QSqlDatabase::database(connectionName);
    int version = currentVersion(db);

    while (version < kSchemaVersion) {
        QSqlQuery tx(db);
        tx.exec(QStringLiteral("BEGIN IMMEDIATE"));

        if (!migrateStep(db, version, errorOut)) {
            QSqlQuery rb(db);
            rb.exec(QStringLiteral("ROLLBACK"));
            return false;
        }

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

    return true;
}
