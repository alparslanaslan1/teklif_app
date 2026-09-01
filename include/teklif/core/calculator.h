#pragma once

#include "teklif/core/money.h"

#include <QVector>

// Tek bir teklif kalemini hesaplamak için gereken minimum bilgi.
//
// Açıklama, birim adı, satır notu gibi görüntüleme alanları BİLEREK burada
// yok — Calculator sadece sayılarla ilgilenir. Kalıcı (veritabanına yazılan)
// QuoteLine modeli Part 3/4'te ayrıca tanımlanacak; UI o modeli doldurup
// hesap için buradaki sayıları (miktar, birimFiyat, iskonto) Calculator'a
// verecek. İki modeli bilerek ayırıyoruz: hesaplama mantığı UI/DB şemasından
// bağımsız kalsın, hangisi değişirse değişsin diğeri etkilenmesin.
struct CalcLine
{
    double miktar = 0.0;
    Money birimFiyat;
    // Veri modelinde alan olarak duruyor ama şu an hiçbir ekran bunu
    // doldurmuyor — her zaman Money(0) gelecek, yani pratikte etkisiz.
    // İskonto özelliği ileride açıldığında hesap tarafında ek iş çıkmasın
    // diye şimdiden burada.
    Money iskonto;
};

struct QuoteTotals
{
    Money araToplam;
    Money kdvTutari;
    Money genelToplam;
};

class Calculator
{
public:
    // miktar × birimFiyat - iskonto, en yakın kuruşa yuvarlanarak.
    // Yuvarlama SATIR bazında yapılır: önce her satır kendi içinde
    // yuvarlanır, sonra satırlar toplanır. Tersi (önce double olarak
    // topla, sonra bir kere yuvarla) farklı ve genelde yanlış bir sonuç
    // verir — bkz. test_calculator.cpp / arithmeticNoPrecisionLoss.
    // Sonuç negatif çıkarsa (iskonto satır tutarını aşarsa) 0'a sabitlenir.
    static Money lineTotal(double miktar, Money birimFiyat, Money iskonto = Money(0));

    // Tüm kalemlerin (zaten kuruşa yuvarlanmış) satır tutarlarını toplar,
    // üzerine kdvOraniYuzde uygular. kdvOraniYuzde tam sayı yüzdedir
    // (örn. 20 -> %20); veritabanı şemasındaki quotes.kdv_orani ile birebir
    // aynı birim.
    static QuoteTotals totals(const QVector<CalcLine> &lines, int kdvOraniYuzde);
};
