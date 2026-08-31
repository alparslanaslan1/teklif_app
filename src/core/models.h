#pragma once

#include "money.h"

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

// customers tablosunun bellek içi karşılığı.
struct Customer
{
    qint64 id = 0;
    QString unvan;
    QString yetkili;
    QString telefon;
    QString email;
    QString adres;
    QString vergiDairesi;
    QString vergiNo;
    QString notlar;
    bool aktif = true;
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
    qint64 customerId = 0;
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
// demekti. customerUnvan tek bir JOIN ile gelir; müşteri adı için satır
// başına ayrı sorgu atılmaz.
struct QuoteSummary
{
    qint64 id = 0;
    QString teklifNo;
    qint64 customerId = 0;
    QString customerUnvan;
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
    QString adres;
    QString telefon;
    QString email;
    QString vergiDairesi;
    QString vergiNo;
};


