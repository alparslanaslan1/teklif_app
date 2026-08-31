#pragma once

#include <QString>

// Türkçe metin işlemlerinin TEK yeri: arama normalizasyonu ve alfabetik
// sıralama. İkisi de uygulama genelinde aynı davranmak zorunda olduğu için
// ayrı ayrı yerlerde tekrar edilmez.

// Arama eşleştirmesi için metni sadeleştirir: tüm Türkçe harfler ASCII
// karşılığına iner, her şey küçük harfe çevrilir ("İşçilik" -> "iscilik").
// Böylece Türkçe klavyesi olmayan kullanıcı "is" yazınca da kalemi bulur.
// Bu çıktı YALNIZCA eşleştirme içindir, ekranda gösterilmez.
QString turkishSearchNormalize(const QString &metin);

// Metni Türk alfabesi sırasına göre karşılaştırılabilir bir anahtara çevirir.
//
// Neden QCollator DEĞİL: QCollator Türkçe sıralama için ICU'ya ihtiyaç duyar;
// ICU'suz derlenmiş bir Qt'de ya da farklı ICU sürümlerinde sonuç DEĞİŞİR.
// Bu uygulamanın tüm listeleri Türkçe olduğu için sıralamanın her makinede
// birebir aynı olması gerekir — anahtar elle üretilir, dış bağımlılık yoktur.
//
// Anahtar sırası: boşluk/noktalama < rakamlar < Türk alfabesi < diğer.
QString turkishSortKey(const QString &metin);

// İki metni Türk alfabesi sırasına göre karşılaştırır.
// Döner: a < b ise negatif, eşitse 0, a > b ise pozitif.
//
// Büyük/küçük harf birincil ölçüt DEĞİLDİR ("ahşap" ile "Ahşap" aynı yere
// düşer); anahtarlar eşit çıkarsa sıralamanın kararlı olması için özgün
// metinler karşılaştırılır.
int turkishCompare(const QString &a, const QString &b);

// SQL'deki ORDER BY yerine bunu kullan: SQLite'ın varsayılan BINARY
// collation'ı Türkçede Ç, İ, ı, Ş, Ğ, Ü, Ö ile başlayan kayıtları Z'DEN
// SONRAYA atar. Kullanım:
//   std::sort(v.begin(), v.end(), TurkishLess<Item>([](const Item &i){ return i.ad; }));
template <typename T, typename KeyFn>
struct TurkishLessBy
{
    KeyFn key;
    explicit TurkishLessBy(KeyFn k) : key(k) {}
    bool operator()(const T &a, const T &b) const { return turkishCompare(key(a), key(b)) < 0; }
};

template <typename T, typename KeyFn>
TurkishLessBy<T, KeyFn> turkishLessBy(KeyFn k)
{
    return TurkishLessBy<T, KeyFn>(k);
}
