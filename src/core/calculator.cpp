#include "teklif/core/calculator.h"

#include <cmath>


// Tek bir teklif satırının tutarını hesaplar: (miktar × birimFiyat) − iskonto.
//   brut : birimFiyat * miktar; Money::operator*(double) burada kuruşa yuvarlar
//   net  : iskonto düşülmüş hali. Negatif çıkarsa 0'a sabitlenir.
// Yuvarlama satır bazında yapılır (önce her satır, sonra toplama) — böylece
// toplamda kuruş sapması birikmez.
Money Calculator::lineTotal(double miktar, Money birimFiyat, Money iskonto)
{
    const Money brut = birimFiyat * miktar; // Money::operator*(double) kuruşa yuvarlar
    const Money net = brut - iskonto;
    return net.isNegative() ? Money(0) : net;
}


// Tüm kalemlerden ara toplam, KDV tutarı ve genel toplamı üretir.
//   ara           : satır tutarlarının toplamı; zaten kuruşa yuvarlanmış
//                   değerler toplandığı için ek bir yuvarlama gerekmez
//   kdvOraniYuzde : tam sayı yüzde (20 -> %20); quotes.kdv_orani ile aynı birim
//   kdvKurus      : ara × oran / 100. Tek yuvarlama noktası burasıdır.
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
