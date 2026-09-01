#include <QtTest/QtTest>

#include "teklif/core/calculator.h"

class TestCalculator : public QObject
{
    Q_OBJECT

private slots:
    void lineTotalBasic_data();
    void lineTotalBasic();
    void lineTotalFractionalQuantity();
    void lineTotalIskontoClampsToZero();
    void totalsEmptyList();
    void totalsSumsWithoutDrift();
    void totalsKdv();
    void totalsKdvZero();
    void totalsLargeAmountNoOverflow();
};

void TestCalculator::lineTotalBasic_data()
{
    QTest::addColumn<double>("miktar");
    QTest::addColumn<QString>("birimFiyat");
    QTest::addColumn<QString>("beklenen");

    QTest::newRow("24 adet x 180,00") << 24.0 << QStringLiteral("180,00") << QStringLiteral("4.320,00");
    QTest::newRow("16 saat x 350,00") << 16.0 << QStringLiteral("350,00") << QStringLiteral("5.600,00");
    QTest::newRow("3 x 0,00") << 3.0 << QStringLiteral("0,00") << QStringLiteral("0,00");
}

void TestCalculator::lineTotalBasic()
{
    QFETCH(double, miktar);
    QFETCH(QString, birimFiyat);
    QFETCH(QString, beklenen);

    const Money fiyat = Money::fromString(birimFiyat).value();
    const Money sonuc = Calculator::lineTotal(miktar, fiyat);
    QCOMPARE(sonuc.toString(), beklenen);
}

void TestCalculator::lineTotalFractionalQuantity()
{
    // 0,333 m² x 100,00 TL -> 33,30 TL. Yuvarlama satır bazında, tutarlı.
    const Money fiyat = Money::fromString(QStringLiteral("100,00")).value();
    const Money sonuc = Calculator::lineTotal(0.333, fiyat);
    QCOMPARE(sonuc.toString(), QStringLiteral("33,30"));
}

void TestCalculator::lineTotalIskontoClampsToZero()
{
    const Money fiyat = Money::fromString(QStringLiteral("10,00")).value();
    const Money iskonto = Money::fromString(QStringLiteral("50,00")).value();
    const Money sonuc = Calculator::lineTotal(1.0, fiyat, iskonto);
    QVERIFY(!sonuc.isNegative());
    QCOMPARE(sonuc.kurus(), qint64(0));
}

void TestCalculator::totalsEmptyList()
{
    const QuoteTotals t = Calculator::totals({}, 20);
    QVERIFY(t.araToplam.isZero());
    QVERIFY(t.kdvTutari.isZero());
    QVERIFY(t.genelToplam.isZero());
}

void TestCalculator::totalsSumsWithoutDrift()
{
    // 3 satır x 33,33 TL: double ile toplansaydı 99,98/100,00 sapma riski
    // vardı. Kuruş bazlı tam sayı toplamda tam 99,99 çıkmalı.
    const Money satirFiyat = Money::fromString(QStringLiteral("33,33")).value();
    const QVector<CalcLine> lines = {
        {1.0, satirFiyat, Money(0)},
        {1.0, satirFiyat, Money(0)},
        {1.0, satirFiyat, Money(0)},
    };
    const QuoteTotals t = Calculator::totals(lines, 0);
    QCOMPARE(t.araToplam.toString(), QStringLiteral("99,99"));
}

void TestCalculator::totalsKdv()
{
    const Money satirFiyat = Money::fromString(QStringLiteral("10.205,00")).value();
    const QVector<CalcLine> lines = {{1.0, satirFiyat, Money(0)}};
    const QuoteTotals t = Calculator::totals(lines, 20);

    QCOMPARE(t.araToplam.toString(), QStringLiteral("10.205,00"));
    QCOMPARE(t.kdvTutari.toString(), QStringLiteral("2.041,00"));
    QCOMPARE(t.genelToplam.toString(), QStringLiteral("12.246,00"));
}

void TestCalculator::totalsKdvZero()
{
    const Money satirFiyat = Money::fromString(QStringLiteral("500,00")).value();
    const QVector<CalcLine> lines = {{2.0, satirFiyat, Money(0)}};
    const QuoteTotals t = Calculator::totals(lines, 0);

    QVERIFY(t.kdvTutari.isZero());
    QCOMPARE(t.genelToplam.kurus(), t.araToplam.kurus());
}

void TestCalculator::totalsLargeAmountNoOverflow()
{
    // 1.000.000 adet x 1,00 TL = 1.000.000,00 TL. Taşma yok.
    const Money birFiyat = Money::fromString(QStringLiteral("1,00")).value();
    const QVector<CalcLine> lines = {{1000000.0, birFiyat, Money(0)}};
    const QuoteTotals t = Calculator::totals(lines, 20);

    QCOMPARE(t.araToplam.toString(), QStringLiteral("1.000.000,00"));
    QCOMPARE(t.kdvTutari.toString(), QStringLiteral("200.000,00"));
    QCOMPARE(t.genelToplam.toString(), QStringLiteral("1.200.000,00"));
}

QTEST_APPLESS_MAIN(TestCalculator)
#include "test_calculator.moc"
