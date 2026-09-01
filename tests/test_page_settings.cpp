#include <QtTest/QtTest>
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "core/db.h"
#include "core/repo_customers.h"
#include "core/repo_quotes.h"
#include "core/settings.h"
#include "print/company_logo.h"
#include "ui/mainwindow.h"
#include "ui/page_quote.h"
#include "ui/quote_line_model.h"
#include "ui/page_settings.h"
#include "ui/theme.h"

// Part 7: ayarlar ekrani, arayuz olcegi, belge yazi boyutu, yedekleme.

class TestPageSettings : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void savesCompanyInfo();
    void reloadShowsStoredValues();
    void totalHasNoVatBreakdown();
    void quoteNoDigitsAffectNumbering();
    void quoteNoDigitsAreClamped();
    void termsTextIsFrozenIntoQuote();
    void uiScaleChangesAppFontOnly();
    void uiScaleIsClamped();
    void uiScalePersists();
    void documentFontIsSeparateFromUiScale();
    void logoPreviewReflectsState();
    void backupProducesUsableDatabase();

private:
    QTemporaryDir *m_dir = nullptr;
    QString m_conn;
    QSqlDatabase m_db;
};

void TestPageSettings::init()
{
    m_dir = new QTemporaryDir();
    QVERIFY(m_dir->isValid());
    m_conn = QStringLiteral("ps_%1").arg(QDateTime::currentMSecsSinceEpoch());
    QString err;
    QVERIFY2(Db::openAndMigrate(m_dir->filePath(QStringLiteral("t.db")), &err, m_conn), qPrintable(err));
    m_db = QSqlDatabase::database(m_conn);
    Theme::applyUiScale(Theme::kDefaultScale);
}

void TestPageSettings::cleanup()
{
    Theme::applyUiScale(Theme::kDefaultScale);
    m_db = QSqlDatabase();
    QSqlDatabase::database(m_conn).close();
    QSqlDatabase::removeDatabase(m_conn);
    delete m_dir;
    m_dir = nullptr;
}

void TestPageSettings::savesCompanyInfo()
{
    PageSettings page(m_db);
    page.findChild<QLineEdit *>(QStringLiteral("ayarUnvanEdit"))
        ->setText(QStringLiteral("Öz Yapı İnşaat Ltd. Şti."));
    page.findChild<QLineEdit *>(QStringLiteral("ayarVergiNoEdit"))
        ->setText(QStringLiteral("1234567890"));

    QSignalSpy spy(&page, &PageSettings::companyInfoChanged);
    QString err;
    QVERIFY2(page.save(&err), qPrintable(err));
    QCOMPARE(spy.count(), 1);

    Settings s(m_db);
    QCOMPARE(s.valueOr(Settings::keyCompanyName()), QStringLiteral("Öz Yapı İnşaat Ltd. Şti."));
    QCOMPARE(s.valueOr(Settings::keyCompanyTaxNo()), QStringLiteral("1234567890"));
}

void TestPageSettings::reloadShowsStoredValues()
{
    Settings s(m_db);
    QVERIFY(s.setValue(Settings::keyCompanyName(), QStringLiteral("Kayıtlı Firma")));
    QVERIFY(s.setInt(Settings::keyDocumentFontPt(), 9));

    PageSettings page(m_db); // ctor refresh() cagirir
    QCOMPARE(page.findChild<QLineEdit *>(QStringLiteral("ayarUnvanEdit"))->text(),
             QStringLiteral("Kayıtlı Firma"));
    QCOMPARE(page.findChild<QSpinBox *>(QStringLiteral("ayarBelgeYaziSpin"))->value(), 9);
}

void TestPageSettings::totalHasNoVatBreakdown()
{
    // KDV ayristirmasi kaldirildi: fiyatlar KDV dahil girilir. Yeni bir
    // teklifte KDV orani 0 olmali ve genel toplam ara toplama esit cikmali.
    MainWindow w(m_db);

    Customer c;
    c.unvan = QStringLiteral("Musteri");
    QString err;
    QVERIFY(RepoCustomers(m_db).add(c, &err));
    w.quotePage()->reloadCustomers();
    w.quotePage()->selectCustomerById(c.id);

    Item it;
    it.kod = QStringLiteral("K-1"); it.ad = QStringLiteral("Kalem");
    it.birim = QStringLiteral("adet"); it.varsayilanFiyat = Money(10000);
    w.quotePage()->lineModel()->addLine(it, 3.0, Money(10000), QString());

    const Quote q = w.quotePage()->currentQuoteSnapshot();
    QCOMPARE(q.kdvOraniYuzde, 0);
    QCOMPARE(q.kdvTutari.kurus(), 0LL);
    QCOMPARE(q.genelToplam.toString(), q.araToplam.toString());
    QCOMPARE(q.genelToplam.toString(), QStringLiteral("300,00"));

    // Ekranda KDV kutusu ve satiri artik olmamali.
    QVERIFY2(!w.quotePage()->findChild<QWidget *>(QStringLiteral("kdvCheck")),
             "KDV kutusu hala ekranda");
    QVERIFY2(!w.quotePage()->findChild<QWidget *>(QStringLiteral("kdvLabel")),
             "KDV satiri hala ekranda");
}

void TestPageSettings::quoteNoDigitsAffectNumbering()
{
    Settings s(m_db);
    QVERIFY(s.setInt(Settings::keyQuoteNoDigits(), 4));

    Customer c;
    c.unvan = QStringLiteral("Musteri");
    QString err;
    QVERIFY(RepoCustomers(m_db).add(c, &err));

    Quote q;
    q.customerId = c.id;
    q.tarih = QDate(2026, 8, 25);
    QVERIFY2(RepoQuotes(m_db).add(q, &err), qPrintable(err));
    QCOMPARE(q.teklifNo, QStringLiteral("0001"));

    // Hane sayisini buyutmek ESKI teklifleri bozmaz (numara metin olarak
    // saklaniyor), yalnizca sonrakileri etkiler.
    QVERIFY(s.setInt(Settings::keyQuoteNoDigits(), 8));
    Quote q2;
    q2.customerId = c.id;
    q2.tarih = QDate(2026, 8, 25);
    QVERIFY2(RepoQuotes(m_db).add(q2, &err), qPrintable(err));
    QCOMPARE(q2.teklifNo, QStringLiteral("00000002"));
    QCOMPARE(RepoQuotes(m_db).get(q.id)->teklifNo, QStringLiteral("0001"));
}

void TestPageSettings::quoteNoDigitsAreClamped()
{
    // Elle bozulmus bir ayar numara uretimini cokertmemeli.
    Settings s(m_db);
    Customer c;
    c.unvan = QStringLiteral("Musteri");
    QString err;
    QVERIFY(RepoCustomers(m_db).add(c, &err));

    QVERIFY(s.setValue(Settings::keyQuoteNoDigits(), QStringLiteral("abc")));
    Quote q;
    q.customerId = c.id; q.tarih = QDate(2026, 8, 25);
    QVERIFY2(RepoQuotes(m_db).add(q, &err), qPrintable(err));
    QCOMPARE(q.teklifNo.size(), RepoQuotes::kDefaultQuoteNoDigits);

    QVERIFY(s.setInt(Settings::keyQuoteNoDigits(), 99)); // aralik disi
    Quote q2;
    q2.customerId = c.id; q2.tarih = QDate(2026, 8, 25);
    QVERIFY2(RepoQuotes(m_db).add(q2, &err), qPrintable(err));
    QCOMPARE(q2.teklifNo.size(), RepoQuotes::kMaxQuoteNoDigits);
}

void TestPageSettings::termsTextIsFrozenIntoQuote()
{
    // Sartlar metni teklif kaydedilirken KOPYALANIR: ayar sonradan degisse
    // bile eski teklif kendi sartlariyla basilir.
    MainWindow w(m_db);
    w.settingsPage()->findChild<QPlainTextEdit *>(QStringLiteral("ayarSartlarEdit"))
        ->setPlainText(QStringLiteral("Fiyatlarımıza KDV dahil değildir."));
    QString err;
    QVERIFY2(w.settingsPage()->save(&err), qPrintable(err));

    Customer c;
    c.unvan = QStringLiteral("Musteri");
    QVERIFY(RepoCustomers(m_db).add(c, &err));
    w.quotePage()->reloadCustomers();
    w.quotePage()->selectCustomerById(c.id);

    Item it;
    it.kod = QStringLiteral("K-1"); it.ad = QStringLiteral("Kalem");
    it.birim = QStringLiteral("adet"); it.varsayilanFiyat = Money(10000);
    w.quotePage()->lineModel()->addLine(it, 1.0, Money(10000), QString());
    QVERIFY2(w.quotePage()->save(&err), qPrintable(err));
    const qint64 id = w.quotePage()->currentQuoteId();

    // Ayari degistir.
    w.settingsPage()->findChild<QPlainTextEdit *>(QStringLiteral("ayarSartlarEdit"))
        ->setPlainText(QStringLiteral("Yeni şartlar"));
    QVERIFY(w.settingsPage()->save(&err));

    // Eski teklif kendi metnini korumali.
    QCOMPARE(RepoQuotes(m_db).get(id)->sartlarMetni,
             QStringLiteral("Fiyatlarımıza KDV dahil değildir."));
}

void TestPageSettings::uiScaleChangesAppFontOnly()
{
    const double once = QApplication::font().pointSizeF();
    Theme::applyUiScale(150);
    const double sonra = QApplication::font().pointSizeF();
    QVERIFY2(sonra > once, "arayuz olcegi yazi tipini buyutmedi");
    QCOMPARE(Theme::currentScale(), 150);
}

void TestPageSettings::uiScaleIsClamped()
{
    Theme::applyUiScale(1000);
    QCOMPARE(Theme::currentScale(), Theme::kMaxScale);
    Theme::applyUiScale(10);
    QCOMPARE(Theme::currentScale(), Theme::kMinScale);
}

void TestPageSettings::uiScalePersists()
{
    Settings s(m_db);
    QString err;
    QVERIFY2(Theme::setAndStore(s, 120, &err), qPrintable(err));
    QCOMPARE(s.intValueOr(Settings::keyUiScale(), 0), 120LL);

    // Yeniden baslatma benzetimi: varsayilana don, sonra ayardan uygula.
    Theme::applyUiScale(Theme::kDefaultScale);
    Theme::applyFromSettings(s);
    QCOMPARE(Theme::currentScale(), 120);
}

void TestPageSettings::documentFontIsSeparateFromUiScale()
{
    // Roadmap'in acik gereksinimi: arayuz olcegi CIKTIYI degistirmemeli,
    // belge yazi boyutu da ARAYUZU.
    MainWindow w(m_db);
    w.settingsPage()->findChild<QSpinBox *>(QStringLiteral("ayarBelgeYaziSpin"))->setValue(8);
    w.settingsPage()->findChild<QSpinBox *>(QStringLiteral("ayarOlcekSpin"))->setValue(150);
    QString err;
    QVERIFY2(w.settingsPage()->save(&err), qPrintable(err));

    Settings s(m_db);
    QCOMPARE(s.intValueOr(Settings::keyDocumentFontPt(), 0), 8LL);
    QCOMPARE(s.intValueOr(Settings::keyUiScale(), 0), 150LL);
    // Ikisi ayri anahtarlar; biri digerini ezmemeli.
    QVERIFY(Settings::keyDocumentFontPt() != Settings::keyUiScale());
}

void TestPageSettings::logoPreviewReflectsState()
{
    PageSettings page(m_db);
    auto *onizleme = page.findChild<QLabel *>(QStringLiteral("ayarLogoOnizleme"));
    auto *sil = page.findChild<QPushButton *>(QStringLiteral("ayarLogoSilButton"));
    QVERIFY(onizleme && sil);

    // Logo yokken: metin gorunur, "Kaldir" kapali.
    QVERIFY(!onizleme->text().isEmpty());
    QVERIFY(!sil->isEnabled());

    QImage img(100, 50, QImage::Format_ARGB32);
    img.fill(Qt::blue);
    Settings s(m_db);
    QVERIFY(CompanyLogo::save(s, img));
    page.refresh();

    QVERIFY2(onizleme->text().isEmpty(), "logo varken hala metin gosteriliyor");
    QVERIFY(!onizleme->pixmap().isNull());
    QVERIFY(sil->isEnabled());
}

void TestPageSettings::backupProducesUsableDatabase()
{
    // Yedek al -> veri sil -> yedegi ac: her sey donmeli.
    Customer c;
    c.unvan = QStringLiteral("Şükrü Çelik");
    QString err;
    QVERIFY(RepoCustomers(m_db).add(c, &err));
    Settings(m_db).setValue(Settings::keyCompanyName(), QStringLiteral("Test Firma"));

    const QString yedek = m_dir->filePath(QStringLiteral("yedek.db"));
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("VACUUM INTO :yol"));
    q.bindValue(QStringLiteral(":yol"), yedek);
    QVERIFY2(q.exec(), qPrintable(q.lastError().text()));
    QVERIFY(QFile::exists(yedek));

    // Yedegi ayri bir baglantiyla ac.
    const QString conn2 = m_conn + QStringLiteral("_yedek");
    QVERIFY2(Db::openAndMigrate(yedek, &err, conn2), qPrintable(err));
    {
        QSqlDatabase db2 = QSqlDatabase::database(conn2);
        QCOMPARE(RepoCustomers(db2).listAll().size(), 1);
        QCOMPARE(RepoCustomers(db2).listAll().first().unvan, QStringLiteral("Şükrü Çelik"));
        QCOMPARE(Settings(db2).valueOr(Settings::keyCompanyName()), QStringLiteral("Test Firma"));
    }
    QSqlDatabase::database(conn2).close();
    QSqlDatabase::removeDatabase(conn2);
}

QTEST_MAIN(TestPageSettings)
#include "test_page_settings.moc"
