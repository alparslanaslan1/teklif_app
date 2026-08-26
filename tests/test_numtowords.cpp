#include <QtTest/QtTest>

#include "core/numtowords.h"

class TestNumToWords : public QObject
{
    Q_OBJECT

private slots:
    void sayiYaziyla_test_data();
    void sayiYaziyla_test();
    void tutarYaziylaKurusluTutar();
    void tutarYaziylaTamTutarKurusYok();
    void tutarYaziylaSifir();
};

void TestNumToWords::sayiYaziyla_test_data()
{
    QTest::addColumn<qint64>("n");
    QTest::addColumn<QString>("beklenen");

    QTest::newRow("sifir") << qint64(0) << QStringLiteral("sıfır");
    QTest::newRow("bin") << qint64(1000) << QStringLiteral("bin"); // "birbin" DEĞİL
    QTest::newRow("binyuz") << qint64(1100) << QStringLiteral("binyüz");
    QTest::newRow("ikibin") << qint64(2000) << QStringLiteral("ikibin");
    QTest::newRow("yuzbes") << qint64(105) << QStringLiteral("yüzbeş");
    QTest::newRow("yuz") << qint64(100) << QStringLiteral("yüz"); // "biryüz" DEĞİL
    QTest::newRow("dokuz") << qint64(9) << QStringLiteral("dokuz");
    QTest::newRow("on") << qint64(10) << QStringLiteral("on");
    QTest::newRow("doksandokuz") << qint64(99) << QStringLiteral("doksandokuz");
    QTest::newRow("milyon") << qint64(1000000) << QStringLiteral("birmilyon"); // "bir" burada bastırılmaz
    QTest::newRow("buyuk_karisik")
        << qint64(1234567) << QStringLiteral("birmilyonikiyüzotuzdörtbinbeşyüzaltmışyedi");
    QTest::newRow("eksi") << qint64(-42) << QStringLiteral("eksi kırkiki");
}

void TestNumToWords::sayiYaziyla_test()
{
    QFETCH(qint64, n);
    QFETCH(QString, beklenen);
    QCOMPARE(sayiYaziyla(n), beklenen);
}

void TestNumToWords::tutarYaziylaKurusluTutar()
{
    const Money tutar = Money::fromString(QStringLiteral("12.246,40")).value();
    QCOMPARE(tutarYaziyla(tutar), QStringLiteral("onikibinikiyüzkırkaltı TL kırk kuruş"));
}

void TestNumToWords::tutarYaziylaTamTutarKurusYok()
{
    // Kuruş kısmı sıfırsa atlanır: "yüz TL", asla "yüz TL sıfır kuruş" değil.
    const Money tutar = Money::fromString(QStringLiteral("100,00")).value();
    QCOMPARE(tutarYaziyla(tutar), QStringLiteral("yüz TL"));
}

void TestNumToWords::tutarYaziylaSifir()
{
    QCOMPARE(tutarYaziyla(Money(0)), QStringLiteral("sıfır TL"));
}

QTEST_APPLESS_MAIN(TestNumToWords)
#include "test_numtowords.moc"
