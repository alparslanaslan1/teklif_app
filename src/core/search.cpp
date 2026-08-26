#include "search.h"

QString turkceAramaNormalize(const QString &metin)
{
    QString s = metin;

    // 'İ' (U+0130) toLower() ÖNCESİNDE elle katlanır: Qt'nin varsayılan
    // toLower()'ı bunu 'i' + U+0307 (birleşen nokta) ikilisine çevirir,
    // tek karakterlik 'i' değil. Ampirik olarak doğrulandı (Qt 6.4.2).
    s.replace(QChar(0x0130), QChar(u'i'));

    s = s.toLower(); // ASCII 'I'->'i' zaten burada olur; Ş/Ğ/Ü/Ö/Ç sorunsuz küçülür.

    // Kalan Türkçe harfleri ASCII karşılıklarına katla.
    s.replace(QChar(0x0131), QChar(u'i')); // ı
    s.replace(QChar(0x015F), QChar(u's')); // ş
    s.replace(QChar(0x011F), QChar(u'g')); // ğ
    s.replace(QChar(0x00FC), QChar(u'u')); // ü
    s.replace(QChar(0x00F6), QChar(u'o')); // ö
    s.replace(QChar(0x00E7), QChar(u'c')); // ç

    return s;
}

QVector<Item> itemAra(const QVector<Item> &katalog, const QString &aranan)
{
    const QString anahtar = turkceAramaNormalize(aranan.trimmed());
    if (anahtar.isEmpty())
        return {};

    QVector<Item> bastanEslesen;
    QVector<Item> icindeGecen;

    for (const Item &it : katalog) {
        if (!it.aktif)
            continue;

        const QString ad = turkceAramaNormalize(it.ad);
        const QString kod = turkceAramaNormalize(it.kod);

        if (ad.startsWith(anahtar) || kod.startsWith(anahtar))
            bastanEslesen.append(it);
        else if (ad.contains(anahtar) || kod.contains(anahtar))
            icindeGecen.append(it);
    }

    bastanEslesen += icindeGecen;
    return bastanEslesen;
}
