#include <QtTest/QtTest>

#include "teklif/core/turkish.h"

#include <algorithm>

class TestTurkish : public QObject
{
    Q_OBJECT

private slots:
    void normalizeFoldsTurkishLetters();
    void normalizeCapitalDottedI();
    void sortsTurkishAlphabetOrder();
    void sortIsCaseInsensitive();
    void sortPutsDigitsBeforeLetters();
    void compareIsTransitive();
    void sortKeyIsDeterministic();
};

void TestTurkish::normalizeFoldsTurkishLetters()
{
    QCOMPARE(turkishSearchNormalize(QStringLiteral("İşçilik")), QStringLiteral("iscilik"));
    QCOMPARE(turkishSearchNormalize(QStringLiteral("Alçıpan")), QStringLiteral("alcipan"));
    QCOMPARE(turkishSearchNormalize(QStringLiteral("ÖĞÜT")), QStringLiteral("ogut"));
    QCOMPARE(turkishSearchNormalize(QStringLiteral("ısı")), QStringLiteral("isi"));
}

void TestTurkish::normalizeCapitalDottedI()
{
    // Qt'nin varsayilan toLower()'i 'İ'yi 'i' + U+0307 (IKI kod noktasi)
    // yapar. Normalize edilmis metin tek karakter olmali, yoksa startsWith
    // karsilastirmalari tutmaz.
    const QString s = turkishSearchNormalize(QStringLiteral("İ"));
    QCOMPARE(s.size(), 1);
    QCOMPARE(s, QStringLiteral("i"));
}

void TestTurkish::sortsTurkishAlphabetOrder()
{
    QStringList v = {QStringLiteral("Zeytin"),  QStringLiteral("Çimento"),
                     QStringLiteral("Alçıpan"), QStringLiteral("ısı yalıtımı"),
                     QStringLiteral("İşçilik"), QStringLiteral("Beton"),
                     QStringLiteral("Ahşap"),   QStringLiteral("çıta")};

    std::sort(v.begin(), v.end(),
              [](const QString &a, const QString &b) { return turkishCompare(a, b) < 0; });

    // SQLite'in BINARY collation'i Ç, ç, İ, ı ile baslayanlari Z'den SONRAYA
    // atiyordu. Turk alfabesi sirasi: a b c ç d e f g ğ h ı i ... v y z
    // DIKKAT: Turk alfabesinde 'i' harfi 'i'DEN ONCE gelir (... g g h i i j ...),
    // bu yuzden "cita" < "Cimento" ve "isi yalitimi" < "Iscilik".
    const QStringList beklenen = {QStringLiteral("Ahşap"),        QStringLiteral("Alçıpan"),
                                   QStringLiteral("Beton"),        QStringLiteral("çıta"),
                                   QStringLiteral("Çimento"),      QStringLiteral("ısı yalıtımı"),
                                   QStringLiteral("İşçilik"),      QStringLiteral("Zeytin")};
    QCOMPARE(v, beklenen);
}

void TestTurkish::sortIsCaseInsensitive()
{
    // Buyuk/kucuk harf birincil olcut degil: "ahsap" ile "Ahsap" bitisik durur,
    // araya "Beton" giremez.
    QCOMPARE(turkishCompare(QStringLiteral("ahşap"), QStringLiteral("Beton")) < 0, true);
    QCOMPARE(turkishCompare(QStringLiteral("AHŞAP"), QStringLiteral("Beton")) < 0, true);
    // Yalnizca harf buyuklugunde ayrilan iki metin esit anahtara sahip olmali,
    // ama siralama kararli olsun diye 0 DONMEZ.
    QVERIFY(turkishCompare(QStringLiteral("ahşap"), QStringLiteral("Ahşap")) != 0);
}

void TestTurkish::sortPutsDigitsBeforeLetters()
{
    QVERIFY(turkishCompare(QStringLiteral("12 mm boru"), QStringLiteral("Boru")) < 0);
    // Rakamlar SOZLUK sirasiyla karsilastirilir, sayisal olarak DEGIL:
    // "10" < "2" (once '1', sonra '2'). Metin siralamasinin standart
    // davranisi budur; sayisal siralama isteniyorsa kodlar sifir dolgulu
    // yazilmalidir ("K-00002" gibi) — katalogda zaten oyle.
    QVERIFY(turkishCompare(QStringLiteral("10"), QStringLiteral("2")) < 0);
}

void TestTurkish::compareIsTransitive()
{
    // a < b ve b < c ise a < c olmali; std::sort bunu varsayar, ihlal edilirse
    // tanimsiz davranis olusur.
    const QStringList v = {QStringLiteral("Ahşap"), QStringLiteral("Çimento"),
                            QStringLiteral("İşçilik"), QStringLiteral("Zeytin")};
    for (int i = 0; i < v.size(); ++i) {
        for (int j = 0; j < v.size(); ++j) {
            const int c = turkishCompare(v[i], v[j]);
            if (i < j)
                QVERIFY2(c < 0, qPrintable(v[i] + " < " + v[j]));
            else if (i > j)
                QVERIFY2(c > 0, qPrintable(v[i] + " > " + v[j]));
            else
                QCOMPARE(c, 0);
        }
    }
}

void TestTurkish::sortKeyIsDeterministic()
{
    // Ayni girdi her zaman ayni anahtari uretmeli — ICU surumune ya da
    // sistem yereline bagli DEGIL.
    const QString a = turkishSortKey(QStringLiteral("Şükrü Çelik"));
    const QString b = turkishSortKey(QStringLiteral("Şükrü Çelik"));
    QCOMPARE(a, b);
    QVERIFY(!a.isEmpty());
}

QTEST_APPLESS_MAIN(TestTurkish)
#include "test_turkish.moc"
