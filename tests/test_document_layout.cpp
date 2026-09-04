#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>

#include "teklif/print/document_layout.h"

namespace {

QuoteLine mkLine(int sira, const QString &aciklama, double miktar, qint64 fiyatKurus,
                  const QString &notu = QString())
{
    QuoteLine l;
    l.sira = sira;
    l.aciklama = aciklama;
    l.birim = QStringLiteral("adet");
    l.miktar = miktar;
    l.birimFiyat = Money(fiyatKurus);
    l.satirNotu = notu;
    l.tutar = Money(static_cast<qint64>(miktar * fiyatKurus));
    return l;
}

DocumentContext mkContext(int satirSayisi)
{
    DocumentContext ctx;
    ctx.company.unvan = QStringLiteral("Öz Yapı İnşaat Ltd. Şti.");
    ctx.company.adres = QStringLiteral("Atatürk Cad. No:1, Ankara");
    ctx.quote.musteri.unvan = QStringLiteral("Ahmet Yılmaz");
    ctx.quote.teklifNo = QStringLiteral("000143");
    ctx.quote.tarih = QDate(2026, 8, 25);
    ctx.quote.projeBasligi = QStringLiteral("Ofis Tadilatı");
    for (int i = 0; i < satirSayisi; ++i)
        ctx.quote.satirlar.append(mkLine(i + 1, QStringLiteral("Kalem %1").arg(i + 1), 2, 18000));
    ctx.quote.araToplam = Money(360 * satirSayisi);
    ctx.quote.kdvOraniYuzde = 20;
    ctx.quote.kdvTutari = Money(72 * satirSayisi);
    ctx.quote.genelToplam = Money(432 * satirSayisi);
    return ctx;
}

// Verilen cozunurlukte, 15 mm kenar bosluklu bir A4 cizim yuzeyi.
// Gercek yazici gerekmez; QImage uzerinde olculen yerlesim kagittakiyle
// AYNI koddur.
struct Canvas
{
    QImage image;
    QPainter painter;
    QRectF pageRect;

    explicit Canvas(int dpi = 150)
        : image(static_cast<int>(8.27 * dpi), static_cast<int>(11.69 * dpi),
                QImage::Format_RGB32)
    {
        image.fill(Qt::white);
        // DPI'yi ACIKCA vermek sart: QImage varsayilan olarak 96 dpi bildirir
        // ve punto -> piksel donusumu buna gore yapilir. Ayarlanmazsa yazi
        // kagittakinden kucuk olculur ve sayfalama gercegi yansitmaz.
        image.setDotsPerMeterX(static_cast<int>(dpi * 39.3701));
        image.setDotsPerMeterY(static_cast<int>(dpi * 39.3701));
        painter.begin(&image);
        const double kenar = dpi * 15.0 / 25.4; // 15 mm
        pageRect = QRectF(kenar, kenar, image.width() - 2 * kenar, image.height() - 2 * kenar);
    }
    ~Canvas() { painter.end(); }
};

} // namespace

class TestDocumentLayout : public QObject
{
    Q_OBJECT

private slots:
    void emptyQuoteStillHasOnePage();
    void shortQuoteFitsOnePage();
    void longQuoteSpansMultiplePages();
    void allLinesAppearExactlyOnce();
    void totalsAreOnLastPageOnly();
    void totalsNeverOrphanedAlone();
    void smallerFontFitsMoreLines();
    void longDescriptionDoesNotOverflowColumn();
    void paintingDoesNotCrashOnEveryPage();
    void missingCompanyInfoShrinksHeader();
    void paginationIsResolutionIndependent();
    void longCompanyAddressWrapsInsteadOfBeingClipped();
    void licenceLineIsAccountedFor();
};

void TestDocumentLayout::emptyQuoteStillHasOnePage()
{
    Canvas c;
    DocumentLayout layout(mkContext(0));
    layout.paginate(&c.painter, c.pageRect);
    QCOMPARE(layout.pageCount(), 1);
    QVERIFY(layout.pageRange(0).toplamlarBurada);
}

void TestDocumentLayout::shortQuoteFitsOnePage()
{
    Canvas c;
    DocumentLayout layout(mkContext(3));
    layout.paginate(&c.painter, c.pageRect);
    QCOMPARE(layout.pageCount(), 1);
    QCOMPARE(layout.pageRange(0).satirSayisi, 3);
}

void TestDocumentLayout::longQuoteSpansMultiplePages()
{
    Canvas c;
    DocumentLayout layout(mkContext(40));
    layout.paginate(&c.painter, c.pageRect);
    QVERIFY2(layout.pageCount() > 1,
             qPrintable(QStringLiteral("40 kalem tek sayfaya sigdi: %1").arg(layout.pageCount())));
}

void TestDocumentLayout::allLinesAppearExactlyOnce()
{
    // Sayfalama hicbir satiri atlamamali ve iki kez basmamali.
    Canvas c;
    const int n = 40;
    DocumentLayout layout(mkContext(n));
    layout.paginate(&c.painter, c.pageRect);

    int toplam = 0;
    int beklenenSonraki = 0;
    for (int i = 0; i < layout.pageCount(); ++i) {
        const auto r = layout.pageRange(i);
        QCOMPARE(r.ilkSatir, beklenenSonraki);
        toplam += r.satirSayisi;
        beklenenSonraki = r.ilkSatir + r.satirSayisi;
    }
    QCOMPARE(toplam, n);
}

void TestDocumentLayout::totalsAreOnLastPageOnly()
{
    Canvas c;
    DocumentLayout layout(mkContext(40));
    layout.paginate(&c.painter, c.pageRect);

    for (int i = 0; i < layout.pageCount() - 1; ++i)
        QVERIFY2(!layout.pageRange(i).toplamlarBurada,
                 qPrintable(QStringLiteral("toplamlar %1. sayfada da var").arg(i)));
    QVERIFY(layout.pageRange(layout.pageCount() - 1).toplamlarBurada);
}

void TestDocumentLayout::totalsNeverOrphanedAlone()
{
    // Kalem sayisini tek tek artirarak sayfa sinirinda biten listeleri
    // yakala: toplamlar hangi sayfaya duserse dussun, o sayfa gecerli
    // olmali ve toplam blok tam olarak BIR sayfada bulunmali.
    for (int n = 1; n <= 60; ++n) {
        Canvas c;
        DocumentLayout layout(mkContext(n));
        layout.paginate(&c.painter, c.pageRect);

        int toplamlarSayfasi = 0;
        for (int i = 0; i < layout.pageCount(); ++i)
            if (layout.pageRange(i).toplamlarBurada)
                ++toplamlarSayfasi;

        QVERIFY2(toplamlarSayfasi == 1,
                 qPrintable(QStringLiteral("%1 kalem: toplam blogu %2 sayfada").arg(n).arg(toplamlarSayfasi)));
    }
}

void TestDocumentLayout::smallerFontFitsMoreLines()
{
    Canvas c8;
    DocumentContext ctx8 = mkContext(40);
    ctx8.fontPt = 8;
    DocumentLayout kucuk(ctx8);
    kucuk.paginate(&c8.painter, c8.pageRect);

    Canvas c12;
    DocumentContext ctx12 = mkContext(40);
    ctx12.fontPt = 12;
    DocumentLayout buyuk(ctx12);
    buyuk.paginate(&c12.painter, c12.pageRect);

    QVERIFY2(kucuk.pageRange(0).satirSayisi > buyuk.pageRange(0).satirSayisi,
             qPrintable(QStringLiteral("8pt: %1 satir, 12pt: %2 satir")
                            .arg(kucuk.pageRange(0).satirSayisi)
                            .arg(buyuk.pageRange(0).satirSayisi)));
}

void TestDocumentLayout::longDescriptionDoesNotOverflowColumn()
{
    // Cok uzun bir malzeme adi satiri sarmali, sayfalamayi bozmamali.
    Canvas c;
    DocumentContext ctx = mkContext(0);
    ctx.quote.satirlar.append(mkLine(
        1,
        QStringLiteral("Alçıpan levha 12,5 mm kalınlığında, çift kat, dubel ve vida dahil, "
                       "tavan ve duvar uygulaması için hazırlanmış özel ölçü kesim malzemesi"),
        10, 18000));
    DocumentLayout layout(ctx);
    layout.paginate(&c.painter, c.pageRect);

    QVERIFY(layout.pageCount() >= 1);
    QCOMPARE(layout.pageRange(0).satirSayisi, 1);
}

void TestDocumentLayout::paintingDoesNotCrashOnEveryPage()
{
    Canvas c;
    DocumentContext ctx = mkContext(40);
    ctx.quote.sartlarMetni = QStringLiteral("Fiyatlarımıza KDV dahil değildir. "
                                             "Teklif 15 gün geçerlidir.");
    ctx.quote.satirlar[0].satirNotu = QStringLiteral("tavan dahil");
    DocumentLayout layout(ctx);
    layout.paginate(&c.painter, c.pageRect);

    for (int i = 0; i < layout.pageCount(); ++i)
        layout.paintPage(&c.painter, c.pageRect, i);

    // Sinir disi indeksler sessizce yok sayilmali, cokmemeli.
    layout.paintPage(&c.painter, c.pageRect, -1);
    layout.paintPage(&c.painter, c.pageRect, 999);
    QVERIFY(true);
}

void TestDocumentLayout::missingCompanyInfoShrinksHeader()
{
    // Firma bilgisi girilmemisse antet bos bir dikdortgen birakmamali:
    // ayni sayfaya DAHA COK kalem sigmali.
    Canvas dolu;
    DocumentLayout ile(mkContext(40));
    ile.paginate(&dolu.painter, dolu.pageRect);

    Canvas bos;
    DocumentContext ctx = mkContext(40);
    ctx.company = CompanyInfo{};
    DocumentLayout siz(ctx);
    siz.paginate(&bos.painter, bos.pageRect);

    QVERIFY2(siz.pageRange(0).satirSayisi >= ile.pageRange(0).satirSayisi,
             qPrintable(QStringLiteral("firmasiz: %1, firmali: %2")
                            .arg(siz.pageRange(0).satirSayisi)
                            .arg(ile.pageRange(0).satirSayisi)));
}

void TestDocumentLayout::paginationIsResolutionIndependent()
{
    // BU TESTIN VARLIK SEBEBI: yerlesim ekran onizlemesinde (~96 dpi),
    // yaziciya basarken (QPrinter::HighResolution = 1200 dpi) ve PDF'te
    // ayni cizim koduyla uretilir. Bosluk sabitleri cihaz pikseli cinsinden
    // verilirse 1200 dpi'de pratikte yok olur ve AYNI belge farkli sayfalanir:
    // onizlemede 2 sayfa gorunen teklif kagittan 1 sayfa cikar.
    //
    // Sabitler punto (1/72 inc) cinsinden oldugu surece her cozunurlukte
    // ayni sayfa sayisi ve ayni satir dagilimi cikmalidir.
    const int beklenenSayfa = [] {
        Canvas c(150);
        DocumentLayout l(mkContext(45));
        l.paginate(&c.painter, c.pageRect);
        return l.pageCount();
    }();
    const int beklenenIlkSayfa = [] {
        Canvas c(150);
        DocumentLayout l(mkContext(45));
        l.paginate(&c.painter, c.pageRect);
        return l.pageRange(0).satirSayisi;
    }();

    QVERIFY2(beklenenSayfa > 1, "45 kalem tek sayfaya sigmamali, test anlamsizlasir");

    for (const int dpi : {96, 300, 600, 1200}) {
        Canvas c(dpi);
        DocumentLayout layout(mkContext(45));
        layout.paginate(&c.painter, c.pageRect);

        QVERIFY2(layout.pageCount() == beklenenSayfa,
                 qPrintable(QStringLiteral("%1 dpi: %2 sayfa, 150 dpi'de %3 sayfaydi")
                                .arg(dpi).arg(layout.pageCount()).arg(beklenenSayfa)));
        QVERIFY2(layout.pageRange(0).satirSayisi == beklenenIlkSayfa,
                 qPrintable(QStringLiteral("%1 dpi: ilk sayfada %2 satir, 150 dpi'de %3 satirdi")
                                .arg(dpi).arg(layout.pageRange(0).satirSayisi).arg(beklenenIlkSayfa)));
    }
}

void TestDocumentLayout::longCompanyAddressWrapsInsteadOfBeingClipped()
{
    // Uzun bir adres KESILMEMELI, sarmali. Kesilseydi antet yuksekligi
    // degismezdi; sardigina gore antet uzar ve sayfaya daha az kalem sigar.
    // Bu testin varlik sebebi: gercek bir adresle ("Incilli Mah. Karakol
    // Cd. No: 36/B, 54500 Karasu/Sakarya") satirin sonu sessizce
    // kirpiliyordu.
    Canvas kisa;
    DocumentContext k = mkContext(60);
    k.company.adres = QStringLiteral("Ankara");
    DocumentLayout kisaLayout(k);
    kisaLayout.paginate(&kisa.painter, kisa.pageRect);

    // DIKKAT: antet yuksekligi sol (firma) ve sag ("TEKLIF / No / Tarih /
    // Gecerlilik" = 4 sabit satir) bloklarin BUYUGUDUR. Adres iki satira
    // sardiginda sol blok hala 4 satir kalir ve yukseklik degismez. Sarmanin
    // gercekten hesaba katildigini gormek icin sol blogun sagi ASMASI lazim,
    // bu yuzden adres bilerek cok uzun.
    Canvas uzun;
    DocumentContext u = mkContext(60);
    // Adres bilerek ABARTILI uzun: sarmanin etkisi antet yuksekligine
    // eklenen bir-iki satirdir ve bu tek basina bir TABLO satirini
    // dusurmeye yetmeyebilir. Etkinin olctugumuz birimde (sayfaya sigan
    // kalem sayisi) kesin gorunmesi icin adres birkac satir sarmali.
    u.company.adres = QStringLiteral(
        "İncilli Mahallesi Karakol Caddesi No: 36/B Kat 3 Daire 7, Sahil Yolu üzeri "
        "Belediye Binası karşısı, Merkez Postane yanı, Eski Sanayi Sitesi arkası, "
        "Cumhuriyet Bulvarı ile kesişim noktası, Karasu Ticaret Merkezi B Blok, "
        "54500 Karasu / Sakarya / Türkiye");
    DocumentLayout uzunLayout(u);
    uzunLayout.paginate(&uzun.painter, uzun.pageRect);

    QVERIFY2(uzunLayout.pageRange(0).satirSayisi < kisaLayout.pageRange(0).satirSayisi,
             qPrintable(QStringLiteral("uzun adres antet yuksekligini degistirmedi "
                                        "(kisa: %1 satir, uzun: %2 satir) - metin kirpiliyor olabilir")
                            .arg(kisaLayout.pageRange(0).satirSayisi)
                            .arg(uzunLayout.pageRange(0).satirSayisi)));

    // Cizim de cokmemeli.
    uzunLayout.paintPage(&uzun.painter, uzun.pageRect, 0);
}

void TestDocumentLayout::licenceLineIsAccountedFor()
{
    // Yetki belgesi satiri (bayilik bilgisi) antet yuksekligine dahil
    // edilmeli; edilmezse musteri blogu uzerine biner.
    Canvas yok;
    DocumentLayout yokLayout(mkContext(60));
    yokLayout.paginate(&yok.painter, yok.pageRect);

    Canvas var;
    DocumentContext v = mkContext(60);
    v.company.yetkiBelgesi = QStringLiteral("Aksa Doğalgaz Yetkili Firma (No: 328)");
    DocumentLayout varLayout(v);
    varLayout.paginate(&var.painter, var.pageRect);

    QVERIFY2(varLayout.pageRange(0).satirSayisi <= yokLayout.pageRange(0).satirSayisi,
             "yetki belgesi satiri antet yuksekligine katilmiyor");
    varLayout.paintPage(&var.painter, var.pageRect, 0);
}

QTEST_MAIN(TestDocumentLayout)
#include "test_document_layout.moc"
