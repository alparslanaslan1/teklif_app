#include <QtTest/QtTest>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSqlDatabase>
#include <QMessageBox>
#include <QTableView>
#include <QTemporaryDir>

#include "teklif/core/db.h"
#include "teklif/core/quote_status.h"
#include "teklif/core/repo_quotes.h"
#include "teklif/ui/mainwindow.h"
#include "teklif/ui/page_archive.h"
#include "teklif/ui/page_quote.h"
#include "teklif/ui/quote_line_model.h"
#include "teklif/ui/quote_summary_model.h"

// Part 6'nin arayuz tarafi: arsiv ekrani, musteri ekrani ve ikisini teklif
// ekranina baglayan MainWindow.

class TestPageArchive : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void archiveListsSavedQuotes();
    void archiveSearchFindsByCustomerName();
    void archiveSummaryShowsTotal();
    void archiveDuplicateAddsRowAndEmits();
    void mainWindowOpensQuoteFromArchive();
    void deleteRemovesRowAndEmits();
    void deletingOpenQuoteClearsQuotePage();

private:
    QTemporaryDir *m_dir = nullptr;
    QString m_conn;
    QSqlDatabase m_db;

    qint64 addQuote(const QString &musteri, const QDate &tarih, qint64 toplamKurus)
    {
        Quote q;
        q.musteri.unvan = musteri;
        q.tarih = tarih;
        q.genelToplam = Money(toplamKurus);
        QuoteLine l;
        l.sira = 1; l.aciklama = QStringLiteral("Kalem"); l.birim = QStringLiteral("adet");
        l.miktar = 1; l.birimFiyat = Money(toplamKurus); l.tutar = Money(toplamKurus);
        q.satirlar.append(l);
        QString e;
        RepoQuotes(m_db).add(q, &e);
        return q.id;
    }
};

void TestPageArchive::init()
{
    m_dir = new QTemporaryDir();
    QVERIFY(m_dir->isValid());
    m_conn = QStringLiteral("pa_%1").arg(QDateTime::currentMSecsSinceEpoch());
    QString err;
    QVERIFY2(Db::openAndMigrate(m_dir->filePath(QStringLiteral("t.db")), &err, m_conn), qPrintable(err));
    m_db = QSqlDatabase::database(m_conn);
}

void TestPageArchive::cleanup()
{
    m_db = QSqlDatabase();
    QSqlDatabase::database(m_conn).close();
    QSqlDatabase::removeDatabase(m_conn);
    delete m_dir;
    m_dir = nullptr;
}

void TestPageArchive::archiveListsSavedQuotes()
{
    const QString c = QStringLiteral("Şükrü Çelik");
    addQuote(c, QDate(2026, 8, 1), 10000);
    addQuote(c, QDate(2026, 8, 5), 20000);

    PageArchive page(m_db);
    QCOMPARE(page.model()->rowCount(), 2);
    // En yeni once.
    QCOMPARE(page.model()->at(0).tarih, QDate(2026, 8, 5));
    QCOMPARE(page.model()->at(0).musteriUnvan, QStringLiteral("Şükrü Çelik"));
}

// Musteri secici kaldirildi; musteriye gore listeleme artik arama
// kutusundan yapiliyor. Kullanicinin en sik ihtiyaci bu oldugu icin
// ekranin ustunden dogrudan test edilir.
void TestPageArchive::archiveSearchFindsByCustomerName()
{
    addQuote(QStringLiteral("Şükrü Çelik"), QDate(2026, 8, 1), 10000);
    addQuote(QStringLiteral("Şükrü Çelik"), QDate(2026, 8, 2), 10000);
    addQuote(QStringLiteral("Mehmet Yılmaz"), QDate(2026, 8, 3), 10000);

    PageArchive page(m_db);
    page.refresh();
    QCOMPARE(page.model()->rowCount(), 3);

    auto *arama = page.findChild<QLineEdit *>(QStringLiteral("arsivAramaEdit"));
    QVERIFY(arama);

    // Turkce harfler katlanir: "sukru" yazan kullanici "Şükrü"yu bulmali.
    arama->setText(QStringLiteral("sukru"));
    QCOMPARE(page.model()->rowCount(), 2);
    QCOMPARE(page.model()->at(0).musteriUnvan, QStringLiteral("Şükrü Çelik"));

    arama->clear();
    QCOMPARE(page.model()->rowCount(), 3);
}

void TestPageArchive::archiveSummaryShowsTotal()
{
    const QString c = QStringLiteral("Musteri");
    addQuote(c, QDate(2026, 8, 1), 10000);
    addQuote(c, QDate(2026, 8, 2), 25050);

    PageArchive page(m_db);
    auto *ozet = page.findChild<QLabel *>(QStringLiteral("arsivOzetLabel"));
    QVERIFY(ozet);
    QVERIFY2(ozet->text().contains(QStringLiteral("350,50")), qPrintable(ozet->text()));
    QVERIFY(ozet->text().contains(QStringLiteral("2")));
}

void TestPageArchive::archiveDuplicateAddsRowAndEmits()
{
    const QString c = QStringLiteral("Musteri");
    addQuote(c, QDate(2025, 1, 1), 10000);

    PageArchive page(m_db);
    QCOMPARE(page.model()->rowCount(), 1);

    // Ilk satiri sec.
    auto *table = page.findChild<QTableView *>(QStringLiteral("arsivTable"));
    QVERIFY(table);
    table->setCurrentIndex(page.model()->index(0, 0));

    QSignalSpy spy(&page, &PageArchive::quoteDuplicated);
    auto *kopyala = page.findChild<QPushButton *>(QStringLiteral("arsivKopyalaButton"));
    QVERIFY(kopyala);
    QVERIFY(kopyala->isEnabled());
    kopyala->click();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(page.model()->rowCount(), 2);
    // Kopya bugunun tarihiyle, yani listenin basinda olmali.
    QCOMPARE(page.model()->at(0).tarih, QDate::currentDate());
    QCOMPARE(page.model()->at(0).durum, QuoteStatus::taslak());
}

void TestPageArchive::mainWindowOpensQuoteFromArchive()
{
    // Part 6'nin can alici akisi: arsivden teklif acinca teklif ekrani o
    // teklifi yukler ve o sayfaya gecilir.
    const QString c = QStringLiteral("Musteri");
    const qint64 qid = addQuote(c, QDate(2026, 8, 1), 45000);

    MainWindow w(m_db);
    QCOMPARE(w.quotePage()->currentQuoteId(), qint64(0)); // once bos

    w.archivePage()->refresh();
    auto *table = w.archivePage()->findChild<QTableView *>(QStringLiteral("arsivTable"));
    QVERIFY(table);
    table->setCurrentIndex(w.archivePage()->model()->index(0, 0));
    QCOMPARE(w.archivePage()->selectedQuoteId(), qid);

    auto *ac = w.archivePage()->findChild<QPushButton *>(QStringLiteral("arsivAcButton"));
    QVERIFY(ac);
    ac->click();

    QCOMPARE(w.quotePage()->currentQuoteId(), qid);
    QCOMPARE(w.quotePage()->lineModel()->rowCount(), 1);
}

void TestPageArchive::deleteRemovesRowAndEmits()
{
    const QString c = QStringLiteral("Musteri");
    addQuote(c, QDate(2026, 8, 1), 10000);
    addQuote(c, QDate(2026, 8, 2), 20000);

    PageArchive page(m_db);
    QCOMPARE(page.model()->rowCount(), 2);

    auto *table = page.findChild<QTableView *>(QStringLiteral("arsivTable"));
    QVERIFY(table);
    table->setCurrentIndex(page.model()->index(0, 0));
    const qint64 silinecek = page.selectedQuoteId();

    QSignalSpy spy(&page, &PageArchive::quoteDeleted);

    // Onay kutusundaki "Sil" dugmesine bas.
    QTimer::singleShot(0, [] {
        for (QWidget *w : QApplication::topLevelWidgets()) {
            if (auto *box = qobject_cast<QMessageBox *>(w)) {
                for (QAbstractButton *b : box->buttons()) {
                    if (box->buttonRole(b) == QMessageBox::DestructiveRole) {
                        b->click();
                        return;
                    }
                }
            }
        }
    });
    page.findChild<QPushButton *>(QStringLiteral("arsivSilButton"))->click();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toLongLong(), silinecek);
    QCOMPARE(page.model()->rowCount(), 1);
    QVERIFY(!RepoQuotes(m_db).get(silinecek).has_value());
}

void TestPageArchive::deletingOpenQuoteClearsQuotePage()
{
    // Ekranda ACIK olan teklif silinirse form bosaltilmali; yoksa kullanici
    // artik var olmayan bir kaydi kaydetmeye calisir.
    const QString c = QStringLiteral("Musteri");
    const qint64 qid = addQuote(c, QDate(2026, 8, 1), 10000);

    MainWindow w(m_db);
    QString err;
    QVERIFY2(w.quotePage()->loadQuote(qid, &err), qPrintable(err));
    QCOMPARE(w.quotePage()->currentQuoteId(), qid);

    w.archivePage()->refresh();
    auto *table = w.archivePage()->findChild<QTableView *>(QStringLiteral("arsivTable"));
    table->setCurrentIndex(w.archivePage()->model()->index(0, 0));

    QTimer::singleShot(0, [] {
        for (QWidget *wd : QApplication::topLevelWidgets()) {
            if (auto *box = qobject_cast<QMessageBox *>(wd)) {
                for (QAbstractButton *b : box->buttons()) {
                    if (box->buttonRole(b) == QMessageBox::DestructiveRole) {
                        b->click();
                        return;
                    }
                }
            }
        }
    });
    w.archivePage()->findChild<QPushButton *>(QStringLiteral("arsivSilButton"))->click();

    QCOMPARE(w.quotePage()->currentQuoteId(), qint64(0));
    QCOMPARE(w.quotePage()->lineModel()->rowCount(), 0);
}

QTEST_MAIN(TestPageArchive)
#include "test_page_archive.moc"
