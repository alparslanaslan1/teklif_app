#include <QtTest/QtTest>

#include "core/numparse.h"

class TestNumParse : public QObject
{
    Q_OBJECT

private slots:
    void gecerli_data();
    void gecerli();
    void gecersiz_data();
    void gecersiz();
};

void TestNumParse::gecerli_data()
{
    QTest::addColumn<QString>("girdi");
    QTest::addColumn<double>("beklenen");

    QTest::newRow("tam_sayi") << QStringLiteral("24") << 24.0;
    QTest::newRow("virgul_2hane") << QStringLiteral("12,5") << 12.5;
    QTest::newRow("virgul_3hane") << QStringLiteral("0,333") << 0.333;
    QTest::newRow("binlik_gruplu") << QStringLiteral("1.234") << 1234.0;
    QTest::newRow("binlik_ve_ondalik") << QStringLiteral("1.234,5") << 1234.5;
    QTest::newRow("eksi") << QStringLiteral("-5,5") << -5.5;
}

void TestNumParse::gecerli()
{
    QFETCH(QString, girdi);
    QFETCH(double, beklenen);
    const auto sonuc = parseTurkishNumber(girdi);
    QVERIFY(sonuc.has_value());
    QCOMPARE(sonuc.value(), beklenen);
}

void TestNumParse::gecersiz_data()
{
    QTest::addColumn<QString>("girdi");

    // "12.5" BİLEREK reddediliyor: nokta sadece binlik ayraç, ondalık
    // değil — Money::fromString ile aynı tutarlı kural.
    QTest::newRow("nokta_ondalik") << QStringLiteral("12.5");
    QTest::newRow("harf") << QStringLiteral("abc");
    QTest::newRow("bos") << QStringLiteral("");
    QTest::newRow("sadece_bosluk") << QStringLiteral("   ");
    QTest::newRow("hatali_gruplama") << QStringLiteral("12.34.56");
    QTest::newRow("iki_virgul") << QStringLiteral("12,34,56");
}

void TestNumParse::gecersiz()
{
    QFETCH(QString, girdi);
    QVERIFY(!parseTurkishNumber(girdi).has_value());
}

QTEST_APPLESS_MAIN(TestNumParse)
#include "test_numparse.moc"
