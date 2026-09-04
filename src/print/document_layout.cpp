#include "teklif/print/document_layout.h"

#include "teklif/core/numtowords.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QPaintDevice>
#include <utility>

namespace {

// Sütun genişlikleri, sayfa genişliğinin oranı olarak. Toplamları 1,0 olmalı.
// Oran kullanılıyor çünkü aynı yerleşim A4'te de, farklı kâğıtta da,
// ekran önizlemesinde de çalışmalı.
constexpr double kOranSira     = 0.06;
constexpr double kOranAciklama = 0.44;
constexpr double kOranBirim    = 0.10;
constexpr double kOranMiktar   = 0.12;
constexpr double kOranFiyat    = 0.14;
constexpr double kOranTutar    = 0.14;

// Boşluklar PUNTO cinsinden tanımlanır, cihaz pikseli olarak DEĞİL.
//
// NEDEN: yerleşim üç farklı çözünürlükte çizilir — ekran önizlemesi (~96 dpi),
// yazıcı (QPrinter::HighResolution, 1200 dpi) ve PDF. Sabitler piksel olarak
// verilirse 1200 dpi'de 5 piksel 0,1 mm'ye düşer, boşluklar pratikte yok olur
// ve AYNI belge çıktı aygıtına göre farklı sayfalanır. Punto ölçüsü aygıttan
// bağımsızdır: 1 punto her zaman 1/72 inçtir.
constexpr double kHucreBoslukPt   = 3.0; // hücre içi yatay/dikey pay
constexpr double kBlokBoslukPt    = 7.0; // bloklar arası dikey boşluk
constexpr double kSatirEkBoslukPt = 4.0; // satırlara eklenen nefes payı

// Logonun antetteki yüksekliği (punto). ~15 mm: antette baskın olmayacak
// ama okunacak kadar. Genişlik en-boy oranından türetilir ve sayfa
// genişliğinin %35'ini aşamaz — çok yatay bir logo anteti yutmasın diye.
constexpr double kLogoYuksekligiPt = 42.0;
constexpr double kLogoMaksGenislikOran = 0.35;
constexpr double kLogoSagBoslukPt = 8.0;

// Antetin dikey bölünme noktası: solda firma bilgisi, sağda "TEKLİF / No /
// Tarih / Geçerlilik". Sağ blok yalnızca dört kısa satır taşır, bu yüzden
// yerin çoğu sola bırakılır — adres genelde en uzun satırdır ve gereksiz
// yere sarmamalıdır.
// TEK SABİT: ölçüm ve çizim aynı değeri kullanmak zorunda, ayrışırlarsa
// metin ya kesilir ya taşar.
constexpr double kAntetBolunmeOrani = 0.60;

// Puntoyu, çizim yapılan aygıtın piksel ölçüsüne çevirir.
double px(const QPainter *p, double punto)
{
    return punto * p->device()->logicalDpiY() / 72.0;
}

QString formatMiktar(double m)
{
    QString s = QString::number(m, 'f', 3);
    while (s.endsWith(QLatin1Char('0')))
        s.chop(1);
    if (s.endsWith(QLatin1Char('.')))
        s.chop(1);
    s.replace(QLatin1Char('.'), QLatin1Char(','));
    return s;
}

} // namespace

DocumentLayout::DocumentLayout(DocumentContext context) : m_ctx(std::move(context)) {}

QFont DocumentLayout::baseFont() const
{
    QFont f;
    f.setPointSize(m_ctx.fontPt);
    return f;
}

QFont DocumentLayout::boldFont() const
{
    QFont f = baseFont();
    f.setBold(true);
    return f;
}

QFont DocumentLayout::smallFont() const
{
    QFont f;
    // Alt bilgi ve "devam" satırı gövdeden bir tık küçük, ama 7 pt'nin
    // altına inmez — 8 pt belge boyutunda okunaksız olurdu.
    f.setPointSize(qMax(7, m_ctx.fontPt - 2));
    return f;
}

DocumentLayout::Columns DocumentLayout::columnsFor(const QRectF &pageRect) const
{
    const double w = pageRect.width();
    double x = pageRect.left();

    auto sut = [&](double oran) {
        const QRectF r(x, 0, w * oran, 0);
        x += w * oran;
        return r;
    };

    Columns c;
    c.sira = sut(kOranSira);
    c.aciklama = sut(kOranAciklama);
    c.birim = sut(kOranBirim);
    c.miktar = sut(kOranMiktar);
    c.fiyat = sut(kOranFiyat);
    c.tutar = sut(kOranTutar);
    return c;
}

// ---------------------------------------------------------------------------
// Ölçüm: paginate() ve paintPage() AYNI yükseklikleri kullanmak zorunda,
// yoksa sayfa taşar. Bu yüzden ölçüm ayrı fonksiyonlarda toplandı.
// ---------------------------------------------------------------------------

QRectF DocumentLayout::logoRect(QPainter *p, const QRectF &pageRect) const
{
    if (m_ctx.logo.isNull())
        return {}; // logo yok -> hiç yer ayrılmaz

    const double h = px(p, kLogoYuksekligiPt);
    // En-boy oranı korunur; sıfıra bölme olmaması için yükseklik kontrol edilir.
    const double oran = m_ctx.logo.height() > 0
                            ? static_cast<double>(m_ctx.logo.width()) / m_ctx.logo.height()
                            : 1.0;
    const double w = qMin(h * oran, pageRect.width() * kLogoMaksGenislikOran);

    return QRectF(pageRect.left(), pageRect.top(), w, h);
}

QRectF DocumentLayout::companyTextRect(QPainter *p, const QRectF &pageRect) const
{
    const double sagX = pageRect.left() + pageRect.width() * kAntetBolunmeOrani;
    const QRectF logo = logoRect(p, pageRect);

    double x = pageRect.left();
    if (!logo.isNull())
        x = logo.right() + px(p, kLogoSagBoslukPt);

    const double w = qMax(sagX - x - px(p, kLogoSagBoslukPt), px(p, 40.0));
    return QRectF(x, pageRect.top(), w, 0);
}

QStringList DocumentLayout::companyLines() const
{
    QStringList satirlar;

    // Vergi dairesi ve numarası çıplak bir sayı olarak basılmamalı;
    // etiketlenip TEK satırda birleştirilir. İkisinden biri boşsa yalnızca
    // dolu olan yazılır.
    QStringList vergi;
    if (!m_ctx.company.vergiDairesi.trimmed().isEmpty())
        vergi << QStringLiteral("V.D.: %1").arg(m_ctx.company.vergiDairesi.trimmed());
    if (!m_ctx.company.vergiNo.trimmed().isEmpty())
        vergi << QStringLiteral("V.No: %1").arg(m_ctx.company.vergiNo.trimmed());

    for (const QString &s : {m_ctx.company.adres, m_ctx.company.telefon, m_ctx.company.email,
                              vergi.join(QStringLiteral("   "))}) {
        if (!s.trimmed().isEmpty())
            satirlar << s.trimmed();
    }
    return satirlar;
}

double DocumentLayout::measureHeader(QPainter *p, const QRectF &pageRect) const
{
    const QFontMetricsF fm(baseFont(), p->device());
    const QFontMetricsF sfm(smallFont(), p->device());
    const double satir = fm.height() + 2.0;

    const QRectF metinAlani = companyTextRect(p, pageRect);

    // Sol blok: firma bilgisi. Uzun bir adres SARAR, kesilmez — bu yüzden
    // yükseklik sabit satır sayısıyla değil, gerçekten ölçülerek bulunur.
    // Boş alanlar hiç satır AÇMAZ: firma bilgisi girilmemişse antet
    // kendiliğinden kısalır.
    double solYukseklik = 0.0;
    if (!m_ctx.company.unvan.trimmed().isEmpty())
        solYukseklik += satir;
    if (!m_ctx.company.yetkiBelgesi.trimmed().isEmpty()) {
        solYukseklik += sfm.boundingRect(QRectF(0, 0, metinAlani.width(), 0), Qt::TextWordWrap,
                                          m_ctx.company.yetkiBelgesi).height();
    }
    for (const QString &l : companyLines()) {
        solYukseklik += fm.boundingRect(QRectF(0, 0, metinAlani.width(), 0), Qt::TextWordWrap, l)
                            .height();
    }

    // Sağ blok: "TEKLİF" başlığı + no + tarih + geçerlilik = 4 satır sabit.
    const double sagYukseklik = 4 * satir;

    // Müşteri bloğu: unvan her zaman, diğerleri doluysa.
    int musteriSatir = 1;
    for (const QString &s : {m_ctx.quote.musteri.yetkili, m_ctx.quote.musteri.telefon, m_ctx.quote.musteri.adres,
                              m_ctx.quote.musteri.vergiDairesi + m_ctx.quote.musteri.vergiNo}) {
        if (!s.trimmed().isEmpty())
            ++musteriSatir;
    }

    // Logo, firma metninden uzun olabilir.
    const QRectF logo = logoRect(p, pageRect);
    const double solBlokYuksekligi = qMax(logo.height(), solYukseklik);

    double h = qMax(solBlokYuksekligi, sagYukseklik) + px(p, kBlokBoslukPt);
    h += satir;                               // "SAYIN" etiketi
    h += musteriSatir * satir + px(p, kBlokBoslukPt);

    if (!m_ctx.quote.projeBasligi.trimmed().isEmpty())
        h += satir + px(p, kBlokBoslukPt);

    return h;
}

double DocumentLayout::measureTotals(QPainter *p, const QRectF &pageRect) const
{
    Q_UNUSED(pageRect);
    const QFontMetricsF fm(baseFont(), p->device());
    const double satir = fm.height() + 2.0;

    // KDV'li kayıtta üç satır (ara toplam + KDV + genel toplam), KDV'siz
    // kayıtta yalnızca genel toplam. paintTotals ile AYNI koşula bakmak
    // zorunda: ayrışırlarsa toplam bloğu için ayrılan yer yanlış olur.
    double h = px(p, kBlokBoslukPt) + (m_ctx.quote.kdvTutari.isZero() ? 1 : 3) * satir;
    // "Yazıyla" satırı her zaman basılır — sözleşme belgelerinde beklenir.
    h += satir + px(p, kHucreBoslukPt);

    if (!m_ctx.quote.sartlarMetni.trimmed().isEmpty()) {
        const QFontMetricsF sfm(smallFont(), p->device());
        const QRectF gerekli = sfm.boundingRect(QRectF(0, 0, pageRect.width(), 0),
                                                 Qt::TextWordWrap, m_ctx.quote.sartlarMetni);
        h += px(p, kBlokBoslukPt) + gerekli.height();
    }
    return h;
}

double DocumentLayout::measureRow(QPainter *p, const QRectF &pageRect, const QuoteLine &l) const
{
    const QFontMetricsF fm(baseFont(), p->device());
    const Columns c = columnsFor(pageRect);

    // Açıklama sütunu sarabilir; satır yüksekliğini o belirler. Satır notu
    // varsa küçük puntoyla açıklamanın altına eklenir.
    QString metin = l.aciklama;
    const QRectF gerekli = fm.boundingRect(QRectF(0, 0, c.aciklama.width() - 2 * px(p, kHucreBoslukPt), 0),
                                            Qt::TextWordWrap, metin);
    double h = gerekli.height();

    if (!l.satirNotu.trimmed().isEmpty()) {
        const QFontMetricsF sfm(smallFont(), p->device());
        const QRectF notRect = sfm.boundingRect(QRectF(0, 0, c.aciklama.width() - 2 * px(p, kHucreBoslukPt), 0),
                                                 Qt::TextWordWrap, l.satirNotu);
        h += notRect.height();
    }

    return qMax(h, fm.height()) + 2 * px(p, kHucreBoslukPt) + px(p, kSatirEkBoslukPt);
}

// ---------------------------------------------------------------------------
// Sayfalama
// ---------------------------------------------------------------------------

void DocumentLayout::paginate(QPainter *painter, const QRectF &pageRect)
{
    m_pages.clear();
    m_satirYukseklikleri.clear();

    const QFontMetricsF fm(baseFont(), painter->device());
    m_satirYuksekligi = fm.height() + 2.0;
    m_antetYuksekligi = measureHeader(painter, pageRect);
    m_tabloBasligiYuksekligi = fm.height() + 2 * px(painter, kHucreBoslukPt);
    m_toplamlarYuksekligi = measureTotals(painter, pageRect);

    const QFontMetricsF sfm(smallFont(), painter->device());
    m_altBilgiYuksekligi = sfm.height() + px(painter, kHucreBoslukPt);

    for (const QuoteLine &l : m_ctx.quote.satirlar)
        m_satirYukseklikleri.append(measureRow(painter, pageRect, l));

    const int satirSayisi = m_ctx.quote.satirlar.size();
    int i = 0;

    while (true) {
        PageRange sayfa;
        sayfa.ilkSatir = i;

        // Her sayfada antet ve tablo başlığı tekrarlanır; alt bilgi için de
        // yer ayrılır.
        double kullanilabilir = pageRect.height() - m_antetYuksekligi
                                 - m_tabloBasligiYuksekligi - m_altBilgiYuksekligi;

        while (i < satirSayisi && m_satirYukseklikleri.at(i) <= kullanilabilir) {
            kullanilabilir -= m_satirYukseklikleri.at(i);
            ++i;
            ++sayfa.satirSayisi;
        }

        // Bir satır hiçbir sayfaya sığmıyorsa (aşırı uzun açıklama) yine de
        // yerleştir; sonsuz döngüye girmektense taşmayı kabul et.
        if (sayfa.satirSayisi == 0 && i < satirSayisi) {
            ++i;
            sayfa.satirSayisi = 1;
            kullanilabilir = 0;
        }

        const bool sonSatirlarBitti = (i >= satirSayisi);
        // TOPLAMLAR BLOĞU ASLA YALNIZ KALMAZ: son sayfada yer varsa oraya
        // konur; yoksa yeni bir sayfa açılır ama o sayfada da tablo başlığı
        // ve "devam" satırı bulunur.
        if (sonSatirlarBitti && m_toplamlarYuksekligi <= kullanilabilir) {
            sayfa.toplamlarBurada = true;
            m_pages.append(sayfa);
            break;
        }

        m_pages.append(sayfa);

        if (sonSatirlarBitti) {
            PageRange sonSayfa;
            sonSayfa.ilkSatir = satirSayisi;
            sonSayfa.satirSayisi = 0;
            sonSayfa.toplamlarBurada = true;
            m_pages.append(sonSayfa);
            break;
        }
    }

    if (m_pages.isEmpty()) {
        PageRange tek;
        tek.toplamlarBurada = true;
        m_pages.append(tek);
    }
}

DocumentLayout::PageRange DocumentLayout::pageRange(int pageIndex) const
{
    if (pageIndex < 0 || pageIndex >= m_pages.size())
        return {};
    return m_pages.at(pageIndex);
}

// ---------------------------------------------------------------------------
// Çizim
// ---------------------------------------------------------------------------

void DocumentLayout::paintHeader(QPainter *p, const QRectF &pageRect, double &y) const
{
    const QFontMetricsF fm(baseFont(), p->device());
    const double satir = fm.height() + 2.0;
    const double sagX = pageRect.left() + pageRect.width() * kAntetBolunmeOrani;
    const double sagW = pageRect.width() * (1.0 - kAntetBolunmeOrani);

    // Logo varsa sol üste çizilir ve firma metni onun SAĞINDAN başlar.
    // Logo yoksa metinX sayfanın sol kenarıdır — yani logosuz düzen
    // birebir eskisi gibi kalır.
    const QRectF logo = logoRect(p, pageRect);
    if (!logo.isNull())
        p->drawImage(logo, m_ctx.logo);

    const QRectF metinAlani = companyTextRect(p, pageRect);
    const double metinX = metinAlani.x();
    const double metinW = metinAlani.width();

    double solY = y;
    p->setFont(boldFont());
    if (!m_ctx.company.unvan.trimmed().isEmpty()) {
        p->drawText(QRectF(metinX, solY, metinW, satir),
                     Qt::AlignLeft | Qt::AlignVCenter, m_ctx.company.unvan);
        solY += satir;
    }

    // Yetki belgesi unvanın hemen altında, gövdeden bir tık küçük: kimlik
    // bilgisidir ama unvanla aynı ağırlıkta olmamalı.
    if (!m_ctx.company.yetkiBelgesi.trimmed().isEmpty()) {
        p->setFont(smallFont());
        const QFontMetricsF sfm(smallFont(), p->device());
        const double h = sfm.boundingRect(QRectF(0, 0, metinW, 0), Qt::TextWordWrap,
                                           m_ctx.company.yetkiBelgesi).height();
        p->drawText(QRectF(metinX, solY, metinW, h),
                     Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, m_ctx.company.yetkiBelgesi);
        solY += h;
    }

    // Adres uzun olabilir; SARAR, kesilmez. Ölçüm de aynı genişlikle
    // yapıldığı için antet yüksekliği buna göre hesaplanmıştır.
    p->setFont(baseFont());
    for (const QString &l : companyLines()) {
        const double h = fm.boundingRect(QRectF(0, 0, metinW, 0), Qt::TextWordWrap, l).height();
        p->drawText(QRectF(metinX, solY, metinW, h),
                     Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, l);
        solY += h;
    }

    double sagY = y;
    p->setFont(boldFont());
    p->drawText(QRectF(sagX, sagY, sagW, satir), Qt::AlignRight | Qt::AlignVCenter,
                 QStringLiteral("TEKLİF"));
    sagY += satir;
    p->setFont(baseFont());
    p->drawText(QRectF(sagX, sagY, sagW, satir), Qt::AlignRight | Qt::AlignVCenter,
                 QStringLiteral("No: %1").arg(m_ctx.quote.teklifNo));
    sagY += satir;
    p->drawText(QRectF(sagX, sagY, sagW, satir), Qt::AlignRight | Qt::AlignVCenter,
                 QStringLiteral("Tarih: %1").arg(m_ctx.quote.tarih.toString(QStringLiteral("dd.MM.yyyy"))));
    sagY += satir;
    p->drawText(QRectF(sagX, sagY, sagW, satir), Qt::AlignRight | Qt::AlignVCenter,
                 QStringLiteral("Geçerlilik: %1 gün").arg(m_ctx.quote.gecerlilikGun));
    sagY += satir;

    // Logo, firma metninden uzun olabilir; antetin alt sınırı üçünün de
    // altında kalmalı, yoksa müşteri bloğu logonun üzerine biner.
    const double logoAlt = logo.isNull() ? 0.0 : logo.bottom();
    y = qMax(qMax(solY, sagY), logoAlt) + px(p, kBlokBoslukPt);

    p->setFont(smallFont());
    p->drawText(QRectF(pageRect.left(), y, pageRect.width(), satir),
                 Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("SAYIN"));
    y += satir;

    p->setFont(boldFont());
    p->drawText(QRectF(pageRect.left(), y, pageRect.width(), satir),
                 Qt::AlignLeft | Qt::AlignVCenter, m_ctx.quote.musteri.unvan);
    y += satir;
    p->setFont(baseFont());

    const QString vergi = m_ctx.quote.musteri.vergiDairesi.trimmed().isEmpty()
                               && m_ctx.quote.musteri.vergiNo.trimmed().isEmpty()
                           ? QString()
                           : QStringLiteral("%1 / %2").arg(m_ctx.quote.musteri.vergiDairesi,
                                                            m_ctx.quote.musteri.vergiNo);
    for (const QString &s : {m_ctx.quote.musteri.yetkili, m_ctx.quote.musteri.telefon,
                              m_ctx.quote.musteri.adres, vergi}) {
        if (s.trimmed().isEmpty())
            continue;
        p->drawText(QRectF(pageRect.left(), y, pageRect.width(), satir),
                     Qt::AlignLeft | Qt::AlignVCenter, s);
        y += satir;
    }
    y += px(p, kBlokBoslukPt);

    if (!m_ctx.quote.projeBasligi.trimmed().isEmpty()) {
        p->setFont(boldFont());
        p->drawText(QRectF(pageRect.left(), y, pageRect.width(), satir),
                     Qt::AlignLeft | Qt::AlignVCenter, m_ctx.quote.projeBasligi);
        p->setFont(baseFont());
        y += satir + px(p, kBlokBoslukPt);
    }
}

void DocumentLayout::paintTableHeader(QPainter *p, const QRectF &pageRect, double &y, bool devam) const
{
    const Columns c = columnsFor(pageRect);
    const double h = m_tabloBasligiYuksekligi;

    if (devam) {
        p->setFont(smallFont());
        p->drawText(QRectF(pageRect.left(), y - m_tabloBasligiYuksekligi, pageRect.width(),
                            m_tabloBasligiYuksekligi),
                     Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("(devam)"));
    }

    p->setFont(boldFont());
    p->fillRect(QRectF(pageRect.left(), y, pageRect.width(), h), QColor(235, 235, 235));

    auto yaz = [&](const QRectF &sut, const QString &metin, Qt::Alignment hiza) {
        p->drawText(QRectF(sut.x() + px(p, kHucreBoslukPt), y, sut.width() - 2 * px(p, kHucreBoslukPt), h),
                     hiza | Qt::AlignVCenter, metin);
    };
    yaz(c.sira, QStringLiteral("#"), Qt::AlignRight);
    yaz(c.aciklama, QStringLiteral("Açıklama"), Qt::AlignLeft);
    yaz(c.birim, QStringLiteral("Birim"), Qt::AlignLeft);
    yaz(c.miktar, QStringLiteral("Miktar"), Qt::AlignRight);
    yaz(c.fiyat, QStringLiteral("B. Fiyat"), Qt::AlignRight);
    yaz(c.tutar, QStringLiteral("Tutar"), Qt::AlignRight);

    p->setFont(baseFont());
    y += h;
    p->drawLine(QPointF(pageRect.left(), y), QPointF(pageRect.right(), y));
}

void DocumentLayout::paintRow(QPainter *p, const QRectF &pageRect, double &y, const QuoteLine &l) const
{
    const Columns c = columnsFor(pageRect);
    const double h = measureRow(p, pageRect, l);
    const QFontMetricsF fm(baseFont(), p->device());

    auto yaz = [&](const QRectF &sut, const QString &metin, Qt::Alignment hiza) {
        p->drawText(QRectF(sut.x() + px(p, kHucreBoslukPt), y + px(p, kHucreBoslukPt), sut.width() - 2 * px(p, kHucreBoslukPt),
                            fm.height()),
                     hiza | Qt::AlignTop, metin);
    };

    yaz(c.sira, QString::number(l.sira), Qt::AlignRight);
    yaz(c.birim, l.birim, Qt::AlignLeft);
    yaz(c.miktar, formatMiktar(l.miktar), Qt::AlignRight);
    yaz(c.fiyat, l.birimFiyat.toString(), Qt::AlignRight);
    yaz(c.tutar, l.tutar.toString(), Qt::AlignRight);

    // Açıklama sarabilir; kendi dikdörtgeninde word-wrap ile çizilir.
    const QRectF aciklamaRect(c.aciklama.x() + px(p, kHucreBoslukPt), y + px(p, kHucreBoslukPt),
                               c.aciklama.width() - 2 * px(p, kHucreBoslukPt), h - 2 * px(p, kHucreBoslukPt));
    p->drawText(aciklamaRect, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, l.aciklama);

    if (!l.satirNotu.trimmed().isEmpty()) {
        const QFontMetricsF sfm(smallFont(), p->device());
        const QRectF aciklamaGerekli = fm.boundingRect(
            QRectF(0, 0, c.aciklama.width() - 2 * px(p, kHucreBoslukPt), 0), Qt::TextWordWrap, l.aciklama);
        p->setFont(smallFont());
        p->setPen(QColor(90, 90, 90));
        p->drawText(QRectF(aciklamaRect.x(), aciklamaRect.y() + aciklamaGerekli.height(),
                            aciklamaRect.width(), sfm.height() * 3),
                     Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, l.satirNotu);
        p->setPen(Qt::black);
        p->setFont(baseFont());
    }

    y += h;
    p->setPen(QColor(210, 210, 210));
    p->drawLine(QPointF(pageRect.left(), y), QPointF(pageRect.right(), y));
    p->setPen(Qt::black);
}

void DocumentLayout::paintTotals(QPainter *p, const QRectF &pageRect, double &y) const
{
    const QFontMetricsF fm(baseFont(), p->device());
    const double satir = fm.height() + 2.0;
    const double etiketX = pageRect.left() + pageRect.width() * 0.55;
    const double etiketW = pageRect.width() * 0.25;
    const double tutarX = etiketX + etiketW;
    const double tutarW = pageRect.width() * 0.20;

    y += px(p, kBlokBoslukPt);

    auto satirYaz = [&](const QString &etiket, const QString &deger, bool kalin) {
        p->setFont(kalin ? boldFont() : baseFont());
        p->drawText(QRectF(etiketX, y, etiketW, satir), Qt::AlignRight | Qt::AlignVCenter, etiket);
        p->drawText(QRectF(tutarX, y, tutarW, satir), Qt::AlignRight | Qt::AlignVCenter, deger);
        y += satir;
    };

    // KDV DÖKÜMÜ YALNIZCA KDV'Lİ KAYITLARDA. Program artık KDV'yi ayrı
    // hesaplamıyor (fiyatlar KDV dahil), bu yüzden yeni tekliflerde tek bir
    // toplam satırı basılır — "Ara Toplam" ile "Genel Toplam"ın aynı sayıyı
    // göstermesi kafa karıştırırdı. Daha önce KDV'li kaydedilmiş teklifler
    // dondurulmuş belgelerdir ve kendi dökümüyle basılmaya devam eder.
    if (!m_ctx.quote.kdvTutari.isZero()) {
        satirYaz(QStringLiteral("Ara Toplam"), m_ctx.quote.araToplam.toString(), false);
        satirYaz(QStringLiteral("KDV (%%1)").arg(m_ctx.quote.kdvOraniYuzde),
                  m_ctx.quote.kdvTutari.toString(), false);
        p->drawLine(QPointF(etiketX, y), QPointF(pageRect.right(), y));
    }
    satirYaz(QStringLiteral("GENEL TOPLAM"), m_ctx.quote.genelToplam.toString(), true);

    p->setFont(baseFont());
    y += px(p, kHucreBoslukPt);
    p->drawText(QRectF(pageRect.left(), y, pageRect.width(), satir), Qt::AlignLeft | Qt::AlignVCenter,
                 QStringLiteral("Yalnız: %1").arg(tutarYaziyla(m_ctx.quote.genelToplam)));
    y += satir;

    if (!m_ctx.quote.sartlarMetni.trimmed().isEmpty()) {
        y += px(p, kBlokBoslukPt);
        p->setFont(smallFont());
        const QFontMetricsF sfm(smallFont(), p->device());
        const QRectF gerekli = sfm.boundingRect(QRectF(0, 0, pageRect.width(), 0), Qt::TextWordWrap,
                                                 m_ctx.quote.sartlarMetni);
        p->drawText(QRectF(pageRect.left(), y, pageRect.width(), gerekli.height()),
                     Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, m_ctx.quote.sartlarMetni);
        y += gerekli.height();
        p->setFont(baseFont());
    }
}

void DocumentLayout::paintFooter(QPainter *p, const QRectF &pageRect, int pageIndex) const
{
    p->setFont(smallFont());
    const QFontMetricsF sfm(smallFont(), p->device());
    p->drawText(QRectF(pageRect.left(), pageRect.bottom() - sfm.height(), pageRect.width(),
                        sfm.height()),
                 Qt::AlignRight | Qt::AlignVCenter,
                 QStringLiteral("%1 / %2").arg(pageIndex + 1).arg(pageCount()));
    p->setFont(baseFont());
}

void DocumentLayout::paintPage(QPainter *p, const QRectF &pageRect, int pageIndex) const
{
    if (pageIndex < 0 || pageIndex >= m_pages.size())
        return;

    const PageRange sayfa = m_pages.at(pageIndex);
    double y = pageRect.top();

    p->save();
    p->setPen(Qt::black);
    p->setFont(baseFont());

    paintHeader(p, pageRect, y);
    paintTableHeader(p, pageRect, y, /*devam=*/pageIndex > 0);

    for (int i = sayfa.ilkSatir; i < sayfa.ilkSatir + sayfa.satirSayisi; ++i) {
        if (i < 0 || i >= m_ctx.quote.satirlar.size())
            break;
        paintRow(p, pageRect, y, m_ctx.quote.satirlar.at(i));
    }

    if (sayfa.toplamlarBurada)
        paintTotals(p, pageRect, y);

    paintFooter(p, pageRect, pageIndex);
    p->restore();
}
