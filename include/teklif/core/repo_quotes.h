#pragma once

#include "teklif/core/models.h"

#include <QDate>
#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <optional>

// Arşiv listesini daraltan ölçütler. Boş/geçersiz bırakılan her alan
// "bu ölçüte bakma" anlamına gelir, ayrı bir "etkin mi" bayrağı gerekmez.
struct QuoteFilter
{
    QDate tarihBaslangic;    // geçersiz = alt sınır yok
    QDate tarihBitis;        // geçersiz = üst sınır yok. Sınır tarihleri DAHİLDİR.
    QString durum;           // boş = tüm durumlar
    // Teklif no, müşteri unvanı ve proje başlığında birden arar. Müşteriye
    // göre listeleme bununla yapılır — ayrı bir müşteri seçici yoktur, çünkü
    // müşteri artık ayrı bir kayıt değil, teklifin kendi alanıdır.
    QString aranan;
};

// quotes + quote_lines tablolarının deposu.
//
// Bağlantıyı nesne içinde tutar (bkz. RepoItems üzerindeki not).
class RepoQuotes
{
public:
    explicit RepoQuotes(QSqlDatabase db);

    // Teklifi (başlık + tüm satırları) tek bir transaction içinde ekler.
    // Teklif no'nun okunup artırılması da AYNI transaction'ın parçasıdır —
    // art arda hızlı iki çağrı asla aynı numarayı almaz. Numara sürekli
    // artar, yıl bazında sıfırlanmaz (settings'te tutulur). Başarılı olursa
    // quote.id ve quote.teklifNo doldurulur.
    bool add(Quote &quote, QString *errorOut = nullptr);

    // Var olan bir teklifi (id'sine göre) günceller: başlık alanları
    // güncellenir, satırlar SİLİNİP yeniden eklenir. Bu en basit doğru
    // yoldur — "hangi eski satır hangi yeni satırla eşleşiyor" belirsizliği
    // hiç ortaya çıkmaz (kullanıcı satır sildi/ekledi/sıraladıysa fark etmez).
    // id bulunamazsa hata döner.
    bool update(const Quote &quote, QString *errorOut = nullptr);

    // id'ye ait teklifi başlığı ve tüm satırlarıyla (sira'ya göre sıralı)
    // birlikte döner. Bulunamazsa veya hata olursa std::nullopt.
    std::optional<Quote> get(qint64 id, QString *errorOut = nullptr) const;

    // Arşiv listesi: filtreye uyan teklifleri, en yeni tarih önce olacak
    // şekilde döner. Satırlar YÜKLENMEZ (bkz. QuoteSummary).
    QVector<QuoteSummary> list(const QuoteFilter &filtre, QString *errorOut = nullptr) const;

    // Teklifin durumunu değiştirir (Taslak -> Gönderildi -> ...).
    // Bilinmeyen bir durum metni reddedilir (bkz. quote_status.h).
    bool setStatus(qint64 id, const QString &durum, QString *errorOut = nullptr);

    // Var olan teklifi KOPYALAYARAK yeni bir teklif oluşturur.
    // Yeni teklif: yeni numara, BUGÜNÜN tarihi, durum Taslak; satırlar,
    // fiyatlar ve notlar aynen kopyalanır. Orijinal hiç değişmez.
    // En sık kullanılan özellik: geçen yılki teklifi çoğaltıp fiyat güncellemek.
    std::optional<Quote> duplicate(qint64 id, QString *errorOut = nullptr);

    // Teklifi KALICI olarak siler; satırları quote_lines üzerindeki
    // ON DELETE CASCADE ile birlikte gider.
    //
    // NEDEN GERÇEKTEN SİLİYORUZ (katalog ve müşterilerin aksine): yanlış
    // girilmiş bir teklif geçmişin parçası değil, hatadır. Katalog kalemi
    // pasife alınır çünkü ESKİ tekliflerde referansı bulunabilir; bir teklife
    // ise hiçbir şey referans vermez, silinmesi hiçbir kaydı bozmaz.
    //
    // id bulunamazsa hata döner (sessizce "başarılı" saymaz).
    bool remove(qint64 id, QString *errorOut = nullptr);

private:
    QSqlDatabase m_db;

    // ÇAĞRILDIĞI YERDE zaten açık bir transaction olmalı.
    QString nextQuoteNumberLocked(QString *errorOut);

public:
    // Teklif numarasının sıfır dolgulu hane sayısı için kabul edilen aralık.
    // Alt sınır 4: daha azı birkaç yüz teklifte hemen taşardı. Üst sınır 8:
    // ötesi belgede okunaksız uzunlukta bir numara demek.
    static constexpr int kMinQuoteNoDigits = 4;
    static constexpr int kMaxQuoteNoDigits = 8;
    static constexpr int kDefaultQuoteNoDigits = 6;

private:
    bool insertLines(qint64 quoteId, const QVector<QuoteLine> &lines, QString *errorOut);
};
