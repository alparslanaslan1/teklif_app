#include "calculator.h"

#include <cmath>

Money Calculator::lineTotal(double miktar, Money birimFiyat, Money iskonto)
{
    const Money brut = birimFiyat * miktar; // Money::operator*(double) kuruşa yuvarlar
    const Money net = brut - iskonto;
    return net.isNegative() ? Money(0) : net;
}

QuoteTotals Calculator::totals(const QVector<CalcLine> &lines, int kdvOraniYuzde)
{
    Money ara;
    for (const CalcLine &l : lines)
        ara += lineTotal(l.miktar, l.birimFiyat, l.iskonto);

    // Kuruş bazlı tam sayı üzerinden yüzde hesabı; tek bir yuvarlama noktası.
    const qint64 kdvKurus =
        static_cast<qint64>(std::llround(static_cast<double>(ara.kurus()) * kdvOraniYuzde / 100.0));
    const Money kdv(kdvKurus);
    const Money genel = ara + kdv;

    return {ara, kdv, genel};
}
