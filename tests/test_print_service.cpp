#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>

#include "print/print_service.h"

namespace {

DocumentContext mkContext(int satirSayisi)
{
    DocumentContext ctx;
    ctx.company.unvan = QStringLiteral("Öz Yapı İnşaat Ltd. Şti.");
    ctx.customer.unvan = QStringLiteral("Ahmet Yılmaz");
    ctx.quote.teklifNo = QStringLiteral("0043");
    ctx.quote.tarih = QDate(2026, 8, 25);
    for (int i = 0; i < satirSayisi; ++i) {
        QuoteLine l;
        l.sira = i + 1;
        l.aciklama = QStringLiteral("İşçilik %1").arg(i + 1);
        l.birim = QStringLiteral("saat");
        l.miktar = 2;
        l.birimFiyat = Money(35000);
        l.tutar = Money(70000);
        ctx.quote.satirlar.append(l);
    }
    return ctx;
}

} // namespace

class TestPrintService : public QObject
{
    Q_OBJECT

private slots:
    void suggestedNameFormat();
    void suggestedNameStripsTurkishChars();
    void suggestedNameHandlesEmptyCustomer();
    void suggestedNameHandlesInvalidDate();
    void exportPdfCreatesFile();
    void exportPdfIsRealPdfWithSelectableText();
    void exportPdfMultiPage();
    void exportPdfCreatesMissingFolder();
    void exportPdfRejectsEmptyPath();
    void paintRejectsNullPrinter();
};

void TestPrintService::suggestedNameFormat()
{
    Quote q;
    q.teklifNo = QStringLiteral("0043");
    q.tarih = QDate(2026, 8, 25);
    Customer c;
    c.unvan = QStringLiteral("Ahmet Yilmaz");
    QCOMPARE(PrintService::suggestedFileName(q, c), QStringLiteral("2026-0043_AhmetYilmaz.pdf"));
}

void TestPrintService::suggestedNameStripsTurkishChars()
{
    // Turkce harfler ve dosya sisteminin sevmedigi karakterler ASCII'ye iner.
    Quote q;
    q.teklifNo = QStringLiteral("000143");
    q.tarih = QDate(2026, 1, 5);
    Customer c;
    c.unvan = QStringLiteral("Şükrü Çelik İnşaat / Ltd.");
    const QString ad = PrintService::suggestedFileName(q, c);
    QCOMPARE(ad, QStringLiteral("2026-000143_SukruCelikInsaatLtd.pdf"));
    QVERIFY(!ad.contains(QLatin1Char('/')));
}

void TestPrintService::suggestedNameHandlesEmptyCustomer()
{
    Quote q;
    q.teklifNo = QStringLiteral("0001");
    q.tarih = QDate(2026, 3, 1);
    QCOMPARE(PrintService::suggestedFileName(q, Customer{}), QStringLiteral("2026-0001.pdf"));
}

void TestPrintService::suggestedNameHandlesInvalidDate()
{
    Quote q;
    q.teklifNo = QStringLiteral("0001");
    // tarih atanmadi -> gecersiz QDate; cokme yerine "0000" kullanilir.
    Customer c;
    c.unvan = QStringLiteral("Test");
    QCOMPARE(PrintService::suggestedFileName(q, c), QStringLiteral("0000-0001_Test.pdf"));
}

void TestPrintService::exportPdfCreatesFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString yol = dir.filePath(QStringLiteral("teklif.pdf"));

    QString err;
    QVERIFY2(PrintService::exportPdf(mkContext(3), yol, &err), qPrintable(err));
    QVERIFY(QFile::exists(yol));
    QVERIFY(QFileInfo(yol).size() > 0);
}

void TestPrintService::exportPdfIsRealPdfWithSelectableText()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString yol = dir.filePath(QStringLiteral("teklif.pdf"));

    QString err;
    QVERIFY2(PrintService::exportPdf(mkContext(3), yol, &err), qPrintable(err));

    QFile f(yol);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray icerik = f.readAll();

    // Gercek bir PDF mi (goruntu degil)?
    QVERIFY2(icerik.startsWith("%PDF"), "cikti PDF baslığıyla baslamiyor");
    // Metin gomulu mu: PDF'te yazi tipi nesnesi varsa metin secilebilirdir.
    // Goruntuye donusturulmus bir cikti /Font icermezdi.
    QVERIFY2(icerik.contains("/Font"), "PDF'te yazi tipi yok - metin goruntuye donusmus olabilir");
}

void TestPrintService::exportPdfMultiPage()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString tek = dir.filePath(QStringLiteral("tek.pdf"));
    const QString cok = dir.filePath(QStringLiteral("cok.pdf"));
    QString err;
    QVERIFY2(PrintService::exportPdf(mkContext(3), tek, &err), qPrintable(err));
    QVERIFY2(PrintService::exportPdf(mkContext(80), cok, &err), qPrintable(err));

    // 80 kalemlik belge 3 kalemlikten buyuk olmali (cok sayfa uretildi).
    QVERIFY(QFileInfo(cok).size() > QFileInfo(tek).size());
}

void TestPrintService::exportPdfCreatesMissingFolder()
{
    // Kullanici Ayarlar'da henuz var olmayan bir klasor secmis olabilir.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString yol = dir.filePath(QStringLiteral("yeni/alt/teklif.pdf"));

    QString err;
    QVERIFY2(PrintService::exportPdf(mkContext(1), yol, &err), qPrintable(err));
    QVERIFY(QFile::exists(yol));
}

void TestPrintService::exportPdfRejectsEmptyPath()
{
    QString err;
    QVERIFY(!PrintService::exportPdf(mkContext(1), QString(), &err));
    QVERIFY(!err.isEmpty());
}

void TestPrintService::paintRejectsNullPrinter()
{
    // Yazici yokken "Yazdir" anlamli hata vermeli, cokmemeli.
    QString err;
    QVERIFY(!PrintService::paint(mkContext(1), nullptr, &err));
    QVERIFY(!err.isEmpty());
}

QTEST_MAIN(TestPrintService)
#include "test_print_service.moc"
