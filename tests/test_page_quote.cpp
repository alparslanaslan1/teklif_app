#include <QtTest/QtTest>
#include <QApplication>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QTimer>

#include "core/db.h"
#include "core/repo_customers.h"
#include "core/repo_items.h"
#include "ui/dlg_line_entry.h"
#include "ui/item_search.h"
#include "ui/page_quote.h"
#include "ui/quote_line_model.h"

namespace {

void closeAndRemove(const QString &connectionName)
{
    QSqlDatabase::database(connectionName).close();
    QSqlDatabase::removeDatabase(connectionName);
}

// Açılması beklenen bir LineEntryDialog'u bulup "Tamam"a basar. Dialog
// exec() ile kendi event loop'unu açtığı için bu QTimer::singleShot(0, ...)
// ile o event loop başladıktan HEMEN SONRA çalışacak şekilde zamanlanır —
// modal Qt dialoglarını test etmenin standart yolu.
void scheduleAcceptNextDialog()
{
    QTimer::singleShot(0, [] {
        for (QWidget *w : QApplication::topLevelWidgets()) {
            if (auto *dlg = qobject_cast<LineEntryDialog *>(w)) {
                auto *box = dlg->findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
                QTest::mouseClick(box->button(QDialogButtonBox::Ok), Qt::LeftButton);
                return;
            }
        }
    });
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
    Customer c;
    c.unvan = QStringLiteral("Test Müşteri");
    QString e;
    QVERIFY2(RepoCustomers(m_db).add(c, &e), qPrintable(e));

    Item it;
    it.kod = QStringLiteral("ISC-01");
    it.ad = QStringLiteral("İşçilik");
    it.birim = QStringLiteral("saat");
    it.varsayilanFiyat = Money::fromString(QStringLiteral("350,00")).value();
    QVERIFY2(RepoItems(m_db).add(it, &e), qPrintable(e));

    PageQuote page(m_db);
    page.reloadCustomers();
    page.reloadCatalog();
    page.selectCustomerById(c.id);
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
    Customer c;
    c.unvan = QStringLiteral("Test Müşteri");
    QString e;
    QVERIFY2(RepoCustomers(m_db).add(c, &e), qPrintable(e));

    Item it;
    it.kod = QStringLiteral("X");
    it.ad = QStringLiteral("Kalem");
    it.birim = QStringLiteral("adet");
    it.varsayilanFiyat = Money::fromString(QStringLiteral("100,00")).value();
    QVERIFY2(RepoItems(m_db).add(it, &e), qPrintable(e));

    PageQuote page(m_db);
    page.reloadCustomers();
    page.reloadCatalog();
    page.selectCustomerById(c.id);
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
    Customer c;
    c.unvan = QStringLiteral("Test Müşteri");
    QString e;
    QVERIFY2(RepoCustomers(m_db).add(c, &e), qPrintable(e));

    for (int i = 0; i < 10; ++i) {
        Item it;
        it.kod = QStringLiteral("K%1").arg(i);
        it.ad = QStringLiteral("Kalem%1").arg(i);
        it.birim = QStringLiteral("adet");
        it.varsayilanFiyat = Money(1000);
        QVERIFY2(RepoItems(m_db).add(it, &e), qPrintable(e));
    }

    PageQuote page(m_db);
    page.reloadCustomers();
    page.reloadCatalog();
    page.selectCustomerById(c.id);
    page.show();

    auto *search = page.findChild<ItemSearch *>(QStringLiteral("itemSearch"));
    QVERIFY(search);
    auto *edit = search->findChild<QLineEdit *>(QStringLiteral("itemSearchEdit"));
    QVERIFY(edit);

    // Fareye HİÇ dokunmadan: yaz -> Enter (popup'tan seç, dialog açılır) ->
    // (zamanlanmış) dialog'un Tamam'ına tıkla -> satır eklenir -> tekrar.
    for (int i = 0; i < 10; ++i) {
        edit->clear();
        QTest::keyClicks(edit, QStringLiteral("Kalem%1").arg(i));
        scheduleAcceptNextDialog();
        QTest::keyClick(edit, Qt::Key_Return);
    }

    QCOMPARE(page.lineModel()->rowCount(), 10);
}

QTEST_MAIN(TestPageQuote)
#include "test_page_quote.moc"
