#pragma once

#include "teklif/core/models.h"
#include "teklif/core/turkish.h"

#include <QString>
#include <QVector>

// Eski ad, geriye dönük uyumluluk için: normalizasyon artık turkish.h'de
// yaşıyor çünkü sıralama da aynı Türkçe harf kurallarına ihtiyaç duyuyor ve
// iki yerde iki kopya olması kaçınılmaz olarak birbirinden ayrışırdı.
inline QString turkceAramaNormalize(const QString &metin)
{
    return turkishSearchNormalize(metin);
}

// items içinde arama yapar. Önce ADI VEYA KODU aranan metinle BAŞLAYAN
// kalemler, sonra aranan metni İÇEREN (ama başlamayan) kalemler gelir;
// her iki grup da kendi içinde orijinal sırayı korur (stabil sıralama).
// Pasif kalemler hiçbir zaman sonuçta çıkmaz. Aranan metin boşsa (veya
// sadece boşluksa) boş liste döner — arama kutusu boşken sonuç listesinin
// kendiliğinden tüm katalogla dolmaması için.
//
// Tek seferlik aramalar için yeterlidir; her tuş vuruşunda arama yapan bir
// arayüz ItemSearchIndex kullanmalıdır (aşağıya bakınız).
QVector<Item> itemAra(const QVector<Item> &katalog, const QString &aranan);

// Yazdıkça arama yapan arayüzler için önceden hazırlanmış katalog indeksi.
//
// NEDEN VAR: itemAra() her çağrıda HER kalemin adını ve kodunu yeniden
// normalize eder; normalizasyon 7 ayrı replace + toLower demektir ve her
// biri yeni bir QString ayırır. 5.000 kalemlik bir katalogda bu, tuş başına
// ~10.000 tahsis eder. Burada normalize edilmiş metinler setCatalog()
// sırasında BİR KEZ hesaplanır; arama yalnızca startsWith/contains yapar.
//
// Sonuçlar Item kopyası olarak değil, katalogdaki indeksler üzerinden
// döndürülebilir (bkz. searchIndices) — büyük kataloglarda kopyalama
// maliyetini de ortadan kaldırır.
class ItemSearchIndex
{
public:
    // Katalogu alır ve normalize edilmiş arama alanlarını hazırlar.
    // Katalog değiştiğinde (kalem eklendi/düzenlendi) yeniden çağrılmalıdır.
    void setCatalog(const QVector<Item> &katalog);

    // Hazırlanmış katalogdaki kalem sayısı (pasifler dâhil).
    int catalogSize() const { return m_items.size(); }

    // itemAra() ile AYNI sıralama kurallarını uygular.
    // maxResults > 0 ise sonuç o sayıda kesilir — "a" gibi çok genel bir
    // aramada binlerce satırlık liste oluşturmamak için.
    QVector<Item> search(const QString &aranan, int maxResults = 0) const;

    // search() ile aynı, ama Item kopyalamak yerine katalog indekslerini
    // döner. Sonuçları doğrudan bir modelde göstermek için tercih edilir.
    QVector<int> searchIndices(const QString &aranan, int maxResults = 0) const;

    // Indeks numarasına karşılık gelen kalem. searchIndices() sonuçlarıyla
    // birlikte kullanılır.
    const Item &itemAt(int index) const { return m_items.at(index); }

private:
    // Kalemin normalize edilmiş arama alanları. Katalogla aynı sırada tutulur,
    // böylece i. giriş i. kaleme karşılık gelir.
    struct Entry
    {
        QString adNormal;
        QString kodNormal;
        bool aktif = true;
    };

    QVector<Item> m_items;
    QVector<Entry> m_entries;
};
