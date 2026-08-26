#include "numparse.h"

#include <QRegularExpression>

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
