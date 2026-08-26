#include "numtowords.h"

#include <QStringList>

namespace {

const QStringList &birler()
{
    static const QStringList v = {"",     "bir",  "iki",  "üç",    "dört",
                                   "beş",  "altı", "yedi", "sekiz", "dokuz"};
    return v;
}

const QStringList &onlar()
{
    static const QStringList v = {"",      "on",     "yirmi",  "otuz",   "kırk",
                                   "elli",  "altmış", "yetmiş", "seksen", "doksan"};
    return v;
}

// 3'erli basamak grupları için ölçek adları: 0. grup ("") birler-yüzler,
// 1. grup "bin", 2. grup "milyon" ...
const QStringList &olcekler()
{
    static const QStringList v = {"", "bin", "milyon", "milyar", "trilyon"};
    return v;
}

// 0-999 arası bir grubu yazıya döker. idx, bu grubun ölçeğidir (1 = "bin"
// grubu). Türkçede "1000" -> "bin" denir, "birbin" denmez; ama bu bastırma
// SADECE bin grubunda ve SADECE yüzler/onlar hanesi boşken geçerlidir:
// 1234 -> "...dörtbin" (dört bastırılmaz), 1000 -> "bin" (bir bastırılır).
// "Yüz" grubunda ayrı bir kural var: 100 -> "yüz" (yüzler hanesindeki 1
// her zaman bastırılır, gruptan bağımsız) — o BIR[yuz] seçiminde ayrıca
// ele alınıyor, burada karıştırılmamalı.
QString grupYaziyla(int grup, int idx)
{
    const int yuz = grup / 100;
    const int on = (grup % 100) / 10;
    const int bir = grup % 10;

    QString s;
    if (yuz > 0)
        s += (yuz == 1 ? QString() : birler()[yuz]) + QStringLiteral("yüz");
    s += onlar()[on];

    const bool birBastir = (idx == 1 && bir == 1 && yuz == 0 && on == 0);
    if (!birBastir)
        s += birler()[bir];

    return s;
}

} // namespace

QString sayiYaziyla(qint64 n)
{
    if (n == 0)
        return QStringLiteral("sıfır");

    const bool negatif = n < 0;
    // qint64 min değerinin mutlak değeri qint64'e sığmaz; quint64'e geçerek
    // bu uç durumu da güvenli ele alıyoruz (pratikte teklif tutarlarında
    // asla görülmeyecek bir büyüklük, ama fonksiyon çökmemeli).
    const quint64 mutlak = negatif ? (static_cast<quint64>(-(n + 1)) + 1) : static_cast<quint64>(n);

    QStringList parcalar;
    quint64 kalan = mutlak;
    int idx = 0;
    while (kalan > 0) {
        const int grup = static_cast<int>(kalan % 1000);
        if (grup != 0) {
            const QString olcek = idx < olcekler().size() ? olcekler()[idx] : QString();
            parcalar.prepend(grupYaziyla(grup, idx) + olcek);
        }
        kalan /= 1000;
        idx++;
    }

    QString sonuc = parcalar.join(QString());
    if (negatif)
        sonuc.prepend(QStringLiteral("eksi "));
    return sonuc;
}

QString tutarYaziyla(const Money &tutar)
{
    const qint64 kurus = tutar.kurus();
    const qint64 mutlakKurus = kurus < 0 ? -kurus : kurus;
    const qint64 tl = mutlakKurus / 100;
    const qint64 kr = mutlakKurus % 100;

    QString sonuc = sayiYaziyla(tl) + QStringLiteral(" TL");
    if (kr != 0)
        sonuc += QLatin1Char(' ') + sayiYaziyla(kr) + QStringLiteral(" kuruş");
    if (kurus < 0)
        sonuc.prepend(QStringLiteral("eksi "));
    return sonuc;
}
