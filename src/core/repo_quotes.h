#pragma once

#include "models.h"

#include <QSqlDatabase>
#include <QString>
#include <optional>

class RepoQuotes
{
public:
    // Teklifi (başlık + tüm satırları) tek bir transaction içinde ekler.
    // Teklif no'nun okunup artırılması da AYNI transaction'ın parçasıdır —
    // art arda hızlı iki çağrı asla aynı numarayı almaz. Numara sürekli
    // artar, yıl bazında sıfırlanmaz (settings.teklif_no_sayac). Başarılı
    // olursa quote.id ve quote.teklifNo doldurulur.
    static bool add(QSqlDatabase &db, Quote &quote, QString *errorOut);

    // Var olan bir teklifi (id'sine göre) günceller: başlık alanları
    // güncellenir, satırlar SİLİNİP yeniden eklenir. Bu en basit doğru
    // yoldur — "hangi eski satır hangi yeni satırla eşleşiyor" belirsizliği
    // hiç ortaya çıkmaz (kullanıcı satır sildi/ekledi/sıraladıysa fark etmez).
    static bool update(QSqlDatabase &db, const Quote &quote, QString *errorOut);

    // id'ye ait teklifi başlığı ve tüm satırlarıyla (sira'ya göre sıralı)
    // birlikte döner. Bulunamazsa veya hata olursa std::nullopt.
    static std::optional<Quote> get(QSqlDatabase &db, qint64 id, QString *errorOut);

private:
    // ÇAĞRILDIĞI YERDE zaten açık bir transaction (BEGIN IMMEDIATE) olmalı.
    static QString nextQuoteNumberLocked(QSqlDatabase &db, QString *errorOut);
    static bool insertLines(QSqlDatabase &db, qint64 quoteId, const QVector<QuoteLine> &lines,
                             QString *errorOut);
};
