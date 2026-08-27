#include "numtowords.h"

#include <QStringList>

namespace {


// ═══ birler() ═════════════════════════════════════════════════════════════
// NE YAPAR : 0-9 rakamlarının Türkçe adlarını verir. 0. eleman BİLEREK boş
//            string'tir: birler()[0] eklendiğinde hiçbir şey eklenmemiş olur,
//            böylece çağıran tarafta "sıfırsa ekleme" kontrolü gerekmez.
// DEBUG    : İndeks daima 0-9 aralığında olmalı. Dışına çıkarsanız QStringList
//            operator[] TANIMSIZ DAVRANIŞ üretir (bounds check yok).
const QStringList &birler()
{
    static const QStringList v = {"",     "bir",  "iki",  "üç",    "dört",
                                   "beş",  "altı", "yedi", "sekiz", "dokuz"};
    return v;
}


// ═══ onlar() ══════════════════════════════════════════════════════════════
// NE YAPAR : Onlar basamağının Türkçe adlarını verir (10=on, 20=yirmi ...).
//            birler() gibi 0. eleman boş string'tir.
// NOT      : Türkçede İngilizcedeki "thirteen" gibi birleşik sözcükler yoktur;
//            13 = "on" + "üç" olarak doğrudan birleşir. Bu yüzden 11-19 için
//            ayrı bir tablo GEREKMEZ.
const QStringList &onlar()
{
    static const QStringList v = {"",      "on",     "yirmi",  "otuz",   "kırk",
                                   "elli",  "altmış", "yetmiş", "seksen", "doksan"};
    return v;
}

// 3'erli basamak grupları için ölçek adları: 0. grup ("") birler-yüzler,
// 1. grup "bin", 2. grup "milyon" ...

// ═══ olcekler() ═══════════════════════════════════════════════════════════
// NE YAPAR : 3'erli basamak gruplarının ölçek adları.
//            idx 0 = "" (birler-yüzler), 1 = "bin", 2 = "milyon", 3 = "milyar",
//            4 = "trilyon".
// TUZAK    : Liste 5 elemanda BİTİYOR. qint64 ~9,2 KATRİLYONA kadar çıkabilir,
//            yani idx 5 ve 6 mümkündür. O durumda sayiYaziyla() ölçek yerine
//            BOŞ STRING koyar -> hata vermeden YANLIŞ yazı üretir.
//            Katrilyon mertebesinde bir sonuç görüyorsanız şüpheli burasıdır.
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

// ═══ grupYaziyla() ════════════════════════════════════════════════════════
// NE YAPAR : 0-999 arası TEK bir basamak grubunu yazıya döker.
//            `idx` bu grubun ölçeğidir (1 = "bin" grubu) ve sadece aşağıdaki
//            "bir" bastırma kuralı için gereklidir.
//
// ADIM ADIM:
//   1) Grup üç basamağa ayrılır:  yuz = grup/100, on = (grup%100)/10, bir = grup%10
//   2) YÜZLER: yuz > 0 ise "yüz" eklenir. yuz == 1 ise önüne "bir" KONMAZ
//      (100 -> "yüz", asla "biryüz"). Bu kural her grupta geçerlidir.
//   3) ONLAR: onlar()[on] doğrudan eklenir (0 ise boş string gelir).
//   4) BİRLER: burada özel kural var —
//        birBastir = (idx == 1 && bir == 1 && yuz == 0 && on == 0)
//      Yani SADECE "bin" grubunda VE grup tam olarak 1 iken "bir" bastırılır:
//        1000  -> "bin"        (bir bastırıldı)
//        1001  -> "binbir"     (birler grubu idx=0, bastırılmaz)
//        4000  -> "dörtbin"    (bir != 1, bastırılmaz)
//        21000 -> "yirmibirbin"(on != 0, bastırılmaz)  <-- en sık yanlış anlaşılan
//
// DEBUG    : Yanlış bir sözcük görüyorsanız üç basamağı ve bayrağı bastırın:
//              qDebug() << grup << idx << yuz << on << bir << birBastir;
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


// ═══ sayiYaziyla() ════════════════════════════════════════════════════════
// NE YAPAR : Tam sayıyı Türkçe yazıya çevirir. Sözcükler BİTİŞİK yazılır
//            ("binikiyüzotuzdört") — teklif/sözleşme belgelerindeki yerleşik
//            Türkçe yazım geleneği budur.
//
// ADIM ADIM:
//   1) n == 0 ise kısayol: "sıfır".                              [ÇIKIŞ 1]
//   2) negatif bayrağı alınır ve mutlak değer QUINT64'e taşınır.
//      Neden quint64: qint64'ün en küçük değerinin (-9223372036854775808)
//      mutlak değeri qint64'e SIĞMAZ; `-n` yapmak taşma olurdu. Bu yüzden
//      `-(n+1) + 1` hilesi kullanılır.
//   3) Sayı SAĞDAN SOLA 3'erli gruplara ayrılır (kalan = kalan / 1000):
//      a) grup = kalan % 1000
//      b) grup 0 ise ATLANIR -> "birmilyonsıfırbinüç" gibi çıktı oluşmaz.
//      c) grup != 0 ise grupYaziyla(grup, idx) + olcekler()[idx] üretilip
//         listenin BAŞINA eklenir (prepend), çünkü sağdan sola gidiyoruz.
//   4) Parçalar ayraçsız birleştirilir; negatifse başa "eksi " gelir.
//
// DEBUG    : Döngü içinde her turu bastırmak neredeyse her hatayı gösterir:
//              qDebug() << "idx" << idx << "grup" << grup << "parca" << parcalar;
//            1.000.000 için beklenen turlar:
//              idx=0 grup=0   (atlanır)
//              idx=1 grup=0   (atlanır)
//              idx=2 grup=1   -> "birmilyon"
//            Not: burada "bir" bastırılmaz; bastırma SADECE idx==1'de olur.
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


// ═══ tutarYaziyla() ═══════════════════════════════════════════════════════
// NE YAPAR : Kuruş cinsinden tutarı "... TL ... kuruş" biçiminde yazıya döker.
//            Teklif belgesinin altındaki "yalnız ..." satırı için kullanılır.
//
// ADIM ADIM:
//   1) İşaret ayrılır, mutlak kuruş alınır.
//   2) tl = mutlakKurus / 100   ve   kr = mutlakKurus % 100.
//   3) sayiYaziyla(tl) + " TL"  her zaman yazılır (tl 0 olsa bile "sıfır TL").
//   4) kr != 0 İSE " " + sayiYaziyla(kr) + " kuruş" eklenir.
//      kr == 0 ise kuruş kısmı HİÇ yazılmaz -> "yüz TL", asla
//      "yüz TL sıfır kuruş" değil.
//   5) Orijinal tutar negatifse en başa "eksi " eklenir.
//
// DEBUG    : Örnek ara değerler (12345 kuruş): tl=123, kr=45
//              -> "yüzyirmiüç TL kırkbeş kuruş"
//            Beklenmedik "sıfır kuruş" görüyorsanız 4. adımdaki kr != 0
//            koşulu atlanmıştır.  qDebug() << tl << kr;
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
