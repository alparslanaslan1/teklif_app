#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

#include "teklif/core/db.h"
#include "teklif/core/repo_quotes.h"

namespace {

QStringList tableNames(QSqlDatabase &db)
{
    QStringList names;
    QSqlQuery q(db);
    q.exec(QStringLiteral(
        "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'"));
    while (q.next())
        names << q.value(0).toString();
    names.sort();
    return names;
}

// Bağlantıyı kapatıp kaldırır. QSqlQuery nesneleri bağlantı kapatılmadan önce
// yok edilmezse Qt "connection still in use" uyarısı basar (zararsız ama
// gürültülü) — bu yüzden testlerde db kullanan her blok kendi süslü parantez
// kapsamında tutulur, buraya sadece kapsamın dışına çıkmış temiz bağlantı gelir.
void closeAndRemove(const QString &connectionName)
{
    QSqlDatabase::database(connectionName).close();
    QSqlDatabase::removeDatabase(connectionName);
}

} // namespace

class TestMigration : public QObject
{
    Q_OBJECT

private slots:
    void newFileGetsCurrentSchema();
    void v0FileIsUpgradedAndBackedUp();
    void alreadyCurrentSkipsMigration();
    void failedStepRollsBackAndLeavesFileIntact();
    void v1FileMovesCustomerDataIntoQuotes();
    void cascadeDeleteRemovesQuoteLines();
    void foreignKeysAreEnforced();
    void oldBackupsArePruned();
};

void TestMigration::newFileGetsCurrentSchema()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("yeni.db"));
    const QString conn = QStringLiteral("test_new");

    QString err;
    QVERIFY2(Db::openAndMigrate(path, &err, conn), qPrintable(err));

    {
        QSqlDatabase db = QSqlDatabase::database(conn);
        QCOMPARE(tableNames(db),
                 (QStringList{"categories", "items", "quote_lines", "quotes", "settings"}));

        QSqlQuery q(db);
        q.exec(QStringLiteral("PRAGMA user_version"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), Db::kSchemaVersion);
    }
    closeAndRemove(conn);

    // Yepyeni dosyada yedek alınmaz.
    QDir d(dir.path());
    QCOMPARE(d.entryList(QStringList{"*.bak-*"}).size(), 0);
}

void TestMigration::v0FileIsUpgradedAndBackedUp()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("eski.db"));

    // v0: boş ama gerçek bir SQLite dosyası (tablo yok, user_version=0).
    {
        QSqlDatabase seed = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("seed_v0"));
        seed.setDatabaseName(path);
        QVERIFY(seed.open());
        {
            QSqlQuery q(seed);
            // Dosyanın gerçekten diske yazılmasını garantiye almak için bir yazma yap.
            QVERIFY(q.exec(QStringLiteral("PRAGMA user_version = 0")));
        }
    }
    closeAndRemove(QStringLiteral("seed_v0"));
    QVERIFY(QFile::exists(path));

    const QString conn = QStringLiteral("test_v0");
    QString err;
    QVERIFY2(Db::openAndMigrate(path, &err, conn), qPrintable(err));

    {
        QSqlDatabase db = QSqlDatabase::database(conn);
        QCOMPARE(tableNames(db),
                 (QStringList{"categories", "items", "quote_lines", "quotes", "settings"}));
    }
    closeAndRemove(conn);

    QDir d(dir.path());
    QCOMPARE(d.entryList(QStringList{"eski.db.bak-*"}).size(), 1);
}

void TestMigration::alreadyCurrentSkipsMigration()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("guncel.db"));

    QString err;
    QVERIFY2(Db::openAndMigrate(path, &err, QStringLiteral("test_cur1")), qPrintable(err));
    closeAndRemove(QStringLiteral("test_cur1"));

    QDir d(dir.path());
    const int bakOnce = d.entryList(QStringList{"*.bak-*"}).size();

    QVERIFY2(Db::openAndMigrate(path, &err, QStringLiteral("test_cur2")), qPrintable(err));
    const int bakTwice = d.entryList(QStringList{"*.bak-*"}).size();

    QCOMPARE(bakTwice, bakOnce);

    closeAndRemove(QStringLiteral("test_cur2"));
}

void TestMigration::failedStepRollsBackAndLeavesFileIntact()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("bozuk.db"));

    // v0 dosyası, ama migration'ın oluşturmak isteyeceği "settings" tablosu
    // uyumsuz bir şemayla önceden var -> CREATE TABLE çakışır, migration
    // başarısız olmalı.
    {
        QSqlDatabase seed = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("seed_fail"));
        seed.setDatabaseName(path);
        QVERIFY(seed.open());
        {
            QSqlQuery q(seed);
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE settings (x INTEGER)")));
            QVERIFY(q.exec(QStringLiteral("INSERT INTO settings (x) VALUES (42)")));
        }
    }
    closeAndRemove(QStringLiteral("seed_fail"));

    const QString conn = QStringLiteral("test_fail");
    QString err;
    QVERIFY(!Db::openAndMigrate(path, &err, conn));
    QVERIFY(!err.isEmpty());
    closeAndRemove(conn);

    // Başarısız migration'dan önce yedek zaten alınmıştı -> ekstra güvenlik ağı.
    QDir d(dir.path());
    QCOMPARE(d.entryList(QStringList{"bozuk.db.bak-*"}).size(), 1);

    // Asıl dosya bozulmamış: user_version hâlâ 0, çakışan tablo orijinal
    // haliyle duruyor.
    const QString checkConn = QStringLiteral("test_fail_check");
    {
        QSqlDatabase check = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), checkConn);
        check.setDatabaseName(path);
        QVERIFY(check.open());

        {
            QSqlQuery qv(check);
            qv.exec(QStringLiteral("PRAGMA user_version"));
            QVERIFY(qv.next());
            QCOMPARE(qv.value(0).toInt(), 0);
        }
        {
            QSqlQuery qs(check);
            QVERIFY(qs.exec(QStringLiteral("SELECT x FROM settings")));
            QVERIFY(qs.next());
            QCOMPARE(qs.value(0).toInt(), 42);
        }
    }
    closeAndRemove(checkConn);
}

// v1 -> v2: musteri bilgisi ayri tablodan teklifin icine tasinir.
//
// EN RISKLI GOC BU: quotes tablosu yeniden kuruluyor ve customers
// dusuruluyor. Bir alan yanlis esleseydi ya da JOIN INNER olsaydi, eski
// tekliflerin musterisi sessizce kaybolurdu — ve bu ancak eski bir teklif
// yazdirildiginda fark edilirdi.
void TestMigration::v1FileMovesCustomerDataIntoQuotes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("v1.db"));

    // --- Elle bir v1 dosyasi kur -------------------------------------
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                     QStringLiteral("seed_v1"));
        db.setDatabaseName(path);
        QVERIFY(db.open());
        QSqlQuery q(db);
        QVERIFY(q.exec(QStringLiteral(
            "CREATE TABLE customers (id INTEGER PRIMARY KEY, unvan TEXT NOT NULL, yetkili TEXT, "
            "telefon TEXT, email TEXT, adres TEXT, vergi_dairesi TEXT, vergi_no TEXT, "
            "notlar TEXT, aktif INTEGER NOT NULL DEFAULT 1, "
            "olusturma TEXT NOT NULL DEFAULT (datetime('now')))")));
        QVERIFY(q.exec(QStringLiteral(
            "CREATE TABLE quotes (id INTEGER PRIMARY KEY, teklif_no TEXT NOT NULL UNIQUE, "
            "customer_id INTEGER NOT NULL REFERENCES customers(id) ON DELETE RESTRICT, "
            "tarih TEXT NOT NULL, gecerlilik_gun INTEGER NOT NULL DEFAULT 15, proje_basligi TEXT, "
            "proje_notu TEXT, durum TEXT NOT NULL DEFAULT 'Taslak', sartlar_metni TEXT, "
            "ara_toplam INTEGER NOT NULL DEFAULT 0, kdv_orani INTEGER NOT NULL DEFAULT 0, "
            "kdv_tutari INTEGER NOT NULL DEFAULT 0, genel_toplam INTEGER NOT NULL DEFAULT 0, "
            "olusturma TEXT NOT NULL DEFAULT (datetime('now')), "
            "guncelleme TEXT NOT NULL DEFAULT (datetime('now')))")));
        QVERIFY(q.exec(QStringLiteral(
            "CREATE TABLE quote_lines (id INTEGER PRIMARY KEY, "
            "quote_id INTEGER NOT NULL REFERENCES quotes(id) ON DELETE CASCADE, "
            "sira INTEGER NOT NULL, aciklama TEXT NOT NULL, birim TEXT NOT NULL, "
            "miktar REAL NOT NULL, birim_fiyat INTEGER NOT NULL, satir_notu TEXT, "
            "tutar INTEGER NOT NULL)")));
        QVERIFY(q.exec(QStringLiteral("CREATE TABLE settings (key TEXT PRIMARY KEY, value TEXT)")));
        QVERIFY(q.exec(QStringLiteral("CREATE TABLE categories (id INTEGER PRIMARY KEY, ad TEXT NOT NULL UNIQUE)")));
        QVERIFY(q.exec(QStringLiteral(
            "CREATE TABLE items (id INTEGER PRIMARY KEY, kod TEXT NOT NULL UNIQUE, ad TEXT NOT NULL, "
            "birim TEXT NOT NULL, varsayilan_fiyat INTEGER NOT NULL DEFAULT 0, "
            "category_id INTEGER REFERENCES categories(id) ON DELETE SET NULL, "
            "aktif INTEGER NOT NULL DEFAULT 1, guncelleme TEXT NOT NULL DEFAULT (datetime('now')))")));

        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO customers (id, unvan, yetkili, telefon, adres, vergi_dairesi, vergi_no) "
            "VALUES (1, 'Şükrü Çelik İnşaat', 'Şükrü Çelik', '05321112233', "
            "'İncilli Mah. Karasu', 'Karasu', '1234567890')")));
        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO quotes (id, teklif_no, customer_id, tarih, genel_toplam) "
            "VALUES (7, '000042', 1, '2025-03-10', 50000)")));
        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO quote_lines (quote_id, sira, aciklama, birim, miktar, birim_fiyat, tutar) "
            "VALUES (7, 1, 'Kolon boyası', 'm2', 5, 10000, 50000)")));
        QVERIFY(q.exec(QStringLiteral("PRAGMA user_version = 1")));
        db.close();
    }
    closeAndRemove(QStringLiteral("seed_v1"));

    // --- Ac: goc calismali -------------------------------------------
    const QString conn = QStringLiteral("test_v1");
    QString err;
    QVERIFY2(Db::openAndMigrate(path, &err, conn), qPrintable(err));

    {
        QSqlDatabase db = QSqlDatabase::database(conn);

        // customers tablosu artik yok.
        QVERIFY(!tableNames(db).contains(QStringLiteral("customers")));

        RepoQuotes repo(db);
        const auto teklif = repo.get(7, &err);
        QVERIFY2(teklif.has_value(), qPrintable(err));

        // Musteri bilgisi teklife tasinmis olmali.
        QCOMPARE(teklif->musteri.unvan, QStringLiteral("Şükrü Çelik İnşaat"));
        QCOMPARE(teklif->musteri.yetkili, QStringLiteral("Şükrü Çelik"));
        QCOMPARE(teklif->musteri.telefon, QStringLiteral("05321112233"));
        QCOMPARE(teklif->musteri.adres, QStringLiteral("İncilli Mah. Karasu"));
        QCOMPARE(teklif->musteri.vergiDairesi, QStringLiteral("Karasu"));
        QCOMPARE(teklif->musteri.vergiNo, QStringLiteral("1234567890"));

        // Teklifin kendisi ve satirlari da yerinde.
        QCOMPARE(teklif->teklifNo, QStringLiteral("000042"));
        QCOMPARE(teklif->genelToplam.toString(), QStringLiteral("500,00"));
        QCOMPARE(teklif->satirlar.size(), 1);
        QCOMPARE(teklif->satirlar.first().aciklama, QStringLiteral("Kolon boyası"));

        QSqlQuery v(db);
        QVERIFY(v.exec(QStringLiteral("PRAGMA user_version")) && v.next());
        QCOMPARE(v.value(0).toInt(), Db::kSchemaVersion);
    }
    closeAndRemove(conn);

    // Goc oncesi yedek alinmis olmali.
    QDir d(dir.path());
    QCOMPARE(d.entryList(QStringList{"v1.db.bak-*"}).size(), 1);
}

void TestMigration::cascadeDeleteRemovesQuoteLines()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("cascade.db"));
    const QString conn = QStringLiteral("test_cascade");

    QString err;
    QVERIFY2(Db::openAndMigrate(path, &err, conn), qPrintable(err));

    {
        QSqlDatabase db = QSqlDatabase::database(conn);
        QSqlQuery q(db);

        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO quotes (teklif_no, musteri_unvan, tarih) "
            "VALUES ('000001', 'Test Müşteri', '2026-08-25')")));
        const qint64 quoteId = q.lastInsertId().toLongLong();

        QVERIFY(q.exec(QStringLiteral(
                           "INSERT INTO quote_lines (quote_id, sira, aciklama, birim, miktar, birim_fiyat, tutar) "
                           "VALUES (%1, 1, 'Test kalem', 'adet', 1, 1000, 1000)")
                           .arg(quoteId)));

        QSqlQuery before(db);
        before.exec(QStringLiteral("SELECT COUNT(*) FROM quote_lines WHERE quote_id=%1").arg(quoteId));
        before.next();
        QCOMPARE(before.value(0).toInt(), 1);

        QVERIFY(q.exec(QStringLiteral("DELETE FROM quotes WHERE id=%1").arg(quoteId)));

        QSqlQuery after(db);
        after.exec(QStringLiteral("SELECT COUNT(*) FROM quote_lines WHERE quote_id=%1").arg(quoteId));
        after.next();
        QCOMPARE(after.value(0).toInt(), 0);
    }
    closeAndRemove(conn);
}

void TestMigration::foreignKeysAreEnforced()
{
    // PRAGMA foreign_keys=ON her bağlantıda açıkça ayarlanmazsa cascade de,
    // bütünlük kontrolü de sessizce çalışmaz. Var olmayan bir quote_id'ye
    // satır eklemeye çalışmak reddedilmeli.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("fk.db"));
    const QString conn = QStringLiteral("test_fk");

    QString err;
    QVERIFY2(Db::openAndMigrate(path, &err, conn), qPrintable(err));

    {
        QSqlDatabase db = QSqlDatabase::database(conn);
        QSqlQuery q(db);
        const bool ok = q.exec(QStringLiteral(
            "INSERT INTO quote_lines (quote_id, sira, aciklama, birim, miktar, birim_fiyat, tutar) "
            "VALUES (999999, 1, 'Yetim satır', 'adet', 1, 1000, 1000)"));
        QVERIFY(!ok);
    }
    closeAndRemove(conn);
}

void TestMigration::oldBackupsArePruned()
{
    // Her goc bir .bak birakiyordu ve hicbiri silinmiyordu; zamanla disk
    // doluyordu. Artik en yeni kSaklananYedek tanesi kaliyor.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("teklif.db"));

    // Gercek bir v1 dosyasi olustur.
    {
        QString err;
        const QString conn = QStringLiteral("prune_seed");
        QVERIFY2(Db::openAndMigrate(path, &err, conn), qPrintable(err));
        closeAndRemove(conn);
    }

    // Elle, gecmise ait fazladan yedekler birak (zaman damgasi sirali).
    QDir d(dir.path());
    for (int i = 0; i < Db::kSaklananYedek + 4; ++i) {
        const QString sahte = path + QStringLiteral(".bak-2020010%1-000000").arg(i);
        QFile f(sahte);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("eski yedek");
        f.close();
    }
    QVERIFY(d.entryList(QStringList{"teklif.db.bak-*"}).size() > Db::kSaklananYedek);

    // Budama yeni bir yedek alinirken calisir.
    QString err;
    Db::backupFile(path, &err);

    const QStringList kalanlar = d.entryList(QStringList{"teklif.db.bak-*"});
    QVERIFY2(kalanlar.size() <= Db::kSaklananYedek,
             qPrintable(QStringLiteral("%1 yedek kaldi, en fazla %2 olmali")
                            .arg(kalanlar.size()).arg(Db::kSaklananYedek)));

    // EN YENILER kalmali. Dosya adlari zaman damgali oldugu icin alfabetik
    // sira kronolojik siradir; silinenler listenin BASINDAN alinir.
    // (Ilk yazdigim iddia "hicbir 2020 yedegi kalmamali" idi ve YANLISTI:
    // 9 sahte + 1 gercek = 10 dosyadan en yeni 5'i kalir, bunlarin bir kismi
    // hala 2020 tarihlidir. Dogru iddia: EN ESKILER gitmis olmali.)
    QVERIFY2(!kalanlar.contains(QStringLiteral("teklif.db.bak-20200100-000000")),
             "en eski yedek silinmemis");
    QVERIFY2(!kalanlar.contains(QStringLiteral("teklif.db.bak-20200101-000000")),
             "ikinci en eski yedek silinmemis");

    // Yeni alinan yedek mutlaka durmali - budama onu silmemeli.
    bool yeniVar = false;
    for (const QString &ad : kalanlar) {
        if (!ad.contains(QStringLiteral("bak-2020")))
            yeniVar = true;
    }
    QVERIFY2(yeniVar, "yeni alinan yedek budamada silinmis");
}

QTEST_MAIN(TestMigration)
#include "test_migration.moc"
