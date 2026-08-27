#include "search.h"


// ═══ turkceAramaNormalize() ═══════════════════════════════════════════════
// NE YAPAR : Bir metni ARAMA KARŞILAŞTIRMASI için sadeleştirir. Tüm Türkçe
//            harfler ASCII karşılığına indirgenir, her şey küçük harfe iner.
//            "İşçilik" -> "iscilik"   böylece kullanıcı "is" yazınca da bulur.
//
// ADIM ADIM (SIRA ÖNEMLİ — değiştirmeyin):
//   1) 'İ' (U+0130) ELLE 'i' yapılır. Bu adım toLower()'DAN ÖNCE olmak
//      ZORUNDA: Qt'nin varsayılan toLower()'ı 'İ' harfini TEK bir 'i'ye değil,
//      'i' + U+0307 (birleşen nokta) İKİLİSİNE çevirir. O zaman metin bir
//      karakter uzar ve startsWith/contains karşılaştırmaları tutmaz.
//   2) toLower() -> ASCII 'I'->'i' ve Ş/Ğ/Ü/Ö/Ç sorunsuz küçülür.
//   3) Kalan Türkçe küçük harfler tek tek ASCII'ye katlanır:
//        ı(U+0131)->i   ş(U+015F)->s   ğ(U+011F)->g
//        ü(U+00FC)->u   ö(U+00F6)->o   ç(U+00E7)->c
//
// DEBUG    : Bir kalem aramada çıkmıyorsa İKİ TARAFI da bastırıp karşılaştırın:
//              qDebug() << turkceAramaNormalize(it.ad)
//                       << turkceAramaNormalize(aranan);
//            Uzunluklar farklıysa 1. adımdaki 'İ' sorunu geri gelmiş demektir:
//              qDebug() << s.size() << s.toUtf8().toHex();
//
// UYARI    : Bu çıktıyı ASLA ekranda göstermeyin — sadece eşleştirme içindir.
// EKSİK    : Â/Î/Û (şapkalı) harfler ve Unicode NFD (ayrık) yazımlar
//            katlanmıyor; öyle bir kayıt aramada bulunmaz.
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


// ═══ itemAra() ════════════════════════════════════════════════════════════
// NE YAPAR : Bellekteki katalogda arar ve sonuçları ALAKAYA GÖRE sıralar:
//            önce "ile başlayanlar", sonra "içinde geçenler".
//
// ADIM ADIM:
//   1) anahtar = normalize(aranan.trimmed()). Boşsa BOŞ LİSTE döner —
//      arama kutusu boşken listenin tüm katalogla dolmaması için.  [ÇIKIŞ 1]
//   2) İki ayrı kova hazırlanır: bastanEslesen ve icindeGecen.
//   3) Katalog BAŞTAN SONA taranır (indeks yok, doğrusal tarama):
//      a) it.aktif false ise satır ATLANIR -> pasif kalem asla çıkmaz.
//      b) it.ad ve it.kod normalize edilir.
//      c) Biri anahtar ile BAŞLIYORSA -> bastanEslesen kovasına.
//      d) Değilse ama İÇERİYORSA      -> icindeGecen kovasına.
//         (else-if olduğu için bir kalem iki kovada birden olamaz.)
//   4) İki kova birleştirilir: bastanEslesen += icindeGecen.
//      Her kova kendi içinde katalog sırasını korur (stabil sıralama).
//
// DEBUG    : Sonuç boş geliyorsa sırayla şunlara bakın:
//              qDebug() << "katalog:" << katalog.size()      // 0 ise reloadCatalog() çalışmamış
//                       << "anahtar:" << anahtar              // boşsa 1. adımda çıkılmış
//                       << "aktif:"   << it.aktif;            // false ise 3a'da atlanmış
//            Sıralama şaşırıyorsa 3c/3d'de hangi kovaya düştüğünü bastırın.
//
// PERFORMANS: Her tuş vuruşunda HER kalem için normalize() 2 kez çağrılır
//            (ad + kod) ve her çağrı yeni QString ayırır. 5.000 kalem =
//            tuş başına ~10.000 tahsis. Yavaşlık ölçüyorsanız kaynak burasıdır;
//            çözüm normalize sonuçlarını setCatalog() içinde bir kez önbelleğe
//            almaktır.
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
