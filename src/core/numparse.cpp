#include "teklif/core/numparse.h"

#include <QRegularExpression>


// Türkçe biçimli miktar metnini double'a çevirir ("0,333" -> 0.333).
// Money::fromString ile aynı dilbilgisi kuralları; farkı 1-4 ondalık haneye
// izin vermesi ve kuruş yerine düz double döndürmesi (miktarlar kesirli
// olabilir: 0,333 m²).
//   metin        : boşlukları kırpılmış girdi
//   re           : nokta binlik, virgül ondalık kuralını uygulayan desen
//   tamKisim     : binlik noktaları silinmiş tam kısım
//   ondalikKisim : virgülü silinmiş ondalık kısım
//   birlesik     : "1234.333" — araya NOKTA konur, çünkü QString::toDouble
//                  C biçimini (nokta ondalık) bekler
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
