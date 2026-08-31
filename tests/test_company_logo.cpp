#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>
#include <QSqlDatabase>
#include <QTemporaryDir>

#include "core/db.h"
#include "core/settings.h"
#include "print/company_logo.h"
#include "print/document_layout.h"

namespace {

QImage mkLogo(int w, int h, QColor renk = Qt::blue)
{
    QImage img(w, h, QImage::Format_ARGB32);
    img.fill(renk);
    return img;
}

DocumentContext mkContext(int satirSayisi, const QImage &logo = QImage())
{
    DocumentContext ctx;
    ctx.company.unvan = QStringLiteral("Öz Yapı İnşaat Ltd. Şti.");
    ctx.company.adres = QStringLiteral("Atatürk Cad. No:1, Ankara");
    ctx.company.telefon = QStringLiteral("0312 111 11 11");
    ctx.customer.unvan = QStringLiteral("Ahmet Yılmaz");
    ctx.quote.teklifNo = QStringLiteral("000143");
    ctx.quote.tarih = QDate(2026, 8, 25);
    ctx.logo = logo;
    for (int i = 0; i < satirSayisi; ++i) {
        QuoteLine l;
        l.sira = i + 1; l.aciklama = QStringLiteral("Kalem %1").arg(i + 1);
        l.birim = QStringLiteral("adet"); l.miktar = 2;
        l.birimFiyat = Money(18000); l.tutar = Money(36000);
        ctx.quote.satirlar.append(l);
    }
    return ctx;
}

struct Canvas
{
    QImage image;
    QPainter painter;
    QRectF pageRect;

    explicit Canvas(int dpi = 150)
        : image(static_cast<int>(8.27 * dpi), static_cast<int>(11.69 * dpi), QImage::Format_RGB32)
    {
        image.fill(Qt::white);
        image.setDotsPerMeterX(static_cast<int>(dpi * 39.3701));
        image.setDotsPerMeterY(static_cast<int>(dpi * 39.3701));
        painter.begin(&image);
        const double kenar = dpi * 15.0 / 25.4;
        pageRect = QRectF(kenar, kenar, image.width() - 2 * kenar, image.height() - 2 * kenar);
    }
    ~Canvas() { painter.end(); }
};

} // namespace

class TestCompanyLogo : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // --- saklama ---
    void loadReturnsNullWhenUnset();
    void saveThenLoadRoundTrip();
    void largeLogoIsScaledDown();
    void smallLogoIsNotUpscaled();
    void clearRemovesLogo();
    void corruptDataReturnsNullNotCrash();
    void saveRejectsNullImage();
    void alphaChannelSurvives();

    // --- yerleşim ---
    void noLogoLeavesHeaderUnchanged();
    void logoShiftsCompanyTextRight();
    void wideLogoIsCappedInWidth();
    void logoDoesNotBreakPagination();

private:
    QTemporaryDir *m_dir = nullptr;
    QString m_conn;
    QSqlDatabase m_db;
};

void TestCompanyLogo::init()
{
    m_dir = new QTemporaryDir();
    QVERIFY(m_dir->isValid());
    m_conn = QStringLiteral("cl_%1").arg(QDateTime::currentMSecsSinceEpoch());
    QString err;
    QVERIFY2(Db::openAndMigrate(m_dir->filePath(QStringLiteral("t.db")), &err, m_conn), qPrintable(err));
    m_db = QSqlDatabase::database(m_conn);
}

void TestCompanyLogo::cleanup()
{
    m_db = QSqlDatabase();
    QSqlDatabase::database(m_conn).close();
    QSqlDatabase::removeDatabase(m_conn);
    delete m_dir;
    m_dir = nullptr;
}

void TestCompanyLogo::loadReturnsNullWhenUnset()
{
    // "Logo yok" hata degil, olagan durum.
    Settings s(m_db);
    QVERIFY(CompanyLogo::load(s).isNull());
}

void TestCompanyLogo::saveThenLoadRoundTrip()
{
    Settings s(m_db);
    QString err;
    QVERIFY2(CompanyLogo::save(s, mkLogo(200, 100), &err), qPrintable(err));

    const QImage geri = CompanyLogo::load(s);
    QVERIFY(!geri.isNull());
    QCOMPARE(geri.width(), 200);
    QCOMPARE(geri.height(), 100);
}

void TestCompanyLogo::largeLogoIsScaledDown()
{
    // Kullanici 4000 pikselllik bir fotograf secerse veritabani sismemeli.
    Settings s(m_db);
    QVERIFY(CompanyLogo::save(s, mkLogo(4000, 2000)));

    const QImage geri = CompanyLogo::load(s);
    QVERIFY(!geri.isNull());
    QVERIFY2(geri.width() <= CompanyLogo::kMaxBoyut && geri.height() <= CompanyLogo::kMaxBoyut,
             qPrintable(QStringLiteral("olceklenmedi: %1x%2").arg(geri.width()).arg(geri.height())));
    // En-boy orani korunmali (2:1).
    QCOMPARE(geri.width() / geri.height(), 2);

    // Saklanan veri de makul boyutta olmali.
    const QString base64 = s.valueOr(Settings::keyCompanyLogo());
    QVERIFY2(base64.size() < 400000,
             qPrintable(QStringLiteral("saklanan logo %1 karakter").arg(base64.size())));
}

void TestCompanyLogo::smallLogoIsNotUpscaled()
{
    // Kucuk bir logoyu buyutmek onu bulaniklastirirdi.
    Settings s(m_db);
    QVERIFY(CompanyLogo::save(s, mkLogo(80, 40)));
    const QImage geri = CompanyLogo::load(s);
    QCOMPARE(geri.width(), 80);
    QCOMPARE(geri.height(), 40);
}

void TestCompanyLogo::clearRemovesLogo()
{
    Settings s(m_db);
    QVERIFY(CompanyLogo::save(s, mkLogo(100, 50)));
    QVERIFY(!CompanyLogo::load(s).isNull());

    QVERIFY(CompanyLogo::clear(s));
    QVERIFY(CompanyLogo::load(s).isNull());
}

void TestCompanyLogo::corruptDataReturnsNullNotCrash()
{
    // Elle duzenlenmis / bozulmus veri programi cokertmemeli, belge
    // logosuz basilmali.
    Settings s(m_db);
    QVERIFY(s.setValue(Settings::keyCompanyLogo(), QStringLiteral("bu base64 degil!!!")));
    QVERIFY(CompanyLogo::load(s).isNull());

    QVERIFY(s.setValue(Settings::keyCompanyLogo(), QStringLiteral("YWJjZGVm"))); // gecerli base64, gecersiz PNG
    QVERIFY(CompanyLogo::load(s).isNull());
}

void TestCompanyLogo::saveRejectsNullImage()
{
    Settings s(m_db);
    QString err;
    QVERIFY(!CompanyLogo::save(s, QImage(), &err));
    QVERIFY(!err.isEmpty());
}

void TestCompanyLogo::alphaChannelSurvives()
{
    // PNG secilmesinin sebebi seffaflik: antette beyaz bir kutu olusmamali.
    QImage img(50, 50, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.fillRect(0, 0, 25, 25, Qt::red);
    p.end();

    Settings s(m_db);
    QVERIFY(CompanyLogo::save(s, img));
    const QImage geri = CompanyLogo::load(s);
    QVERIFY(!geri.isNull());
    QVERIFY2(geri.hasAlphaChannel(), "seffaflik kayboldu");
    QCOMPARE(qAlpha(geri.pixel(40, 40)), 0);
}

// ---------------------------------------------------------------------------

void TestCompanyLogo::noLogoLeavesHeaderUnchanged()
{
    // Logo verilmezse antet icin hic yer AYRILMAMALI: ayni sayfaya ayni
    // sayida kalem sigmali.
    Canvas c1;
    DocumentLayout logosuz(mkContext(60));
    logosuz.paginate(&c1.painter, c1.pageRect);

    Canvas c2;
    DocumentContext ctx = mkContext(60);
    ctx.logo = QImage(); // acikca bos
    DocumentLayout bos(ctx);
    bos.paginate(&c2.painter, c2.pageRect);

    QCOMPARE(bos.pageRange(0).satirSayisi, logosuz.pageRange(0).satirSayisi);
}

void TestCompanyLogo::logoShiftsCompanyTextRight()
{
    // Logo eklenince antet en fazla logo yuksekligi kadar buyumeli; firma
    // metni logonun YANINA gectigi icin satirlar alt alta eklenmemeli.
    Canvas c1;
    DocumentLayout logosuz(mkContext(60));
    logosuz.paginate(&c1.painter, c1.pageRect);
    const int logosuzSatir = logosuz.pageRange(0).satirSayisi;

    Canvas c2;
    DocumentLayout logolu(mkContext(60, mkLogo(200, 100)));
    logolu.paginate(&c2.painter, c2.pageRect);
    const int logoluSatir = logolu.pageRange(0).satirSayisi;

    // Logo yer kaplar, ama az: birkac satirdan fazlasini yememeli.
    QVERIFY2(logoluSatir <= logosuzSatir,
             "logo antet yuksekligini azaltamaz");
    QVERIFY2(logosuzSatir - logoluSatir <= 3,
             qPrintable(QStringLiteral("logo %1 satir yedi (logosuz %2, logolu %3)")
                            .arg(logosuzSatir - logoluSatir).arg(logosuzSatir).arg(logoluSatir)));

    // Cizim cokmemeli.
    logolu.paintPage(&c2.painter, c2.pageRect, 0);
}

void TestCompanyLogo::wideLogoIsCappedInWidth()
{
    // Cok yatay bir logo (10:1) anteti yutmamali; genislik sinirlanir ve
    // firma metnine yer kalir.
    Canvas c;
    DocumentLayout layout(mkContext(60, mkLogo(2000, 200)));
    layout.paginate(&c.painter, c.pageRect);
    layout.paintPage(&c.painter, c.pageRect, 0);

    // Sayfa yine de kalem alabilmeli.
    QVERIFY2(layout.pageRange(0).satirSayisi > 10,
             qPrintable(QStringLiteral("genis logo sayfayi yuttu: %1 satir")
                            .arg(layout.pageRange(0).satirSayisi)));
}

void TestCompanyLogo::logoDoesNotBreakPagination()
{
    // Logolu belgede de her satir tam bir kez basilmali ve toplamlar tek
    // sayfada kalmali.
    Canvas c;
    const int n = 80;
    DocumentLayout layout(mkContext(n, mkLogo(300, 150)));
    layout.paginate(&c.painter, c.pageRect);

    int toplam = 0, toplamlarSayfasi = 0, beklenen = 0;
    for (int i = 0; i < layout.pageCount(); ++i) {
        const auto r = layout.pageRange(i);
        QCOMPARE(r.ilkSatir, beklenen);
        toplam += r.satirSayisi;
        beklenen = r.ilkSatir + r.satirSayisi;
        if (r.toplamlarBurada)
            ++toplamlarSayfasi;
    }
    QCOMPARE(toplam, n);
    QCOMPARE(toplamlarSayfasi, 1);
}

QTEST_MAIN(TestCompanyLogo)
#include "test_company_logo.moc"
