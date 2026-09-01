#include "teklif/core/search.h"

// Arama, iki kovalı bir tarama: aranan metinle BAŞLAYANLAR önce, İÇERENLER
// sonra. Bu dosyadaki iki uygulama (serbest fonksiyon ve indeks sınıfı) aynı
// kuralı paylaşır — sonuçların birbirinden ayrışmaması için tek yardımcıya
// alındı.
namespace {

// Bir kaydın aranan anahtara göre hangi kovaya düştüğünü söyler.
// Döner: 0 = baştan eşleşiyor, 1 = içinde geçiyor, -1 = eşleşmiyor.
int eslesmeKovasi(const QString &adNormal, const QString &kodNormal, const QString &anahtar)
{
    if (adNormal.startsWith(anahtar) || kodNormal.startsWith(anahtar))
        return 0;
    if (adNormal.contains(anahtar) || kodNormal.contains(anahtar))
        return 1;
    return -1;
}

} // namespace

QVector<Item> itemAra(const QVector<Item> &katalog, const QString &aranan)
{
    const QString anahtar = turkishSearchNormalize(aranan.trimmed());
    if (anahtar.isEmpty())
        return {};

    QVector<Item> bastanEslesen;
    QVector<Item> icindeGecen;

    for (const Item &it : katalog) {
        if (!it.aktif)
            continue;

        const int kova = eslesmeKovasi(turkishSearchNormalize(it.ad),
                                        turkishSearchNormalize(it.kod), anahtar);
        if (kova == 0)
            bastanEslesen.append(it);
        else if (kova == 1)
            icindeGecen.append(it);
    }

    bastanEslesen += icindeGecen;
    return bastanEslesen;
}

void ItemSearchIndex::setCatalog(const QVector<Item> &katalog)
{
    m_items = katalog;

    m_entries.clear();
    m_entries.reserve(katalog.size());
    for (const Item &it : katalog) {
        Entry e;
        // Pahalı olan kısım burada, tek seferde yapılır.
        e.adNormal = turkishSearchNormalize(it.ad);
        e.kodNormal = turkishSearchNormalize(it.kod);
        e.aktif = it.aktif;
        m_entries.append(e);
    }
}

QVector<int> ItemSearchIndex::searchIndices(const QString &aranan, int maxResults) const
{
    const QString anahtar = turkishSearchNormalize(aranan.trimmed());
    if (anahtar.isEmpty())
        return {};

    QVector<int> bastanEslesen;
    QVector<int> icindeGecen;

    for (int i = 0; i < m_entries.size(); ++i) {
        const Entry &e = m_entries.at(i);
        if (!e.aktif)
            continue;

        const int kova = eslesmeKovasi(e.adNormal, e.kodNormal, anahtar);
        if (kova == 0)
            bastanEslesen.append(i);
        else if (kova == 1)
            icindeGecen.append(i);

        // Sınır varsa erken çıkış: iki kova birlikte sınıra ulaştıysa daha
        // fazla taramanın anlamı yok. "Baştan eşleşen" grubu her zaman önce
        // geldiği için kesme sonucun kalitesini düşürmez.
        if (maxResults > 0 && bastanEslesen.size() >= maxResults)
            break;
    }

    bastanEslesen += icindeGecen;
    if (maxResults > 0 && bastanEslesen.size() > maxResults)
        bastanEslesen.resize(maxResults);
    return bastanEslesen;
}

QVector<Item> ItemSearchIndex::search(const QString &aranan, int maxResults) const
{
    const QVector<int> indeksler = searchIndices(aranan, maxResults);

    QVector<Item> sonuc;
    sonuc.reserve(indeksler.size());
    for (const int i : indeksler)
        sonuc.append(m_items.at(i));
    return sonuc;
}
