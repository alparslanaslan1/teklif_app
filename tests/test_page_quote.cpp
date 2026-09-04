#include <QtTest/QtTest>
#include <QApplication>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QTemporaryDir>
#include <QSqlDatabase>

#include "teklif/core/db.h"
#include "teklif/core/repo_items.h"
#include "teklif/ui/item_search.h"
#include "teklif/ui/page_quote.h"
#include "teklif/ui/quote_line_model.h"

namespace {

// Yalnizca unvani dolu bir musteri. Testlerin cogunda tek zorunlu alan
// budur; digerleri bos birakilabilir.
Customer musteriOlarak(const QString &unvan)
{
    Customer m;
    m.unvan = unvan;
    return m;
}


void closeAndRemove(const QString &connectionName)
{
    QSqlDatabase::database(connectionName).close();
    QSqlDatabase::removeDatabase(connectionName);
}

} // namespace

class TestPageQuote : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void saveWithoutCustomerFails();
    void saveThenReloadPreservesLinesAndTotals();
    void savedQuoteKeepsOldCatalogPrice();
    void tenLinesKeyboardOnly();
    void manualRowIsAddedAndEditable();
    void emptyRowsAreDroppedOnSave();
    void customerFieldsAreSavedWithQuote();

private:
    QTemporaryDir *m_dir = nullptr;
    QString m_conn;
    QSqlDatabase m_db;
};

void TestPageQuote::init()
{
    m_dir = new QTemporaryDir();
    QVERIFY(m_dir->isValid());
    m_conn = QStringLiteral("pq_%1").arg(QDateTime::currentMSecsSinceEpoch());
    QString err;
    QVERIFY2(Db::openAndMigrate(m_dir->filePath(QStringLiteral("t.db")), &err, m_conn), qPrintable(err));
    m_db = QSqlDatabase::database(m_conn);
}

void TestPageQuote::cleanup()
{
    closeAndRemove(m_conn);
    delete m_dir;
    m_dir = nullptr;
}

void TestPageQuote::saveWithoutCustomerFails()
{
    PageQuote page(m_db);
    QString err;
    QVERIFY(!page.save(&err));
    QVERIFY(!err.isEmpty());
}

void TestPageQuote::saveThenReloadPreservesLinesAndTotals()
{
    const QString musteriUnvani = QStringLiteral("Test Müşteri");
    QString e;

    Item it;
    it.kod = QStringLiteral("ISC-01");
    it.ad = QStringLiteral("İşçilik");
    it.birim = QStringLiteral("saat");
    it.varsayilanFiyat = Money::fromString(QStringLiteral("350,00")).value();
    QVERIFY2(RepoItems(m_db).add(it, &e), qPrintable(e));

    PageQuote page(m_db);
    page.reloadCatalog();
    page.setCustomer(musteriOlarak(musteriUnvani));
    page.lineModel()->addLine(it, 16.0, it.varsayilanFiyat, QStringLiteral("tavan dahil"));

    QString saveErr;
    QVERIFY2(page.save(&saveErr), qPrintable(saveErr));
    const qint64 savedId = page.currentQuoteId();
    QVERIFY(savedId > 0);
    QVERIFY(!page.currentQuoteNo().isEmpty());

    // Kapat / aç: yeni bir PageQuote ile aynı veritabanından tekrar oku.
    PageQuote page2(m_db);
    QString loadErr;
    QVERIFY2(page2.loadQuote(savedId, &loadErr), qPrintable(loadErr));

    QCOMPARE(page2.lineModel()->lines().size(), 1);
    QCOMPARE(page2.lineModel()->lines().first().aciklama, QStringLiteral("İşçilik"));
    QCOMPARE(page2.lineModel()->lines().first().satirNotu, QStringLiteral("tavan dahil"));
    QCOMPARE(page2.lineModel()->lines().first().tutar.toString(), QStringLiteral("5.600,00"));
    QCOMPARE(page2.currentQuoteNo(), page.currentQuoteNo());
}

void TestPageQuote::savedQuoteKeepsOldCatalogPrice()
{
    const QString musteriUnvani = QStringLiteral("Test Müşteri");
    QString e;

    Item it;
    it.kod = QStringLiteral("X");
    it.ad = QStringLiteral("Kalem");
    it.birim = QStringLiteral("adet");
    it.varsayilanFiyat = Money::fromString(QStringLiteral("100,00")).value();
    QVERIFY2(RepoItems(m_db).add(it, &e), qPrintable(e));

    PageQuote page(m_db);
    page.reloadCatalog();
    page.setCustomer(musteriOlarak(musteriUnvani));
    page.lineModel()->addLine(it, 1.0, it.varsayilanFiyat, QString());

    QString err;
    QVERIFY2(page.save(&err), qPrintable(err));
    const qint64 id = page.currentQuoteId();

    // Katalog fiyatını değiştir.
    it.varsayilanFiyat = Money::fromString(QStringLiteral("500,00")).value();
    QVERIFY2(RepoItems(m_db).update(it, &e), qPrintable(e));

    PageQuote page2(m_db);
    QVERIFY2(page2.loadQuote(id, &err), qPrintable(err));
    QCOMPARE(page2.lineModel()->lines().first().birimFiyat.toString(), QStringLiteral("100,00"));
}

void TestPageQuote::tenLinesKeyboardOnly()
{
    const QString musteriUnvani = QStringLiteral("Test Müşteri");
    QString e;

    for (int i = 0; i < 10; ++i) {
        Item it;
        it.kod = QStringLiteral("K%1").arg(i);
        it.ad = QStringLiteral("Kalem%1").arg(i);
        it.birim = QStringLiteral("adet");
        it.varsayilanFiyat = Money(1000);
        QVERIFY2(RepoItems(m_db).add(it, &e), qPrintable(e));
    }

    PageQuote page(m_db);
    page.reloadCatalog();
    page.setCustomer(musteriOlarak(musteriUnvani));
    page.show();

    auto *search = page.findChild<ItemSearch *>(QStringLiteral("itemSearch"));
    QVERIFY(search);
    auto *edit = search->findChild<QLineEdit *>(QStringLiteral("itemSearchEdit"));
    QVERIFY(edit);

    // Fareye HİÇ dokunmadan: yaz -> Enter (popup'tan seç) -> satır eklenir
    // -> tekrar. Araya diyalog GİRMEZ; miktar/fiyat düzeltmesi gerekiyorsa
    // tablodan yapılır.
    for (int i = 0; i < 10; ++i) {
        edit->clear();
        QTest::keyClicks(edit, QStringLiteral("Kalem%1").arg(i));
        QTest::keyClick(edit, Qt::Key_Return);
    }

    QCOMPARE(page.lineModel()->rowCount(), 10);
}

// "+" dugmesi katalog kaydi olmadan satir acabilmeli: her teklifte bir iki
// kalem o ise ozeldir ve bunun icin katalog kaydi acmak zorunda kalmamali.
void TestPageQuote::manualRowIsAddedAndEditable()
{
    PageQuote page(m_db);
    auto *ekle = page.findChild<QPushButton *>(QStringLiteral("satirEkleButton"));
    QVERIFY(ekle);

    ekle->click();
    QCOMPARE(page.lineModel()->rowCount(), 1);

    // Hucreler dogrudan doldurulabilir olmali.
    auto *model = page.lineModel();
    QVERIFY(model->setData(model->index(0, QuoteLineModel::ColAciklama),
                            QStringLiteral("Kolon boyası"), Qt::EditRole));
    QVERIFY(model->setData(model->index(0, QuoteLineModel::ColBirim),
                            QStringLiteral("m2"), Qt::EditRole));
    QVERIFY(model->setData(model->index(0, QuoteLineModel::ColMiktar),
                            QStringLiteral("12,5"), Qt::EditRole));
    QVERIFY(model->setData(model->index(0, QuoteLineModel::ColBirimFiyat),
                            QStringLiteral("80,00"), Qt::EditRole));

    const QuoteLine &l = model->lines().first();
    QCOMPARE(l.aciklama, QStringLiteral("Kolon boyası"));
    QCOMPARE(l.birim, QStringLiteral("m2"));
    QCOMPARE(l.miktar, 12.5);
    // Tutar elle yazilmaz, miktar x fiyattan hesaplanir.
    QCOMPARE(l.tutar.toString(), QStringLiteral("1.000,00"));

    // "-" dugmesi de calismali.
    auto *sil = page.findChild<QPushButton *>(QStringLiteral("satirSilButton"));
    QVERIFY(sil);
    page.findChild<QTableView *>(QStringLiteral("quoteTable"))
        ->setCurrentIndex(model->index(0, QuoteLineModel::ColAciklama));
    sil->click();
    QCOMPARE(model->rowCount(), 0);
}

// Kullanici "+" ile acip doldurmadigi bir satiri ekranda birakmis olabilir;
// belgeye ve kayda bos satir girmemeli.
void TestPageQuote::emptyRowsAreDroppedOnSave()
{
    PageQuote page(m_db);
    page.setCustomer(musteriOlarak(QStringLiteral("Test Müşteri")));

    auto *model = page.lineModel();
    model->addEmptyLine();
    model->setData(model->index(0, QuoteLineModel::ColAciklama), QStringLiteral("Dolu satır"),
                    Qt::EditRole);
    model->setData(model->index(0, QuoteLineModel::ColBirimFiyat), QStringLiteral("100,00"),
                    Qt::EditRole);
    model->addEmptyLine(); // bos birakilir

    QCOMPARE(model->rowCount(), 2);

    QString err;
    QVERIFY2(page.save(&err), qPrintable(err));

    PageQuote page2(m_db);
    QVERIFY2(page2.loadQuote(page.currentQuoteId(), &err), qPrintable(err));
    QCOMPARE(page2.lineModel()->rowCount(), 1);
    QCOMPARE(page2.lineModel()->lines().first().aciklama, QStringLiteral("Dolu satır"));
}

// Musteri artik ayri bir kayit degil; formdaki alanlar teklifin icine
// yazilmali ve teklif tekrar acildiginda geri gelmeli.
void TestPageQuote::customerFieldsAreSavedWithQuote()
{
    Customer m;
    m.unvan = QStringLiteral("Şükrü Çelik İnşaat");
    m.yetkili = QStringLiteral("Şükrü Çelik");
    m.telefon = QStringLiteral("0532 111 22 33");
    m.adres = QStringLiteral("İncilli Mah. Karasu");
    m.vergiDairesi = QStringLiteral("Karasu");
    m.vergiNo = QStringLiteral("1234567890");

    PageQuote page(m_db);
    page.setCustomer(m);

    QString err;
    QVERIFY2(page.save(&err), qPrintable(err));

    PageQuote page2(m_db);
    QVERIFY2(page2.loadQuote(page.currentQuoteId(), &err), qPrintable(err));
    QCOMPARE(page2.currentCustomer().unvan, m.unvan);
    QCOMPARE(page2.currentCustomer().yetkili, m.yetkili);
    QCOMPARE(page2.currentCustomer().telefon, m.telefon);
    QCOMPARE(page2.currentCustomer().adres, m.adres);
    QCOMPARE(page2.currentCustomer().vergiDairesi, m.vergiDairesi);
    QCOMPARE(page2.currentCustomer().vergiNo, m.vergiNo);
}

QTEST_MAIN(TestPageQuote)
#include "test_page_quote.moc"
