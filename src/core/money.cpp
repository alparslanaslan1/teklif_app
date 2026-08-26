#include "money.h"

#include <QRegularExpression>
#include <cmath>

namespace {

// Kabul edilen biçim (isteğe bağlı eksi işareti hariç açıklama):
//   tam kısım  := \d+                      ("1234" gibi, gruplama yok)
//              |  \d{1,3}(\.\d{3})+        ("1.234", "12.345" gibi, doğru gruplu)
//   ondalık    := ("," + 1 ya da 2 hane)?   isteğe bağlı
//
// "12.34.56" gibi hatalı gruplamalar hiçbir alternatifle eşleşmediği için
// reddedilir; bu kasıtlı, "sessizce yanlış yorumlamaktansa reddet" ilkesi.
const QRegularExpression &moneyPattern()
{
    static const QRegularExpression re(
        QStringLiteral(R"(^-?(\d+|\d{1,3}(?:\.\d{3})+)(,\d{1,2})?$)"));
    return re;
}

QString groupThousands(QString digits)
{
    for (int i = digits.length() - 3; i > 0; i -= 3)
        digits.insert(i, QChar(u'.'));
    return digits;
}

} // namespace

std::optional<Money> Money::fromString(const QString &metinHam)
{
    const QString metin = metinHam.trimmed();
    if (metin.isEmpty())
        return std::nullopt;

    const QRegularExpressionMatch m = moneyPattern().match(metin);
    if (!m.hasMatch())
        return std::nullopt;

    const bool negatif = metin.startsWith(QLatin1Char('-'));
    QString tamKisim = m.captured(1);
    tamKisim.remove(QLatin1Char('.'));

    QString ondalikKisim = m.captured(2); // örn. ",5" veya ",56" veya boş
    ondalikKisim.remove(QLatin1Char(','));
    if (ondalikKisim.isEmpty())
        ondalikKisim = QStringLiteral("00");
    else if (ondalikKisim.length() == 1)
        ondalikKisim += QLatin1Char('0');

    bool tamOk = false, ondalikOk = false;
    const qint64 tl = tamKisim.toLongLong(&tamOk);
    const qint64 kr = ondalikKisim.toLongLong(&ondalikOk);
    if (!tamOk || !ondalikOk)
        return std::nullopt;

    qint64 kurus = tl * 100 + kr;
    if (negatif)
        kurus = -kurus;

    return Money(kurus);
}

Money Money::fromDouble(double tl) noexcept
{
    return Money(static_cast<qint64>(std::llround(tl * 100.0)));
}

QString Money::toString() const
{
    const qint64 mutlak = m_kurus < 0 ? -m_kurus : m_kurus;
    const qint64 tl = mutlak / 100;
    const qint64 kr = mutlak % 100;

    QString sonuc = groupThousands(QString::number(tl)) + QLatin1Char(',') +
                     QString::number(kr).rightJustified(2, QLatin1Char('0'));
    if (m_kurus < 0)
        sonuc.prepend(QLatin1Char('-'));
    return sonuc;
}

Money Money::operator*(double kat) const noexcept
{
    return Money(static_cast<qint64>(std::llround(static_cast<double>(m_kurus) * kat)));
}

Money Money::operator/(qint64 payda) const noexcept
{
    if (payda == 0)
        return Money(0);
    return Money(static_cast<qint64>(std::llround(static_cast<double>(m_kurus) / payda)));
}
