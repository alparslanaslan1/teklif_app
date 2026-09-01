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

// Para metnini doğrulayan regex'i tek bir statik nesne olarak verir (her
// çağrıda yeniden derlenmesin diye). Desen: isteğe bağlı '-', ardından tam
// kısım (düz rakam ya da doğru gruplanmış "1.234"), ardından isteğe bağlı
// ",dd" ondalık.
const QRegularExpression &moneyPattern()
{
    static const QRegularExpression re(
        QStringLiteral(R"(^-?(\d+|\d{1,3}(?:\.\d{3})+)(,\d{1,2})?$)"));
    return re;
}


// Salt rakamlardan oluşan metne binlik noktası serpiştirir: "1234567" ->
// "1.234.567".
//   digits : üzerinde çalışılan metin (değerle alınır, kopyası değiştirilir)
//   i      : noktanın ekleneceği konum; sondan başlar, 3'er azalır. i > 0
//            koşulu metnin en başına nokta konmasını engeller.
QString groupThousands(QString digits)
{
    for (int i = digits.length() - 3; i > 0; i -= 3)
        digits.insert(i, QChar(u'.'));
    return digits;
}

} // namespace


// "1.234,56" gibi Türkçe biçimli metni kuruş tam sayısına çevirir.
// Çevrilemezse istisna fırlatmaz, std::nullopt döner.
//   metin        : boşlukları kırpılmış girdi
//   m            : regex eşleşmesi; captured(1) tam kısım, captured(2) ondalık
//   negatif      : metin '-' ile mi başlıyor (regex grubu eksiyi içermez)
//   tamKisim     : binlik noktaları silinmiş tam kısım
//   ondalikKisim : virgülü silinmiş ondalık; boşsa "00", tek haneyse sonuna
//                  '0' eklenerek 2 haneye tamamlanır (",5" = 50 kuruş)
//   kurus        : tl * 100 + kr, işareti uygulanmış hali
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

    // TAŞMA KORUMASI: aşağıdaki tl * 100 + kr işlemi qint64'e sığmalı.
    // qint64'ün üst sınırı ~9,2 × 10^18 olduğundan lira kısmı en fazla 16
    // hane olabilir. Regex basamak sayısını sınırlamadığı için bu kontrol
    // olmadan yeterince uzun bir girdi SESSİZCE taşardı (imzalı taşma =
    // tanımsız davranış) ve sonuç negatif bir tutar olarak görünebilirdi.
    // 16 hane = 10 katrilyon TL; gerçek bir teklifte asla ulaşılmaz.
    constexpr int kMaksTamHane = 16;
    if (tamKisim.size() > kMaksTamHane)
        return std::nullopt;

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


// Ondalık bir TL tutarını en yakın kuruşa yuvarlayarak Money'ye çevirir.
// llround klasik yuvarlama yapar (0,5 sıfırdan uzağa), bankacı yuvarlaması değil.
Money Money::fromDouble(double tl) noexcept
{
    return Money(static_cast<qint64>(std::llround(tl * 100.0)));
}


// Kuruşu "1.234,56" biçiminde, her zaman 2 ondalık haneyle metne çevirir.
//   mutlak : işaret ayrı ele alınacağı için soyulmuş kuruş değeri
//   tl     : mutlak / 100 -> lira kısmı, binlik noktalarıyla
//   kr     : mutlak % 100 -> kuruş kısmı, sol sıfırla 2 haneye tamamlanır
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


// Tutarı kesirli bir katsayıyla (miktar) çarpar ve sonucu hemen kuruşa
// yuvarlar. Calculator::lineTotal her satırı burada yuvarlar; yuvarlama satır
// satır yapıldığı için toplamda kuruş sapması birikmez.
Money Money::operator*(double kat) const noexcept
{
    return Money(static_cast<qint64>(std::llround(static_cast<double>(m_kurus) * kat)));
}


// Tutarı tam sayıya böler (taksit bölme gibi az sayıda kullanım için).
//   payda : 0 ise çökmek yerine Money(0) döner
// Kalan kuruş en yakın tam sayıya yuvarlanır.
Money Money::operator/(qint64 payda) const noexcept
{
    if (payda == 0)
        return Money(0);
    return Money(static_cast<qint64>(std::llround(static_cast<double>(m_kurus) / payda)));
}
