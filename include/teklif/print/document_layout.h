#pragma once

#include "teklif/core/models.h"

#include <QImage>
#include <QStringList>
#include <QRectF>
#include <QVector>

class QPainter;

// Bir teklif belgesinin yazdırılacak/PDF'e verilecek hâlini çizer.
//
// EKRAN VE KÂĞIT AYNI KODU KULLANIR: önizleme, yazıcı ve PDF üçü de bu
// sınıfın paintPage()'ini çağırır. İki ayrı yerleşim kodu iki ayrı hata
// kaynağı demektir — önizlemede düzgün görünüp kâğıtta bozulan bir belge
// bu yüzden mümkün değildir.
//
// LOGO İSTEĞE BAĞLIDIR: logo verilmezse antet için hiç yer AYRILMAZ, firma
// bilgisi sayfanın sol kenarından başlar. "Logo yok" özel bir durum değil,
// varsayılan davranıştır — antet kendiliğinden kısalır, boş bir dikdörtgen
// kalmaz.

// Belgeyi çizmek için gereken her şey. Yerleşim veritabanına HİÇ gitmez —
// çağıran taraf ne basılacağını eksiksiz verir, böylece sınıf arayüzden de
// veritabanından da bağımsız kalır ve testte elle kurulabilir.
struct DocumentContext
{
    Quote quote;
    // Müşteri bilgisi AYRI TUTULMAZ, quote.musteri'den okunur — belgede
    // görünen muhatabın kaydedilen teklifinkinden farklı olması mümkün olmasın
    // diye tek kaynak vardır.
    CompanyInfo company;
    // Boş bırakılabilir; o zaman antet logosuz düzene geçer.
    // Ayarlardan okumak için bkz. print/company_logo.h.
    QImage logo;
    // Belge yazı boyutu (Part 7'de ayarlanabilir olacak, 8-12 pt).
    // Arayüz ölçeğinden BAĞIMSIZDIR: bu yalnızca çıktıyı etkiler.
    int fontPt = 10;
};

class DocumentLayout
{
public:
    explicit DocumentLayout(DocumentContext context);

    // Sayfalara böler. Çizimden ÖNCE çağrılmalıdır; pageCount() ve
    // paintPage() bu hesabın sonucuna dayanır.
    //
    // pageRect: kenar boşlukları düşülmüş, çizilebilir alan (device
    // koordinatlarında). painter yalnızca yazı ölçümü için kullanılır,
    // üzerine çizim YAPILMAZ.
    void paginate(QPainter *painter, const QRectF &pageRect);

    // paginate() sonrası sayfa sayısı. Satır olmasa bile en az 1.
    int pageCount() const { return m_pages.size(); }

    // pageIndex (0'dan başlar) numaralı sayfayı çizer.
    void paintPage(QPainter *painter, const QRectF &pageRect, int pageIndex) const;

    // Belirli bir sayfada gösterilen satır aralığı — testlerin sayfalamayı
    // doğrulayabilmesi için açık.
    struct PageRange
    {
        int ilkSatir = 0;   // quote.satirlar içindeki indeks
        int satirSayisi = 0;
        bool toplamlarBurada = false; // toplam bloğu bu sayfada mı
    };
    PageRange pageRange(int pageIndex) const;

private:
    DocumentContext m_ctx;
    QVector<PageRange> m_pages;

    // Ölçüler paginate() sırasında hesaplanır ve paintPage() aynılarını
    // kullanır; ikisinin ayrışması sayfa taşmasına yol açardı.
    double m_satirYuksekligi = 0.0;
    double m_antetYuksekligi = 0.0;
    double m_tabloBasligiYuksekligi = 0.0;
    double m_toplamlarYuksekligi = 0.0;
    double m_altBilgiYuksekligi = 0.0;
    QVector<double> m_satirYukseklikleri; // her kalemin sardıktan sonraki yüksekliği

    // Sütun sınırları (pageRect genişliğine oranla hesaplanır).
    struct Columns
    {
        QRectF sira, aciklama, birim, miktar, fiyat, tutar;
    };
    Columns columnsFor(const QRectF &pageRect) const;

    // Logonun çizileceği dikdörtgen. Logo yoksa boş (isNull) döner ve
    // antet metni sayfanın sol kenarından başlar.
    QRectF logoRect(QPainter *p, const QRectF &pageRect) const;

    // Firma metninin yazılacağı sütun. Logo varsa onun sağından başlar,
    // sağdaki "TEKLİF / No / Tarih" bloğuna kadar uzanır. Ölçüm ve çizim
    // AYNI genişliği kullanmak zorunda: farklı olsalardı satır sarma
    // hesabı tutmaz ve metin ya kesilir ya taşardı.
    QRectF companyTextRect(QPainter *p, const QRectF &pageRect) const;

    // Firma antetindeki satırlar, boş olanlar elenmiş hâlde.
    // Vergi dairesi ve numarası etiketlenerek tek satırda birleştirilir.
    QStringList companyLines() const;

    double measureHeader(QPainter *p, const QRectF &pageRect) const;
    double measureTotals(QPainter *p, const QRectF &pageRect) const;
    double measureRow(QPainter *p, const QRectF &pageRect, const QuoteLine &l) const;

    void paintHeader(QPainter *p, const QRectF &pageRect, double &y) const;
    void paintTableHeader(QPainter *p, const QRectF &pageRect, double &y, bool devam) const;
    void paintRow(QPainter *p, const QRectF &pageRect, double &y, const QuoteLine &l) const;
    void paintTotals(QPainter *p, const QRectF &pageRect, double &y) const;
    void paintFooter(QPainter *p, const QRectF &pageRect, int pageIndex) const;

    QFont baseFont() const;
    QFont boldFont() const;
    QFont smallFont() const;
};
