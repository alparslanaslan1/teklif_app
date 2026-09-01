#include <QtTest/QtTest>
#include <QSqlDatabase>
#include <QTemporaryDir>

#include "teklif/core/db.h"
#include "teklif/core/settings.h"

class TestSettings : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void missingKeyReturnsNullopt();
    void setThenGet();
    void setOverwritesExisting();
    void emptyValueIsNotMissing();
    void intRoundTrip();
    void intFallsBackOnGarbage();
    void boolRoundTrip();
    void removeKey();
    void keysAreDistinct();
    void defaultTermsMentionVatIncluded();

private:
    QTemporaryDir *m_dir = nullptr;
    QString m_conn;
    QSqlDatabase m_db;
};

void TestSettings::init()
{
    m_dir = new QTemporaryDir();
    QVERIFY(m_dir->isValid());
    m_conn = QStringLiteral("st_%1").arg(QDateTime::currentMSecsSinceEpoch());
    QString err;
    QVERIFY2(Db::openAndMigrate(m_dir->filePath(QStringLiteral("t.db")), &err, m_conn), qPrintable(err));
    m_db = QSqlDatabase::database(m_conn);
}

void TestSettings::cleanup()
{
    m_db = QSqlDatabase();
    QSqlDatabase::database(m_conn).close();
    QSqlDatabase::removeDatabase(m_conn);
    delete m_dir;
    m_dir = nullptr;
}

void TestSettings::missingKeyReturnsNullopt()
{
    Settings s(m_db);
    QVERIFY(!s.value(QStringLiteral("olmayan")).has_value());
    QCOMPARE(s.valueOr(QStringLiteral("olmayan"), QStringLiteral("vars")), QStringLiteral("vars"));
}

void TestSettings::setThenGet()
{
    Settings s(m_db);
    QString err;
    QVERIFY2(s.setValue(Settings::keyCompanyName(), QStringLiteral("Öz Yapı Ltd."), &err),
             qPrintable(err));
    QCOMPARE(s.valueOr(Settings::keyCompanyName()), QStringLiteral("Öz Yapı Ltd."));
}

void TestSettings::setOverwritesExisting()
{
    Settings s(m_db);
    QVERIFY(s.setValue(Settings::keyCompanyName(), QStringLiteral("Eski")));
    QVERIFY(s.setValue(Settings::keyCompanyName(), QStringLiteral("Yeni")));
    QCOMPARE(s.valueOr(Settings::keyCompanyName()), QStringLiteral("Yeni"));
}

void TestSettings::emptyValueIsNotMissing()
{
    // "kayit yok" ile "degeri bos metin" ayirt edilebilmeli — optional'in
    // varlik sebebi bu.
    Settings s(m_db);
    QVERIFY(s.setValue(Settings::keyPdfFolder(), QString()));
    const auto v = s.value(Settings::keyPdfFolder());
    QVERIFY(v.has_value());
    QVERIFY(v->isEmpty());
}

void TestSettings::intRoundTrip()
{
    Settings s(m_db);
    QVERIFY(s.setInt(Settings::keyQuoteCounter(), 143));
    QCOMPARE(s.intValueOr(Settings::keyQuoteCounter(), 0), 143LL);
    QCOMPARE(s.intValueOr(QStringLiteral("olmayan"), 7), 7LL);
}

void TestSettings::intFallsBackOnGarbage()
{
    // Elle bozulmus bir deger programi cokertmemeli.
    Settings s(m_db);
    QVERIFY(s.setValue(Settings::keyQuoteCounter(), QStringLiteral("abc")));
    QCOMPARE(s.intValueOr(Settings::keyQuoteCounter(), 99), 99LL);
}

void TestSettings::boolRoundTrip()
{
    Settings s(m_db);
    QVERIFY(s.setBool(Settings::keyUpdateCheckEnabled(), true));
    QVERIFY(s.boolValueOr(Settings::keyUpdateCheckEnabled(), false));
    QVERIFY(s.setBool(Settings::keyUpdateCheckEnabled(), false));
    QVERIFY(!s.boolValueOr(Settings::keyUpdateCheckEnabled(), true));
    // Anahtar yoksa varsayilan doner.
    QVERIFY(s.boolValueOr(QStringLiteral("olmayan"), true));
}

void TestSettings::removeKey()
{
    Settings s(m_db);
    QVERIFY(s.setValue(Settings::keyTermsText(), QStringLiteral("Şartlar")));
    QVERIFY(s.remove(Settings::keyTermsText()));
    QVERIFY(!s.value(Settings::keyTermsText()).has_value());
}

void TestSettings::keysAreDistinct()
{
    // Kopyala-yapistir ile iki ayarin ayni anahtari kullanmasi, birinin
    // digerini sessizce ezmesi demektir.
    const QStringList anahtarlar = {
        Settings::keyQuoteCounter(),      Settings::keyCompanyName(),
        Settings::keyCompanyAddress(),    Settings::keyCompanyPhone(),
        Settings::keyCompanyEmail(),      Settings::keyCompanyTaxOffice(),
        Settings::keyCompanyTaxNo(),      Settings::keyCompanyLicence(),
        Settings::keyUiScale(),           Settings::keyDocumentFontPt(),
        Settings::keyPdfFolder(),         Settings::keyTermsText(),
        Settings::keyUpdateSkipVersion(), Settings::keyUpdateCheckEnabled(),
    };
    QCOMPARE(QSet<QString>(anahtarlar.begin(), anahtarlar.end()).size(), anahtarlar.size());
}

void TestSettings::defaultTermsMentionVatIncluded()
{
    // Program KDV'yi ayri bir kalem olarak hesaplamiyor; fiyatlar KDV
    // dahil. Bunun belgede yazmasi musteriyle aradaki TEK kayit — toplam
    // satirinda bir KDV dokumu gorunmedigi icin belirtilmezse belirsiz
    // kalirdi.
    const QString varsayilan = Settings::varsayilanSartlar();
    QVERIFY2(varsayilan.contains(QStringLiteral("KDV")),
             qPrintable(QStringLiteral("varsayilan sartlarda KDV gecmiyor: %1").arg(varsayilan)));
    QVERIFY(varsayilan.contains(QStringLiteral("dahil")));

    // Kullanici kendi metnini girmediyse varsayilan kullanilmali.
    Settings s(m_db);
    QCOMPARE(s.valueOr(Settings::keyTermsText(), Settings::varsayilanSartlar()), varsayilan);

    // Girdiyse kendi metni korunmali.
    QVERIFY(s.setValue(Settings::keyTermsText(), QStringLiteral("Kendi şartlarım")));
    QCOMPARE(s.valueOr(Settings::keyTermsText(), Settings::varsayilanSartlar()),
             QStringLiteral("Kendi şartlarım"));
}

QTEST_MAIN(TestSettings)
#include "test_settings.moc"
