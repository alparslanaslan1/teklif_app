#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "teklif/core/db.h"
#include "teklif/core/repo_items.h"
#include "teklif/core/repo_quotes.h"

namespace {

void closeAndRemove(const QString &connectionName)
{
    QSqlDatabase::database(connectionName).close();
    QSqlDatabase::removeDatabase(connectionName);
}

QuoteLine mkLine(int sira, const QString &aciklama, const QString &birim, double miktar, qint64 fiyatKurus)
{
    QuoteLine l;
    l.sira = sira;
    l.aciklama = aciklama;
    l.birim = birim;
    l.miktar = miktar;
    l.birimFiyat = Money(fiyatKurus);
    l.tutar = Money(static_cast<qint64>(miktar * fiyatKurus));
    return l;
}

} // namespace

class TestRepoQuotes : public QObject
{
    Q_OBJECT

private slots:
    void addAssignsSequentialNumbers();
    void addPersistsLinesAndTotals();
    void updateReplacesLines();
    void priceIsCopiedNotReferenced();
    void sequenceSurvivesAcrossYearBoundary();
    void sequenceWraps7Digits();
    void rapidSequentialAddsNeverDuplicate();
};

void TestRepoQuotes::addAssignsSequentialNumbers()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = QStringLiteral("q_seq");
    QString err;
    QVERIFY2(Db::openAndMigrate(dir.filePath(QStringLiteral("t.db")), &err, conn), qPrintable(err));

    {
        QSqlDatabase db = QSqlDatabase::database(conn);
        const QString musteriUnvani = QStringLiteral("Test Müşteri");
        QString e;

        Quote q1;
        q1.musteri.unvan = musteriUnvani;
        q1.tarih = QDate(2026, 8, 25);
        QVERIFY2(RepoQuotes(db).add(q1, &e), qPrintable(e));
        QCOMPARE(q1.teklifNo, QStringLiteral("000001"));

        Quote q2;
        q2.musteri.unvan = musteriUnvani;
        q2.tarih = QDate(2026, 8, 25);
        QVERIFY2(RepoQuotes(db).add(q2, &e), qPrintable(e));
        QCOMPARE(q2.teklifNo, QStringLiteral("000002"));
    }
    closeAndRemove(conn);
}

void TestRepoQuotes::addPersistsLinesAndTotals()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = QStringLiteral("q_persist");
    QString err;
    QVERIFY2(Db::openAndMigrate(dir.filePath(QStringLiteral("t.db")), &err, conn), qPrintable(err));

    {
        QSqlDatabase db = QSqlDatabase::database(conn);
        const QString musteriUnvani = QStringLiteral("Test Müşteri");
        QString e;

        Quote q;
        q.musteri.unvan = musteriUnvani;
        q.tarih = QDate(2026, 8, 25);
        q.kdvOraniYuzde = 20;
        q.araToplam = Money::fromString(QStringLiteral("10.205,00")).value();
        q.kdvTutari = Money::fromString(QStringLiteral("2.041,00")).value();
        q.genelToplam = Money::fromString(QStringLiteral("12.246,00")).value();
        q.satirlar = {
            mkLine(1, QStringLiteral("Alçıpan Levha"), QStringLiteral("adet"), 24, 18000),
            mkLine(2, QStringLiteral("İşçilik"), QStringLiteral("saat"), 16, 35000),
        };
        QVERIFY2(RepoQuotes(db).add(q, &e), qPrintable(e));
        QVERIFY(q.id > 0);

        const auto okundu = RepoQuotes(db).get(q.id, &e);
        QVERIFY2(okundu.has_value(), qPrintable(e));
        QCOMPARE(okundu->teklifNo, q.teklifNo);
        QCOMPARE(okundu->musteri.unvan, musteriUnvani);
        QCOMPARE(okundu->kdvOraniYuzde, 20);
        QCOMPARE(okundu->araToplam.toString(), QStringLiteral("10.205,00"));
        QCOMPARE(okundu->kdvTutari.toString(), QStringLiteral("2.041,00"));
        QCOMPARE(okundu->genelToplam.toString(), QStringLiteral("12.246,00"));

        QCOMPARE(okundu->satirlar.size(), 2);
        QCOMPARE(okundu->satirlar[0].aciklama, QStringLiteral("Alçıpan Levha"));
        QCOMPARE(okundu->satirlar[0].sira, 1);
        QCOMPARE(okundu->satirlar[0].miktar, 24.0);
        QCOMPARE(okundu->satirlar[0].birimFiyat.toString(), QStringLiteral("180,00"));
        QCOMPARE(okundu->satirlar[1].aciklama, QStringLiteral("İşçilik"));
        QCOMPARE(okundu->satirlar[1].sira, 2);
    }
    closeAndRemove(conn);
}

void TestRepoQuotes::updateReplacesLines()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = QStringLiteral("q_update");
    QString err;
    QVERIFY2(Db::openAndMigrate(dir.filePath(QStringLiteral("t.db")), &err, conn), qPrintable(err));

    {
        QSqlDatabase db = QSqlDatabase::database(conn);
        const QString musteriUnvani = QStringLiteral("Test Müşteri");
        QString e;

        Quote q;
        q.musteri.unvan = musteriUnvani;
        q.tarih = QDate(2026, 8, 25);
        q.satirlar = {mkLine(1, QStringLiteral("Eski Satır"), QStringLiteral("adet"), 1, 1000)};
        QVERIFY2(RepoQuotes(db).add(q, &e), qPrintable(e));

        q.satirlar = {
            mkLine(1, QStringLiteral("Yeni Satır 1"), QStringLiteral("adet"), 2, 2000),
            mkLine(2, QStringLiteral("Yeni Satır 2"), QStringLiteral("adet"), 3, 3000),
        };
        QVERIFY2(RepoQuotes(db).update(q, &e), qPrintable(e));

        const auto okundu = RepoQuotes(db).get(q.id, &e);
        QVERIFY2(okundu.has_value(), qPrintable(e));
        QCOMPARE(okundu->satirlar.size(), 2);
        QCOMPARE(okundu->satirlar[0].aciklama, QStringLiteral("Yeni Satır 1"));
        QCOMPARE(okundu->satirlar[1].aciklama, QStringLiteral("Yeni Satır 2"));
    }
    closeAndRemove(conn);
}

void TestRepoQuotes::priceIsCopiedNotReferenced()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = QStringLiteral("q_copy");
    QString err;
    QVERIFY2(Db::openAndMigrate(dir.filePath(QStringLiteral("t.db")), &err, conn), qPrintable(err));

    {
        QSqlDatabase db = QSqlDatabase::database(conn);

        Item it;
        it.kod = QStringLiteral("ISC-01");
        it.ad = QStringLiteral("İşçilik");
        it.birim = QStringLiteral("saat");
        it.varsayilanFiyat = Money::fromString(QStringLiteral("350,00")).value();
        QString e;
        QVERIFY2(RepoItems(db).add(it, &e), qPrintable(e));

        Quote q;
        q.musteri.unvan = QStringLiteral("Test Müşteri");
        q.tarih = QDate(2026, 8, 25);
        q.satirlar = {mkLine(1, it.ad, it.birim, 10, it.varsayilanFiyat.kurus())};
        QVERIFY2(RepoQuotes(db).add(q, &e), qPrintable(e));

        // Katalogdaki fiyatı değiştir.
        it.varsayilanFiyat = Money::fromString(QStringLiteral("500,00")).value();
        QVERIFY2(RepoItems(db).update(it, &e), qPrintable(e));

        // Teklifi tekrar oku: satırdaki fiyat ESKİ haliyle durmalı.
        const auto okundu = RepoQuotes(db).get(q.id, &e);
        QVERIFY2(okundu.has_value(), qPrintable(e));
        QCOMPARE(okundu->satirlar.first().birimFiyat.toString(), QStringLiteral("350,00"));
    }
    closeAndRemove(conn);
}

void TestRepoQuotes::sequenceSurvivesAcrossYearBoundary()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = QStringLiteral("q_year");
    QString err;
    QVERIFY2(Db::openAndMigrate(dir.filePath(QStringLiteral("t.db")), &err, conn), qPrintable(err));

    {
        QSqlDatabase db = QSqlDatabase::database(conn);
        const QString musteriUnvani = QStringLiteral("Test Müşteri");
        QString e;

        Quote q1;
        q1.musteri.unvan = musteriUnvani;
        q1.tarih = QDate(2026, 12, 31);
        QVERIFY2(RepoQuotes(db).add(q1, &e), qPrintable(e));

        Quote q2;
        q2.musteri.unvan = musteriUnvani;
        q2.tarih = QDate(2027, 1, 1); // yıl değişti
        QVERIFY2(RepoQuotes(db).add(q2, &e), qPrintable(e));

        // Sayaç sıfırlanmadı, artmaya devam etti.
        QCOMPARE(q1.teklifNo, QStringLiteral("000001"));
        QCOMPARE(q2.teklifNo, QStringLiteral("000002"));
    }
    closeAndRemove(conn);
}

void TestRepoQuotes::sequenceWraps7Digits()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = QStringLiteral("q_wrap");
    QString err;
    QVERIFY2(Db::openAndMigrate(dir.filePath(QStringLiteral("t.db")), &err, conn), qPrintable(err));

    {
        QSqlDatabase db = QSqlDatabase::database(conn);
        QSqlQuery seed(db);
        QVERIFY(seed.exec(QStringLiteral(
            "INSERT INTO settings (key, value) VALUES ('teklif_no_sayac', '999999')")));

        const QString musteriUnvani = QStringLiteral("Test Müşteri");
        QString e;

        Quote q;
        q.musteri.unvan = musteriUnvani;
        q.tarih = QDate(2026, 8, 25);
        QVERIFY2(RepoQuotes(db).add(q, &e), qPrintable(e));

        QCOMPARE(q.teklifNo, QStringLiteral("1000000")); // 7 haneye taştı, çökmedi
    }
    closeAndRemove(conn);
}

void TestRepoQuotes::rapidSequentialAddsNeverDuplicate()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = QStringLiteral("q_rapid");
    QString err;
    QVERIFY2(Db::openAndMigrate(dir.filePath(QStringLiteral("t.db")), &err, conn), qPrintable(err));

    {
        QSqlDatabase db = QSqlDatabase::database(conn);
        const QString musteriUnvani = QStringLiteral("Test Müşteri");
        QString e;

        QSet<QString> numaralar;
        for (int i = 0; i < 20; ++i) {
            Quote q;
            q.musteri.unvan = musteriUnvani;
            q.tarih = QDate(2026, 8, 25);
            QVERIFY2(RepoQuotes(db).add(q, &e), qPrintable(e));
            QVERIFY2(!numaralar.contains(q.teklifNo),
                     qPrintable(QStringLiteral("Tekrarlanan numara: %1").arg(q.teklifNo)));
            numaralar.insert(q.teklifNo);
        }
        QCOMPARE(numaralar.size(), 20);
    }
    closeAndRemove(conn);
}

QTEST_MAIN(TestRepoQuotes)
#include "test_repo_quotes.moc"
