#include "numparse.h"

#include <QRegularExpression>


// ═══ parseTurkishNumber() ═════════════════════════════════════════════════
// NE YAPAR : Türkçe biçimli MİKTAR metnini double'a çevirir ("0,333" -> 0.333).
//            Money::fromString ile aynı dilbilgisi, tek farkı 1-4 ondalık
//            haneye izin vermesi ve kuruş yerine düz double döndürmesi.
//
// ADIM ADIM (hata ayıklarken bu sırayı takip edin):
//   1) trimmed(); boşsa nullopt.                              [ÇIKIŞ 1]
//   2) Regex eşleşmesi. Uymazsa nullopt.                      [ÇIKIŞ 2]
//      Buraya düşmenin en sık sebebi kullanıcının NOKTA ile ondalık
//      yazmasıdır ("0.5"). Bu KASITLI olarak reddedilir; nokta yalnızca
//      binlik ayraçtır.
//   3) tamKisim = captured(1), içindeki '.' karakterleri silinir.
//   4) ondalikKisim = captured(2), ',' silinir.
//   5) İkisi ARADA NOKTA ile birleştirilir -> "1234.333". Bu artık
//      C/İngiliz biçimidir; QString::toDouble bunu bekler.
//      (ondalık boşsa "0" konur -> "1234.0")
//   6) toDouble; başarısızsa nullopt.                         [ÇIKIŞ 3]
//   7) Metin '-' ile başlıyorsa sonuç negatiflenir (regex grubu eksiyi almaz).
//
// DEBUG    : 5. adımdan sonra `birlesik` değerini bastırmak neredeyse her
//            sorunu gösterir:  qDebug() << metin << "->" << birlesik;
//            "1.234,5" için beklenen: "1234.5"
//            Eğer "1.234.5" görüyorsanız 3. adımdaki nokta temizliği çalışmamıştır.
//
// NOT      : Bu fonksiyon LOCALE'DEN BAĞIMSIZDIR. QLocale::toDouble kullanmak
//            işletim sistemi diline göre farklı davranırdı; burada davranış
//            her makinede aynıdır.
std::optional<double> parseTurkishNumber(const QString &metinHam)
{
    const QString metin = metinHam.trimmed();
    if (metin.isEmpty())
        return std::nullopt;

    // Money::fromString ile aynı tam kısım kuralı (money.cpp'deki
    // moneyPattern() ile bilerek birebir aynı mantık — ayrı bir dosyada
    // tutuluyor çünkü miktarın ondalık hane sınırı 2 değil 1-4).
    static const QRegularExpression re(
        QStringLiteral(R"(^-?(\d+|\d{1,3}(?:\.\d{3})+)(,\d{1,4})?$)"));

    const QRegularExpressionMatch m = re.match(metin);
    if (!m.hasMatch())
        return std::nullopt;

    QString tamKisim = m.captured(1);
    tamKisim.remove(QLatin1Char('.'));

    QString ondalikKisim = m.captured(2);
    ondalikKisim.remove(QLatin1Char(','));

    const QString birlesik =
        tamKisim + QLatin1Char('.') + (ondalikKisim.isEmpty() ? QStringLiteral("0") : ondalikKisim);

    bool ok = false;
    double deger = birlesik.toDouble(&ok);
    if (!ok)
        return std::nullopt;

    if (metin.startsWith(QLatin1Char('-')))
        deger = -deger;

    return deger;
}
