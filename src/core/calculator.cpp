#include "calculator.h"

#include <cmath>


// ═══ Calculator::lineTotal() ══════════════════════════════════════════════
// NE YAPAR : Tek bir teklif satırının tutarını hesaplar:
//            (miktar × birimFiyat) − iskonto, en yakın kuruşa yuvarlanmış.
//
// ADIM ADIM:
//   1) brut = birimFiyat * miktar
//      -> Money::operator*(double) çağrılır ve BURADA yuvarlama olur.
//         Sonuç artık tam kuruştur; kesir kalmaz.
//   2) net = brut - iskonto   (iskonto şu an her yerden Money(0) gelir)
//   3) net negatifse 0'a sabitlenir -> teklifte eksi satır tutarı olamaz.
//
// NEDEN BU SIRA: Önce SATIR yuvarlanır, sonra satırlar toplanır. Tersini
//   yapsaydık (double toplayıp sonda bir kez yuvarlamak) toplam 1-2 kuruş
//   şaşardı. Bu davranışın testi: test_calculator.cpp / arithmeticNoPrecisionLoss
//
// DEBUG    : Satır tutarı beklenenden 1 kuruş farklıysa 1. adımdaki ham
//            çarpımı bastırın:
//              qDebug() << QString::number(birimFiyat.kurus() * miktar, 'f', 6);
//            Ör. 3 × 3333 kuruş = 9999.000000 -> 9999 (doğru).
// TUZAK    : `iskonto` parametresi veri modelinde var ama HİÇBİR ekran onu
//            doldurmuyor; her zaman Money(0) gelir. 2. ve 3. adımlar pratikte
//            etkisizdir — burada breakpoint'iniz tetiklenmiyorsa sebebi budur.
Money Calculator::lineTotal(double miktar, Money birimFiyat, Money iskonto)
{
    const Money brut = birimFiyat * miktar; // Money::operator*(double) kuruşa yuvarlar
    const Money net = brut - iskonto;
    return net.isNegative() ? Money(0) : net;
}


// ═══ Calculator::totals() ═════════════════════════════════════════════════
// NE YAPAR : Tüm satırlardan ara toplam, KDV tutarı ve genel toplamı üretir.
//
// ADIM ADIM:
//   1) `ara` Money(0) olarak başlar.
//   2) Her satır için lineTotal() çağrılır ve `ara`ya EKLENİR.
//      -> Toplama, zaten yuvarlanmış tam kuruşlar üzerinden yapılır.
//   3) kdvKurus = round(ara.kurus() * kdvOraniYuzde / 100.0)
//      -> TEK bir yuvarlama noktası. kdvOraniYuzde TAM SAYI yüzdedir (20 = %20),
//         quotes.kdv_orani sütunuyla birebir aynı birim.
//   4) genel = ara + kdv
//
// DEBUG    : Toplam tutmuyorsa şu üçlüyü bastırın:
//              qDebug() << lines.size() << ara.kurus() << kdvKurus;
//            • lines.size() beklenenden azsa sorun Calculator'da DEĞİL,
//              modeli dolduran tarafta (QuoteLineModel) demektir.
//            • kdvOraniYuzde 0 geliyorsa KDV kutusu işaretlenmemiştir;
//              kdvKurus 0 çıkar ve genel == ara olur.
// TUZAK    : kdvOraniYuzde'yi 0,20 gibi ONDALIK vermeyin — int parametre
//            0'a kırpılır ve KDV sessizce sıfır çıkar.
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
