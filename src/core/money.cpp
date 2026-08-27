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

// ═══ moneyPattern() ═══════════════════════════════════════════════════════
// NE YAPAR : Para metnini doğrulayan regex'i TEK bir statik nesne olarak verir.
//
// ADIM ADIM:
//   1) İlk çağrıda `static const QRegularExpression re(...)` bir kez kurulur;
//      sonraki tüm çağrılar AYNI nesneyi döndürür (regex yeniden derlenmez).
//   2) Desenin parçaları:
//        ^-?                        -> isteğe bağlı eksi işareti
//        (\d+ | \d{1,3}(?:\.\d{3})+) -> tam kısım: düz rakam VEYA doğru gruplu
//        (,\d{1,2})?$               -> isteğe bağlı virgüllü ondalık (1-2 hane)
//
// DEBUG    : Buraya breakpoint koyarsanız SADECE programın ilk fromString()
//            çağrısında durur (static olduğu için). Regex'i sınamak için:
//              qDebug() << re.isValid() << re.errorString();
//            isValid()==false ise desende yazım hatası vardır.
const QRegularExpression &moneyPattern()
{
    static const QRegularExpression re(
        QStringLiteral(R"(^-?(\d+|\d{1,3}(?:\.\d{3})+)(,\d{1,2})?$)"));
    return re;
}


// ═══ groupThousands() ═════════════════════════════════════════════════════
// NE YAPAR : Salt rakamdan oluşan bir metne binlik NOKTA ayracı serpiştirir.
//            "1234567" -> "1.234.567"
//
// ADIM ADIM:
//   1) i = uzunluk-3 konumundan başlar (sondan 3. karakter).
//   2) i > 0 olduğu sürece o konuma '.' EKLER ve i'yi 3 azaltır.
//   3) i > 0 koşulu kritik: i == 0 olsaydı metnin EN BAŞINA nokta koyardı
//      ("123" -> ".123" gibi bozuk bir çıktı).
//
// DEBUG    : Döngü içinde `digits` değerini izleyin. 6 haneli "123456" için
//            beklenen ara adımlar:  i=3 -> "123.456", sonra i=0 -> döngü biter.
// TUZAK    : Bu fonksiyon EKSİ İŞARETİ BEKLEMEZ. toString() eksiyi ayırıp
//            sadece mutlak değeri gönderdiği için burası hep pozitiftir.
//            Eksi işaretli metin gelirse gruplama bir karakter kayar.
QString groupThousands(QString digits)
{
    for (int i = digits.length() - 3; i > 0; i -= 3)
        digits.insert(i, QChar(u'.'));
    return digits;
}

} // namespace


// ═══ Money::fromString() ══════════════════════════════════════════════════
// NE YAPAR : "1.234,56" gibi Türkçe biçimli metni KURUŞ tam sayısına çevirir.
//            Ayrıştıramazsa istisna fırlatmaz, std::nullopt döner.
//
// ADIM ADIM (bir hata ayıklarken bu sırayı takip edin):
//   1) metin = metinHam.trimmed()  -> baştaki/sondaki boşluklar atılır.
//      Boşsa hemen nullopt.                       [ÇIKIŞ 1]
//   2) moneyPattern().match(metin) -> desene uymuyorsa nullopt. [ÇIKIŞ 2]
//      Buraya düşüyorsanız girdi ya nokta ondalık kullanıyor ("12.5"),
//      ya hatalı gruplu ("12.34.56"), ya da 2'den fazla ondalık haneli.
//   3) negatif = metin '-' ile mi başlıyor  (regex grubu eksiyi İÇERMEZ).
//   4) tamKisim  = m.captured(1); içindeki tüm '.' silinir -> saf rakam.
//   5) ondalikKisim = m.captured(2); ',' silinir. Boşsa "00", tek haneyse
//      sonuna '0' eklenir ("5" -> "50", yani ",5" = 50 kuruş).
//   6) İkisi de toLongLong ile sayıya çevrilir; biri bile başarısızsa
//      nullopt.                                    [ÇIKIŞ 3]
//   7) kurus = tl * 100 + kr; negatifse işareti çevrilir.
//
// DEBUG    : 2. adımdan sonra şunu bastırın:
//              qDebug() << m.hasMatch() << m.captured(1) << m.captured(2);
//            "1.234,5" için beklenen: true "1.234" ",5"
//            7. adımdan önce: qDebug() << tamKisim << ondalikKisim;
//            "1234" ve "50" görmelisiniz -> sonuç 123450 kuruş.
//
// TUZAK    : 7. adımdaki `tl * 100` çok büyük girdilerde SESSİZCE taşar
//            (regex basamak sayısını sınırlamıyor). Beklenmedik negatif bir
//            sonuç görüyorsanız önce girdinin uzunluğuna bakın.
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


// ═══ Money::fromDouble() ══════════════════════════════════════════════════
// NE YAPAR : Ondalık TL değerini (ör. çarpımdan gelen ara sonuç) en yakın
//            kuruşa yuvarlayarak Money'ye çevirir.
//
// ADIM ADIM:
//   1) tl * 100.0  -> TL'yi kuruş ölçeğine taşır (hâlâ double).
//   2) std::llround -> en yakın tam sayıya yuvarlar (0,5 sıfırdan UZAĞA;
//      yani 2,5 -> 3 ve -2,5 -> -3). Bankacı yuvarlaması DEĞİLDİR.
//
// DEBUG    : Yuvarlama şüphesinde ham double'ı tam basamakla bastırın:
//              qDebug() << QString::number(tl * 100.0, 'f', 10);
//            Klasik tuzak: 1,15 * 100 = 114,99999999999999 -> llround 115 (doğru).
//            Ama 8,285 gibi değerler double'da 8,28499... olabilir -> 828 çıkar.
Money Money::fromDouble(double tl) noexcept
{
    return Money(static_cast<qint64>(std::llround(tl * 100.0)));
}


// ═══ Money::toString() ════════════════════════════════════════════════════
// NE YAPAR : Kuruşu "1.234,56" biçiminde ekrana/CSV'ye uygun metne çevirir.
//            HER ZAMAN 2 ondalık hane basar.
//
// ADIM ADIM:
//   1) mutlak = |m_kurus|  -> işaret ayrı ele alınacağı için önce soyulur.
//   2) tl = mutlak / 100   (tam bölme)   ve   kr = mutlak % 100.
//   3) tl groupThousands() ile binlik noktalanır.
//   4) kr, rightJustified(2,'0') ile 2 haneye tamamlanır (5 -> "05").
//   5) Orijinal değer negatifse en başa '-' eklenir.
//
// DEBUG    : -1234 kuruş için ara değerler: mutlak=1234, tl=12, kr=34
//            -> "12" + "," + "34" -> "-12,34".
//            Sonuçta ',00' yerine ',0' görüyorsanız 4. adım (rightJustified)
//            atlanmış demektir.
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


// ═══ Money::operator*(double) ═════════════════════════════════════════════
// NE YAPAR : Tutarı kesirli bir katsayıyla (miktar) çarpar ve SONUCU HEMEN
//            kuruşa yuvarlar. Projedeki en kritik yuvarlama noktasıdır.
//
// ADIM ADIM:
//   1) m_kurus double'a çevrilir (tam sayı bölmesi olmasın diye).
//   2) kat ile çarpılır -> kesirli kuruş.
//   3) llround ile en yakın tam kuruşa indirilir.
//
// NEDEN ÖNEMLİ: Calculator::lineTotal her satırı BURADA yuvarlar. Yuvarlama
//   satır satır yapıldığı için toplamda 1 kuruşluk sapma birikmez. Önce
//   double toplayıp sonra tek sefer yuvarlamak FARKLI sonuç verir.
//
// DEBUG    : Satır tutarı 1 kuruş şaşıyorsa yuvarlamadan önceki ham değeri
//            bastırın:  qDebug() << QString::number(m_kurus * kat, 'f', 6);
//            .5 sınırındaki değerlerde (ör. 1250.5) llround YUKARI yuvarlar.
Money Money::operator*(double kat) const noexcept
{
    return Money(static_cast<qint64>(std::llround(static_cast<double>(m_kurus) * kat)));
}


// ═══ Money::operator/(qint64) ═════════════════════════════════════════════
// NE YAPAR : Tutarı tam sayıya böler (taksitlendirme gibi az sayıda kullanım).
//
// ADIM ADIM:
//   1) payda == 0 ise ÇÖKMEZ, Money(0) döner.   [KORUMA — sessiz davranış!]
//   2) Değilse double'a çevirip böler, llround ile en yakın kuruşa yuvarlar.
//
// DEBUG    : Beklenmedik bir şekilde 0 alıyorsanız ilk şüpheli payda'nın 0
//            olmasıdır — fonksiyon bunu hata olarak BİLDİRMEZ, sessizce
//            0 döndürür. Çağırmadan önce payda'yı bastırın.
// TUZAK    : Bölmede kalan kaybolur. 100 kuruşu 3'e bölerseniz 3 taksit
//            33+33+33 = 99 eder; eksik 1 kuruşu çağıran taraf dağıtmalıdır.
Money Money::operator/(qint64 payda) const noexcept
{
    if (payda == 0)
        return Money(0);
    return Money(static_cast<qint64>(std::llround(static_cast<double>(m_kurus) / payda)));
}
