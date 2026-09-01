#include <QtTest/QtTest>

#include "core/money.h"

class TestMoney : public QObject
{
    Q_OBJECT

private slots:
    void parseBasic();
    void parseGrouped();
    void roundTripToString();
    void rejectsOverflowingInput();
    void multiplyByInt();
    void arithmeticNoPrecisionLoss();
    void kdvHesabi();
    void parseInvalid_data();
    void parseInvalid();
};

void TestMoney::parseBasic()
{
    const auto m = Money::fromString(QStringLiteral("1234,56"));
    QVERIFY(m.has_value());
    QCOMPARE(m->kurus(), qint64(123456));
}

void TestMoney::parseGrouped()
{
    const auto m1 = Money::fromString(QStringLiteral("1.234,56"));
    QVERIFY(m1.has_value());
    QCOMPARE(m1->kurus(), qint64(123456));

    // 5+ haneli tutarlarda birden fazla nokta grubu.
    const auto m2 = Money::fromString(QStringLiteral("12.345,67"));
    QVERIFY(m2.has_value());
    QCOMPARE(m2->kurus(), qint64(1234567));
}

void TestMoney::roundTripToString()
{
    QCOMPARE(Money(123456).toString(), QStringLiteral("1.234,56"));
    QCOMPARE(Money(0).toString(), QStringLiteral("0,00"));
    QCOMPARE(Money(5).toString(), QStringLiteral("0,05"));
}

void TestMoney::multiplyByInt()
{
    QCOMPARE(Money(1) * qint64(3), Money(3));
}

void TestMoney::arithmeticNoPrecisionLoss()
{
    // 3 satır x 33,33 TL: double ile toplansaydı 99,98/100,00 gibi bir sapma
    // riski vardı. Kuruş bazlı tam sayı toplamda tam 99,99 çıkmalı.
    const Money satir = Money::fromString(QStringLiteral("33,33")).value();
    Money toplam;
    toplam += satir;
    toplam += satir;
    toplam += satir;
    QCOMPARE(toplam.kurus(), qint64(9999));
    QCOMPARE(toplam.toString(), QStringLiteral("99,99"));
}

void TestMoney::kdvHesabi()
{
    const Money araToplam = Money::fromString(QStringLiteral("10.205,00")).value();
    const Money kdv = araToplam * 0.20;
    const Money genel = araToplam + kdv;
    QCOMPARE(kdv.toString(), QStringLiteral("2.041,00"));
    QCOMPARE(genel.toString(), QStringLiteral("12.246,00"));
}

void TestMoney::parseInvalid_data()
{
    QTest::addColumn<QString>("girdi");
    QTest::newRow("harfler") << QStringLiteral("abc");
    QTest::newRow("bos") << QStringLiteral("");
    QTest::newRow("hatali_gruplama") << QStringLiteral("12.34.56");
    QTest::newRow("sadece_virgul") << QStringLiteral(",");
    QTest::newRow("virgulden_sonra_bos") << QStringLiteral("1234,");
    QTest::newRow("iki_virgul") << QStringLiteral("12,34,56");
    QTest::newRow("bosluk_var") << QStringLiteral("12 34");
    QTest::newRow("eksik_grup") << QStringLiteral(".234,56");
}

void TestMoney::parseInvalid()
{
    QFETCH(QString, girdi);
    QVERIFY(!Money::fromString(girdi).has_value());
}

void TestMoney::rejectsOverflowingInput()
{
    // fromString icinde tl * 100 + kr yapiliyor; qint64'un ustunu asan bir
    // girdi eskiden SESSIZCE tasiyordu (imzali tasma = tanimsiz davranis) ve
    // sonuc negatif bir tutar olarak gorunebiliyordu. Artik reddediliyor.
    QVERIFY(!Money::fromString(QStringLiteral("99999999999999999")).has_value());   // 17 hane
    QVERIFY(!Money::fromString(QStringLiteral("123456789012345678901234")).has_value());

    // Sinirin ALTI hala kabul edilmeli: 16 hane ~ 10 katrilyon TL.
    const auto sinirda = Money::fromString(QStringLiteral("9999999999999999"));
    QVERIFY2(sinirda.has_value(), "16 haneli gecerli tutar reddedildi");
    QVERIFY(!sinirda->isNegative());

    // Gunluk tutarlar elbette etkilenmemeli.
    QCOMPARE(Money::fromString(QStringLiteral("1.234,56"))->kurus(), 123456LL);
}

QTEST_APPLESS_MAIN(TestMoney)
#include "test_money.moc"
