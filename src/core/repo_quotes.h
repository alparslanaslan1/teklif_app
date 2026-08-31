#pragma once

#include "models.h"

#include <QSqlDatabase>
#include <QString>
#include <optional>

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

private:
    QSqlDatabase m_db;

    // ÇAĞRILDIĞI YERDE zaten açık bir transaction olmalı.
    QString nextQuoteNumberLocked(QString *errorOut);
    bool insertLines(qint64 quoteId, const QVector<QuoteLine> &lines, QString *errorOut);
};
