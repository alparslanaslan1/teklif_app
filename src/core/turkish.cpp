#include "teklif/core/turkish.h"

#include <QHash>

namespace {

// Türk alfabesi, sıralama sırasıyla. q/w/x alfabede yoktur ama yabancı
// kaynaklı kayıtlarda geçebilir; sözlüklerdeki yaygın uygulamaya uyarak
// fonetik komşularının yanına yerleştirildiler.
const QString &alfabe()
{
    static const QString a = QStringLiteral("abcçdefgğhıijklmnoöpqrsştuüvwxyz");
    return a;
}

// Harf -> alfabedeki sıra. Her çağrıda indexOf ile aramak yerine tek sefer
// kurulan bir tablo; sıralama binlerce karşılaştırma yapar.
const QHash<QChar, int> &harfSirasi()
{
    static const QHash<QChar, int> tablo = [] {
        QHash<QChar, int> h;
        const QString &a = alfabe();
        for (int i = 0; i < a.size(); ++i)
            h.insert(a.at(i), i);
        return h;
    }();
    return tablo;
}

// Türkçeye özgü küçük harfe çevirme. Qt'nin varsayılan toLower()'ı
// 'I' -> 'i' yapar (Türkçede 'ı' olmalı) ve 'İ' -> 'i' + U+0307 (iki kod
// noktası) üretir. İkisi de bu uygulamada yanlış sonuç verir.
QChar turkishLower(QChar c)
{
    if (c == QChar(u'I'))
        return QChar(u'ı');
    if (c == QChar(0x0130)) // İ
        return QChar(u'i');
    return c.toLower();
}

// Anahtar bölgeleri: aynı türden karakterler bir arada kalsın, türler
// arasında sabit bir sıra olsun diye kod noktaları bloklara ayrılır.
constexpr ushort kBolgeNoktalama = 0x0100;
constexpr ushort kBolgeRakam     = 0x0200;
constexpr ushort kBolgeHarf      = 0x0300;
constexpr ushort kBolgeDiger     = 0x1000;

} // namespace

QString turkishSearchNormalize(const QString &metin)
{
    QString s = metin;

    // 'İ' (U+0130) toLower()'dan ÖNCE elle katlanır; sebebi turkishLower()
    // üzerindeki nota bakınız.
    s.replace(QChar(0x0130), QChar(u'i'));
    s = s.toLower();

    s.replace(QChar(0x0131), QChar(u'i')); // ı
    s.replace(QChar(0x015F), QChar(u's')); // ş
    s.replace(QChar(0x011F), QChar(u'g')); // ğ
    s.replace(QChar(0x00FC), QChar(u'u')); // ü
    s.replace(QChar(0x00F6), QChar(u'o')); // ö
    s.replace(QChar(0x00E7), QChar(u'c')); // ç

    return s;
}

QString turkishSortKey(const QString &metin)
{
    QString anahtar;
    anahtar.reserve(metin.size());

    for (const QChar ham : metin) {
        const QChar c = turkishLower(ham);

        const auto it = harfSirasi().constFind(c);
        if (it != harfSirasi().constEnd()) {
            anahtar.append(QChar(static_cast<ushort>(kBolgeHarf + it.value())));
        } else if (c.isDigit()) {
            anahtar.append(QChar(static_cast<ushort>(kBolgeRakam + c.digitValue())));
        } else if (c.isSpace() || c.isPunct()) {
            anahtar.append(QChar(kBolgeNoktalama));
        } else {
            // Tanınmayan karakter (ör. Kiril, emoji): en sona düşer ama
            // sıralama yine de belirlenimlidir — aynı girdi hep aynı sonucu verir.
            anahtar.append(QChar(static_cast<ushort>(kBolgeDiger + (c.unicode() & 0x0FFF))));
        }
    }
    return anahtar;
}

int turkishCompare(const QString &a, const QString &b)
{
    const int birincil = QString::compare(turkishSortKey(a), turkishSortKey(b));
    if (birincil != 0)
        return birincil;
    // Anahtarlar eşit: yalnızca büyük/küçük harfte ayrılıyorlar. Sıralamanın
    // kararlı ve yinelenebilir olması için özgün metne bakılır.
    return QString::compare(a, b);
}
