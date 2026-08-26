#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "core/db.h"
#include "core/repo_items.h"

namespace {

// Bağlantıyı kapatıp kaldırır; QSqlQuery nesnelerinin bağlantı kapatılmadan
// önce yok edilmiş olması gerekir (bkz. test_migration.cpp'deki not).
void closeAndRemove(const QString &connectionName)
{
    QSqlDatabase::database(connectionName).close();
    QSqlDatabase::removeDatabase(connectionName);
}

} // namespace

class TestRepos : public QObject
{
    Q_OBJECT

private slots:
    void addAssignsId();
    void addDuplicateKodFails();
    void updateChangesFields();
    void setActiveHidesFromDefaultList();
    void listAllOrderedByAd();
    void csvRoundTripPreservesTurkishChars();
    void csvSharedCategoryNameNotDuplicated();
    void csvMalformedRowLeavesDbUnchanged();
};

void TestRepos::addAssignsId()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = QStringLiteral("repo_add");
    QString err;
    QVERIFY2(Db::openAndMigrate(dir.filePath(QStringLiteral("t.db")), &err, conn), qPrintable(err));

    {
        QSqlDatabase db = QSqlDatabase::database(conn);
        Item it;
        it.kod = QStringLiteral("TST-01");
        it.ad = QStringLiteral("Test Kalemi");
        it.birim = QStringLiteral("adet");
        it.varsayilanFiyat = Money::fromString(QStringLiteral("10,00")).value();

        QString addErr;
        QVERIFY2(RepoItems::add(db, it, &addErr), qPrintable(addErr));
        QVERIFY(it.id > 0);
    }
    closeAndRemove(conn);
}

void TestRepos::addDuplicateKodFails()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = QStringLiteral("repo_dup");
    QString err;
    QVERIFY2(Db::openAndMigrate(dir.filePath(QStringLiteral("t.db")), &err, conn), qPrintable(err));

    {
        QSqlDatabase db = QSqlDatabase::database(conn);

        Item birinci;
        birinci.kod = QStringLiteral("AYNI-01");
        birinci.ad = QStringLiteral("Birinci");
        birinci.birim = QStringLiteral("adet");
        QString e1;
        QVERIFY2(RepoItems::add(db, birinci, &e1), qPrintable(e1));

        Item ikinci;
        ikinci.kod = QStringLiteral("AYNI-01"); // aynı kod
        ikinci.ad = QStringLiteral("İkinci");
        ikinci.birim = QStringLiteral("adet");
        QString e2;
        QVERIFY(!RepoItems::add(db, ikinci, &e2));
        QVERIFY(e2.contains(QStringLiteral("zaten kayıtlı")));
        QVERIFY(e2.contains(QStringLiteral("AYNI-01")));
    }
    closeAndRemove(conn);
}

void TestRepos::updateChangesFields()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = QStringLiteral("repo_update");
    QString err;
    QVERIFY2(Db::openAndMigrate(dir.filePath(QStringLiteral("t.db")), &err, conn), qPrintable(err));

    {
        QSqlDatabase db = QSqlDatabase::database(conn);

        Item it;
        it.kod = QStringLiteral("UPD-01");
        it.ad = QStringLiteral("Eski Ad");
        it.birim = QStringLiteral("adet");
        it.varsayilanFiyat = Money::fromString(QStringLiteral("10,00")).value();
        QString addErr;
        QVERIFY2(RepoItems::add(db, it, &addErr), qPrintable(addErr));

        it.ad = QStringLiteral("Yeni Ad");
        it.varsayilanFiyat = Money::fromString(QStringLiteral("25,00")).value();
        QString updErr;
        QVERIFY2(RepoItems::update(db, it, &updErr), qPrintable(updErr));

        const QVector<Item> liste = RepoItems::listAll(db);
        QCOMPARE(liste.size(), 1);
        QCOMPARE(liste.first().ad, QStringLiteral("Yeni Ad"));
        QCOMPARE(liste.first().varsayilanFiyat.toString(), QStringLiteral("25,00"));
    }
    closeAndRemove(conn);
}

void TestRepos::setActiveHidesFromDefaultList()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = QStringLiteral("repo_setactive");
    QString err;
    QVERIFY2(Db::openAndMigrate(dir.filePath(QStringLiteral("t.db")), &err, conn), qPrintable(err));

    {
        QSqlDatabase db = QSqlDatabase::database(conn);

        Item it;
        it.kod = QStringLiteral("PSF-01");
        it.ad = QStringLiteral("Pasife Alınacak");
        it.birim = QStringLiteral("adet");
        QString addErr;
        QVERIFY2(RepoItems::add(db, it, &addErr), qPrintable(addErr));

        QString saErr;
        QVERIFY2(RepoItems::setActive(db, it.id, false, &saErr), qPrintable(saErr));

        QCOMPARE(RepoItems::listAll(db, /*includeInactive=*/false).size(), 0);
        QCOMPARE(RepoItems::listAll(db, /*includeInactive=*/true).size(), 1);
    }
    closeAndRemove(conn);
}

void TestRepos::listAllOrderedByAd()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = QStringLiteral("repo_order");
    QString err;
    QVERIFY2(Db::openAndMigrate(dir.filePath(QStringLiteral("t.db")), &err, conn), qPrintable(err));

    {
        QSqlDatabase db = QSqlDatabase::database(conn);
        for (const QString &ad : {QStringLiteral("Çelik"), QStringLiteral("Ahşap"), QStringLiteral("Boya")}) {
            Item it;
            it.kod = ad;
            it.ad = ad;
            it.birim = QStringLiteral("adet");
            QString e;
            QVERIFY2(RepoItems::add(db, it, &e), qPrintable(e));
        }

        const QVector<Item> liste = RepoItems::listAll(db);
        QCOMPARE(liste.size(), 3);
        QCOMPARE(liste[0].ad, QStringLiteral("Ahşap"));
        QCOMPARE(liste[1].ad, QStringLiteral("Boya"));
        QCOMPARE(liste[2].ad, QStringLiteral("Çelik"));
    }
    closeAndRemove(conn);
}

void TestRepos::csvRoundTripPreservesTurkishChars()
{
    QTemporaryDir dirA, dirB;
    QVERIFY(dirA.isValid() && dirB.isValid());
    const QString connA = QStringLiteral("repo_csv_a");
    const QString connB = QStringLiteral("repo_csv_b");

    QString err;
    QVERIFY2(Db::openAndMigrate(dirA.filePath(QStringLiteral("a.db")), &err, connA), qPrintable(err));
    QVERIFY2(Db::openAndMigrate(dirB.filePath(QStringLiteral("b.db")), &err, connB), qPrintable(err));

    QString csv;
    {
        QSqlDatabase dbA = QSqlDatabase::database(connA);

        Item it1;
        it1.kod = QStringLiteral("ISC-01");
        it1.ad = QStringLiteral("İşçilik");
        it1.birim = QStringLiteral("saat");
        it1.varsayilanFiyat = Money::fromString(QStringLiteral("350,00")).value();
        QString catErr;
        it1.categoryId = 0; // add() ile doğrudan kategori adı verilemez; CSV yolunu test ediyoruz
        QString e1;
        QVERIFY2(RepoItems::add(dbA, it1, &e1), qPrintable(e1));

        // Kategoriyi CSV üzerinden dolaylı test etmek için ikinci kalemi
        // doğrudan importCsv ile ekleyip ardından dışa aktaracağız — bu
        // yüzden burada sadece birinci kalemi elle ekledik.
        QString expErr;
        csv = RepoItems::exportCsv(dbA, &expErr);
        QVERIFY2(!csv.isEmpty(), qPrintable(expErr));
        QVERIFY(csv.contains(QStringLiteral("İşçilik")));
    }
    closeAndRemove(connA);

    {
        QSqlDatabase dbB = QSqlDatabase::database(connB);
        QString impErr;
        QVERIFY2(RepoItems::importCsv(dbB, csv, &impErr), qPrintable(impErr));

        const QVector<Item> liste = RepoItems::listAll(dbB);
        QCOMPARE(liste.size(), 1);
        QCOMPARE(liste.first().kod, QStringLiteral("ISC-01"));
        QCOMPARE(liste.first().ad, QStringLiteral("İşçilik")); // Türkçe karakter korunmuş
        QCOMPARE(liste.first().birim, QStringLiteral("saat"));
        QCOMPARE(liste.first().varsayilanFiyat.toString(), QStringLiteral("350,00"));
    }
    closeAndRemove(connB);
}

void TestRepos::csvSharedCategoryNameNotDuplicated()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = QStringLiteral("repo_csv_cat");
    QString err;
    QVERIFY2(Db::openAndMigrate(dir.filePath(QStringLiteral("t.db")), &err, conn), qPrintable(err));

    {
        QSqlDatabase db = QSqlDatabase::database(conn);
        const QString csv = QStringLiteral(
            "kod,ad,birim,fiyat,kategori\n"
            "K1,Kalem Bir,adet,\"10,00\",İşçilik Kalemleri\n"
            "K2,Kalem İki,adet,\"20,00\",İşçilik Kalemleri\n");

        QString impErr;
        QVERIFY2(RepoItems::importCsv(db, csv, &impErr), qPrintable(impErr));

        QSqlQuery q(db);
        QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM categories WHERE ad = 'İşçilik Kalemleri'")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1); // tek kategori satırı, iki kere oluşturulmamış
    }
    closeAndRemove(conn);
}

void TestRepos::csvMalformedRowLeavesDbUnchanged()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = QStringLiteral("repo_csv_bad");
    QString err;
    QVERIFY2(Db::openAndMigrate(dir.filePath(QStringLiteral("t.db")), &err, conn), qPrintable(err));

    {
        QSqlDatabase db = QSqlDatabase::database(conn);

        Item mevcut;
        mevcut.kod = QStringLiteral("VAR-01");
        mevcut.ad = QStringLiteral("Zaten Var Olan");
        mevcut.birim = QStringLiteral("adet");
        QString addErr;
        QVERIFY2(RepoItems::add(db, mevcut, &addErr), qPrintable(addErr));

        // İkinci satırda "kategori" sütunu eksik (4 sütun, 5 bekleniyor).
        const QString bozukCsv = QStringLiteral(
            "kod,ad,birim,fiyat,kategori\n"
            "K1,Kalem Bir,adet,\"10,00\",Malzeme\n"
            "K2,Kalem İki,adet,\"20,00\"\n");

        QString impErr;
        QVERIFY(!RepoItems::importCsv(db, bozukCsv, &impErr));
        QVERIFY(!impErr.isEmpty());

        // Veritabanı değişmemiş: sadece başlangıçtaki tek kalem var,
        // bozuk CSV'nin ilk (geçerli) satırı bile eklenmemiş.
        const QVector<Item> liste = RepoItems::listAll(db, /*includeInactive=*/true);
        QCOMPARE(liste.size(), 1);
        QCOMPARE(liste.first().kod, QStringLiteral("VAR-01"));
    }
    closeAndRemove(conn);
}

QTEST_MAIN(TestRepos)
#include "test_repos.moc"
