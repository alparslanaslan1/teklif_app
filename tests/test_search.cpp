#include <QtTest/QtTest>
#include <QElapsedTimer>

#include "core/search.h"

namespace {

Item mk(const QString &kod, const QString &ad, const QString &birim, qint64 fiyatKurus,
        bool aktif = true)
{
    Item it;
    it.id = 0;
    it.kod = kod;
    it.ad = ad;
    it.birim = birim;
    it.varsayilanFiyat = Money(fiyatKurus);
    it.aktif = aktif;
    return it;
}

// Testler boyunca kullanılan sabit katalog. Sıra kasıtlı: "baştan eşleşen"
// grubunun ("İşçilik", "İş İskelesi Kurulumu") kendi aralarındaki sırasının
// korunduğunu görebilmek için İşçilik önce geliyor.
QVector<Item> ornekKatalog()
{
    return {
        mk(QStringLiteral("ISC-01"), QStringLiteral("İşçilik"), QStringLiteral("saat"), 35000),
        mk(QStringLiteral("ISC-02"), QStringLiteral("İş İskelesi Kurulumu"), QStringLiteral("adet"), 12000),
        mk(QStringLiteral("ISC-03"), QStringLiteral("Kalıp İşçiliği"), QStringLiteral("m²"), 24000),
        mk(QStringLiteral("ALC-01"), QStringLiteral("Alçıpan Levha"), QStringLiteral("adet"), 18000),
        mk(QStringLiteral("PAS-01"), QStringLiteral("Pasif Test Kalemi"), QStringLiteral("adet"), 1000,
           /*aktif=*/false),
    };
}

QStringList kodlar(const QVector<Item> &sonuc)
{
    QStringList l;
    for (const Item &it : sonuc)
        l << it.kod;
    return l;
}

} // namespace

class TestSearch : public QObject
{
    Q_OBJECT

private slots:
    void isAra();
    void buyukIsAraAyniSonuc();
    void asciiIsAraNormalizasyon();
    void bulunamayanArama();
    void bosArama();
    void bosluklarArama();
    void pasifKalemAramaSonucundaYok();
    void performans5000Kalem();
};

void TestSearch::isAra()
{
    const QVector<Item> sonuc = itemAra(ornekKatalog(), QStringLiteral("iş"));
    // Baştan eşleşenler (orijinal sırayla) önce, içinde geçen sonra.
    QCOMPARE(kodlar(sonuc), (QStringList{"ISC-01", "ISC-02", "ISC-03"}));
}

void TestSearch::buyukIsAraAyniSonuc()
{
    const QVector<Item> kucuk = itemAra(ornekKatalog(), QStringLiteral("iş"));
    const QVector<Item> buyuk = itemAra(ornekKatalog(), QStringLiteral("IŞ"));
    QCOMPARE(kodlar(buyuk), kodlar(kucuk));
}

void TestSearch::asciiIsAraNormalizasyon()
{
    // Türkçe klavyesi olmadan yazılan düz "is", ş->s katlaması sayesinde
    // "İşçilik"i (ve aynı köke sahip diğer kalemleri) bulmalı.
    const QVector<Item> sonuc = itemAra(ornekKatalog(), QStringLiteral("is"));
    QVERIFY(kodlar(sonuc).contains(QStringLiteral("ISC-01")));
}

void TestSearch::bulunamayanArama()
{
    const QVector<Item> sonuc = itemAra(ornekKatalog(), QStringLiteral("xyz"));
    QVERIFY(sonuc.isEmpty());
}

void TestSearch::bosArama()
{
    const QVector<Item> sonuc = itemAra(ornekKatalog(), QString());
    QVERIFY(sonuc.isEmpty()); // tüm katalog DEĞİL, boş liste
}

void TestSearch::bosluklarArama()
{
    const QVector<Item> sonuc = itemAra(ornekKatalog(), QStringLiteral("   "));
    QVERIFY(sonuc.isEmpty());
}

void TestSearch::pasifKalemAramaSonucundaYok()
{
    const QVector<Item> sonuc = itemAra(ornekKatalog(), QStringLiteral("pasif"));
    QVERIFY(sonuc.isEmpty());
}

void TestSearch::performans5000Kalem()
{
    QVector<Item> buyukKatalog;
    buyukKatalog.reserve(5000);
    for (int i = 0; i < 5000; ++i) {
        buyukKatalog.append(mk(QStringLiteral("K-%1").arg(i, 5, 10, QLatin1Char('0')),
                                QStringLiteral("Kalem %1").arg(i), QStringLiteral("adet"), 100));
    }

    QElapsedTimer zaman;
    zaman.start();
    // Tüm kalemler "Kalem" ile başladığı için en kötü durumu (hepsi
    // sonuçta) ölçüyoruz.
    const QVector<Item> sonuc = itemAra(buyukKatalog, QStringLiteral("kalem"));
    const qint64 gecenMs = zaman.elapsed();

    QCOMPARE(sonuc.size(), 5000);
    // Sınır bilerek GENİŞ. Duvar saatiyle dar bir sınır koymak (eskiden
    // 50 ms'ti) yüklü bir CI makinesinde rastgele kırmızı yakar; oysa burada
    // yakalanmak istenen şey büyüklük mertebesinde bir gerileme — ör. arama
    // yolunun kazara O(n²) olması ya da her kalem için veritabanına gidilmesi.
    // Yazdıkça arama yapan arayüz zaten itemAra'yı değil ItemSearchIndex'i
    // kullanıyor (bkz. core/search.h).
    QVERIFY2(gecenMs < 2000,
             qPrintable(QStringLiteral("5000 kalemde arama %1 ms sürdü").arg(gecenMs)));
}

QTEST_APPLESS_MAIN(TestSearch)
#include "test_search.moc"
