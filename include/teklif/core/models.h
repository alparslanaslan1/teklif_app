#pragma once

#include "teklif/core/money.h"

#include <QDate>
#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVector>

// items tablosunun bellek içi karşılığı. Alan adları veritabanı şemasıyla
// birebir aynı — repo_items bunu SELECT/INSERT/UPDATE için kullanır.
struct Item
{
    qint64 id = 0;
    QString kod;
    QString ad;
    QString birim;
    Money varsayilanFiyat;
    // categories(id)'ye referans. 0 = kategori yok (DB'de NULL). Geçerli
    // SQLite rowid'leri her zaman 1'den başladığı için 0 güvenle "boş"
    // anlamına gelir, ayrı bir "var mı" bayrağı gerekmez.
    qint64 categoryId = 0;
    bool aktif = true;
    QDateTime guncelleme;
};
Q_DECLARE_METATYPE(Item) // ItemSearch::itemChosen sinyali bunu taşır; QSignalSpy için gerekli.

// categories tablosunun bellek içi karşılığı. Kategoriler yalnızca ad'dan
// ibarettir; ayrı bir yönetim ekranı yoktur, CSV içe aktarımında ve katalog
// ekranındaki açılır listede adı geçen kategori kendiliğinden oluşur.
struct Category
{
    qint64 id = 0;
    QString ad;
};

// Teklifin muhatabı. AYRI BİR TABLO DEĞİL: bu bilgiler teklif kaydedilirken
// quotes satırının içine yazılır.
//
// NEDEN AYRI MÜŞTERİ KAYDI TUTMUYORUZ: teklif, kaydedildiği anda donan bir
// belgedir. Müşteri ayrı bir tabloda dursaydı, unvanı ya da adresi sonradan
// düzeltmek geçmişteki BÜTÜN tekliflerin antetini geriye dönük değiştirirdi —
// bir yıl önce verilmiş belgenin çıktısı bugün farklı basılırdı. Satırların
// katalogdan kopyalanmasıyla (bkz. QuoteLine) aynı gerekçe.
//
// Müşteriye göre arama, arşiv ekranındaki metin aramasıyla yapılır; unvan
// tekliflerin kendi içinde yazılı olduğu için ayrı bir müşteri listesi
// tutulmaz.
struct Customer
{
    QString unvan;
    QString yetkili;
    QString telefon;
    QString email;
    QString adres;
    QString vergiDairesi;
    QString vergiNo;

    // Antet için en az bunun dolu olması gerekir; teklif unvansız kaydedilemez.
    bool isEmpty() const { return unvan.trimmed().isEmpty(); }
};

// quote_lines tablosunun bellek içi karşılığı.
//
// KRİTİK: aciklama, birim ve birimFiyat katalogdaki Item'dan KOPYALANIR,
// item.id'ye referans TUTULMAZ. Bir teklif kaydedildikten sonra dondurulmuş
// bir belgedir — katalogda fiyat değişse veya kalem pasife alınsa bile bu
// satır etkilenmez (bkz. proje planı, Bölüm 4).
struct QuoteLine
{
    qint64 id = 0;
    int sira = 0;
    QString aciklama;
    QString birim;
    double miktar = 0.0;
    Money birimFiyat;
    QString satirNotu;
    // Kaydedilirken Calculator::lineTotal ile hesaplanır ve DB'ye de
    // yazılır — her okumada yeniden hesaplamak yerine son hesaplanan
    // değeri saklamak, tutarlılığı garanti eder.
    Money tutar;
};

// quotes tablosunun bellek içi karşılığı, satırlarıyla birlikte.
struct Quote
{
    qint64 id = 0;
    // Sürekli artan, sıfır dolgulu 6 haneli numara (örn. "000143").
    // RepoQuotes::add() tarafından atanır; elle set edilmez.
    QString teklifNo;
    // Teklif kaydedilirken içine YAZILAN müşteri bilgisi (bkz. Customer).
    Customer musteri;
    QDate tarih;
    int gecerlilikGun = 15;
    QString projeBasligi;
    QString projeNotu;
    QString durum = QStringLiteral("Taslak");
    QString sartlarMetni;
    Money araToplam;
    int kdvOraniYuzde = 0;
    Money kdvTutari;
    Money genelToplam;
    QVector<QuoteLine> satirlar;
};

// Arşiv/teklif listesi için hafif özet.
//
// NEDEN AYRI BİR YAPI: liste ekranı yüzlerce teklif gösterir ama hiçbirinin
// satırlarına ihtiyaç duymaz. Quote'u kullanmak her satır için quote_lines
// sorgusu açmak (N+1) ya da hiç kullanılmayacak veriyi belleğe almak
// demekti. Müşteri unvanı teklifin kendi sütununda durduğu için ayrıca
// JOIN de gerekmez.
struct QuoteSummary
{
    qint64 id = 0;
    QString teklifNo;
    QString musteriUnvan;
    QDate tarih;
    QString durum;
    Money genelToplam;
};

// Belge antetinde (yazdırma/PDF) kullanılan firma bilgisi. Part 7'de
// Ayarlar ekranından/veritabanından (settings tablosu) doldurulacak;
// şimdilik çağıran taraf elle verir. Logo BİLEREK yok — henüz hiçbir
// yerde saklanmıyor; document_layout logo alanını hiç ayırmadan tasarlanır
// (bkz. print/document_layout.h), böylece "logo yok" özel bir durum değil,
// varsayilan davranıştır.
struct CompanyInfo
{
    QString unvan;
    // Bayilik / yetki belgesi satırı (örn. "Aksa Doğalgaz Yetkili Firma
    // (No: 328)"). Antette unvanın hemen altında görünür.
    //
    // NEDEN AYRI ALAN: adrese ya da unvana sıkıştırılabilirdi, ama ikisi de
    // yanlış olurdu — adres bir konum, unvan ise ticari isimdir. Yetki
    // bilgisi ayrı bir kimlik satırıdır ve boş bırakılabilir olmalıdır;
    // her firmanın bayiliği yoktur.
    QString yetkiBelgesi;
    QString adres;
    QString telefon;
    QString email;
    QString vergiDairesi;
    QString vergiNo;
};


