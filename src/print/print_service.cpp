#include "print_service.h"

#include <QFileInfo>
#include <QDir>
#include <QPainter>
#include <QPrinter>

namespace {

// Dosya adına giren metni sadeleştirir: Türkçe harfler ASCII'ye iner,
// dosya sisteminin kabul etmediği karakterler ve boşluklar atılır.
QString dosyaAdiIcinSadelestir(const QString &metin)
{
    QString s = metin;
    s.replace(QChar(0x0130), QLatin1Char('I')); // İ
    s.replace(QChar(0x0131), QLatin1Char('i')); // ı
    s.replace(QChar(0x015E), QLatin1Char('S')); // Ş
    s.replace(QChar(0x015F), QLatin1Char('s')); // ş
    s.replace(QChar(0x011E), QLatin1Char('G')); // Ğ
    s.replace(QChar(0x011F), QLatin1Char('g')); // ğ
    s.replace(QChar(0x00DC), QLatin1Char('U')); // Ü
    s.replace(QChar(0x00FC), QLatin1Char('u')); // ü
    s.replace(QChar(0x00D6), QLatin1Char('O')); // Ö
    s.replace(QChar(0x00F6), QLatin1Char('o')); // ö
    s.replace(QChar(0x00C7), QLatin1Char('C')); // Ç
    s.replace(QChar(0x00E7), QLatin1Char('c')); // ç

    QString sonuc;
    sonuc.reserve(s.size());
    for (const QChar c : s) {
        if (c.isLetterOrNumber() && c.unicode() < 128)
            sonuc.append(c);
        // Boşluk ve diğer her şey atılır: "Ahmet Yılmaz" -> "AhmetYilmaz"
    }
    return sonuc;
}

} // namespace

QString PrintService::suggestedFileName(const Quote &quote, const Customer &customer)
{
    const QString yil = quote.tarih.isValid() ? QString::number(quote.tarih.year())
                                               : QStringLiteral("0000");
    const QString musteri = dosyaAdiIcinSadelestir(customer.unvan);

    QString ad = QStringLiteral("%1-%2").arg(yil, quote.teklifNo);
    if (!musteri.isEmpty())
        ad += QLatin1Char('_') + musteri;
    return ad + QStringLiteral(".pdf");
}

bool PrintService::paint(const DocumentContext &ctx, QPrinter *printer, QString *errorOut)
{
    if (!printer) {
        if (errorOut)
            *errorOut = QStringLiteral("Yazıcı belirtilmedi.");
        return false;
    }
    // Yazıcı yoksa/hazır değilse çökmek yerine anlamlı hata döndürülür.
    if (!printer->isValid()) {
        if (errorOut)
            *errorOut = QStringLiteral("Yazıcı kullanılabilir değil. "
                                        "Bir yazıcı kurulu olduğundan emin olun.");
        return false;
    }

    QPainter painter;
    if (!painter.begin(printer)) {
        if (errorOut)
            *errorOut = QStringLiteral("Yazdırma başlatılamadı. Yazıcı meşgul olabilir.");
        return false;
    }

    // Çizilebilir alan: QPrinter kenar boşluklarını zaten düşer, bu yüzden
    // (0,0) çizilebilir bölgenin sol üstüdür.
    const QRectF sayfaAlani(0, 0, painter.viewport().width(), painter.viewport().height());

    DocumentLayout layout(ctx);
    layout.paginate(&painter, sayfaAlani);

    for (int i = 0; i < layout.pageCount(); ++i) {
        if (i > 0)
            printer->newPage();
        layout.paintPage(&painter, sayfaAlani, i);
    }

    painter.end();
    return true;
}

bool PrintService::exportPdf(const DocumentContext &ctx, const QString &filePath, QString *errorOut)
{
    if (filePath.trimmed().isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("PDF dosya yolu boş.");
        return false;
    }

    // Hedef klasör yoksa oluştur; kullanıcı Ayarlar'da henüz var olmayan bir
    // klasör seçmiş olabilir.
    const QDir klasor = QFileInfo(filePath).absoluteDir();
    if (!klasor.exists() && !QDir().mkpath(klasor.absolutePath())) {
        if (errorOut)
            *errorOut = QStringLiteral("Klasör oluşturulamadı: %1").arg(klasor.absolutePath());
        return false;
    }

    // PdfFormat: Qt'nin kendi PDF motoru. Sistemde yazıcı KURULU OLMASA da
    // çalışır ve metin seçilebilir kalır (görüntüye dönüştürülmez).
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filePath);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

    QPainter painter;
    if (!painter.begin(&printer)) {
        if (errorOut)
            *errorOut = QStringLiteral("PDF yazılamadı: %1").arg(filePath);
        return false;
    }

    const QRectF sayfaAlani(0, 0, painter.viewport().width(), painter.viewport().height());

    DocumentLayout layout(ctx);
    layout.paginate(&painter, sayfaAlani);

    for (int i = 0; i < layout.pageCount(); ++i) {
        if (i > 0)
            printer.newPage();
        layout.paintPage(&painter, sayfaAlani, i);
    }

    painter.end();
    return true;
}
