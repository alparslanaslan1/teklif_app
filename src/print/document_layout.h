#pragma once

#include "core/models.h"

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
// LOGO YOK: hiçbir yerde logo saklanmıyor, bu yüzden yerleşim logo alanı
// AYIRMADAN tasarlandı. "Logo yok" özel bir durum değil, varsayılan
// davranıştır; sonradan eklenirse antet yüksekliği yeniden hesaplanır.

// Belgeyi çizmek için gereken her şey. Yerleşim veritabanına HİÇ gitmez —
// çağıran taraf ne basılacağını eksiksiz verir, böylece sınıf arayüzden de
// veritabanından da bağımsız kalır ve testte elle kurulabilir.
struct DocumentContext
{
    Quote quote;
    Customer customer;
    CompanyInfo company;
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
