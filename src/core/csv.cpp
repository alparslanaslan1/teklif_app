#include "teklif/core/csv.h"

#include <QStringList>

namespace {

// RFC4180'e uygun düşük seviye tokenizer: tırnaklı alan içindeki virgül ve
// satır sonlarını metin sayar, çift tırnağı kaçış olarak çözer (""->").
// Satır bazlı split() KULLANILMAZ — tırnaklı bir alan içinde gerçek bir
// satır sonu (\n) olabilir, o zaman naif satır bölme yanlış sonuç verir.

// Ham CSV metnini satır/alan matrisine böler; henüz hiçbir anlam yüklemez.
//   tirnakIcinde : tırnaklı bir alanın içinde miyiz. Tüm mantık buna bağlıdır —
//                  içerideyken virgül ve satır sonu metin sayılır, çift tırnak
//                  ("") ise tek tırnak kaçışıdır.
//   alan         : biriktirilmekte olan mevcut alan
//   satir        : biriktirilmekte olan mevcut satırın alanları
//   satiriBitir  : son alanı satıra ekleyen, satırı listeye atan ve ikisini de
//                  sıfırlayan yardımcı
// split() KULLANILMAZ: tırnaklı bir alanın içinde gerçek bir satır sonu
// olabilir, naif satır bölme o durumda yanlış sonuç verir.
// Dosya satır sonuyla bitmemişse elde kalan alan/satır için satiriBitir()
// bir kez daha çağrılır.
QVector<QVector<QString>> csvTokenize(const QString &icerik, QChar ayrac)
{
    QVector<QVector<QString>> satirlar;
    QVector<QString> satir;
    QString alan;
    bool tirnakIcinde = false;
    const int n = icerik.length();
    int i = 0;

    auto satiriBitir = [&]() {
        satir.append(alan);
        alan.clear();
        satirlar.append(satir);
        satir.clear();
    };

    while (i < n) {
        const QChar c = icerik.at(i);

        if (tirnakIcinde) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < n && icerik.at(i + 1) == QLatin1Char('"')) {
                    alan += QLatin1Char('"');
                    i += 2;
                    continue;
                }
                tirnakIcinde = false;
                ++i;
                continue;
            }
            alan += c;
            ++i;
            continue;
        }

        if (c == QLatin1Char('"')) {
            tirnakIcinde = true;
            ++i;
            continue;
        }
        if (c == ayrac) {
            satir.append(alan);
            alan.clear();
            ++i;
            continue;
        }
        if (c == QLatin1Char('\r')) {
            ++i; // \r\n durumunda \n zaten satırı bitirecek; tek başına \r'yi de yut
            continue;
        }
        if (c == QLatin1Char('\n')) {
            satiriBitir();
            ++i;
            continue;
        }

        alan += c;
        ++i;
    }

    // Son satır bir satır sonuyla bitmemiş olabilir.
    if (!alan.isEmpty() || !satir.isEmpty())
        satiriBitir();

    return satirlar;
}


// csvTokenize()'ın tersi: bir alanı CSV'ye yazılabilir hale getirir.
// Alan virgül, tırnak veya satır sonu içermiyorsa aynen döner. İçeriyorsa
// içindeki tırnaklar ikiye katlanır ("" kaçışı) ve alanın tamamı tırnağa alınır.
QString csvAlanKac(const QString &alan)
{
    // Fiyat alanı Türkçe biçimde virgül içerir ("1.234,56"); tırnaklanmazsa
    // virgül ayracıyla yazılan bir dosyada iki sütuna bölünürdü.
    if (alan.contains(QLatin1Char(',')) || alan.contains(QLatin1Char(';'))
        || alan.contains(QLatin1Char('"'))
        || alan.contains(QLatin1Char('\n')) || alan.contains(QLatin1Char('\r'))) {
        QString kacan = alan;
        kacan.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QLatin1Char('"') + kacan + QLatin1Char('"');
    }
    return alan;
}


// Dosyanın hangi ayracı kullandığını BAŞLIK SATIRINDAN anlar.
//
// NEDEN GEREKLİ: Türkçe Windows'ta Excel, listeyi noktalı virgülle ayırır
// (bölgesel ondalık ayracı virgül olduğu için). Kullanıcı dışa aktardığımız
// dosyayı Excel'de açıp kaydederse geri gelen dosya ';' ayraçlı olur. Sadece
// virgül beklersek o dosya tek sütun okunur ve "5 sütun bekleniyordu, 1
// bulundu" hatası verir.
//
// Yalnızca ilk satıra bakılır: başlık ("kod,ad,birim,fiyat,kategori") tırnak
// içermez, dolayısıyla sayım güvenilirdir. Gövdedeki fiyat alanları virgül
// içerdiği için tüm dosyayı saymak yanıltıcı olurdu.
QChar ayraciTespitEt(const QString &icerik)
{
    const int satirSonu = icerik.indexOf(QLatin1Char('\n'));
    const QString ilkSatir = satirSonu < 0 ? icerik : icerik.left(satirSonu);

    return ilkSatir.count(QLatin1Char(';')) > ilkSatir.count(QLatin1Char(','))
               ? QLatin1Char(';')
               : QLatin1Char(',');
}

} // namespace


// Tokenize edilmiş CSV'yi doğrular ve CsvItemRow listesine çevirir. Tek bir
// satır bile bozuksa tüm ayrıştırma başarısız sayılır (boş liste + errorOut) —
// yarım bir içe aktarmadan iyidir.
//   hamSatirlar  : tokenizer çıktısı
//   satirlar     : tamamen boş satırları elenmiş hali. Dosya sonundaki fazladan
//                  satır sonu ya da araya sıkışmış boş satır hata sayılmaz.
//   SUTUN_SAYISI : beklenen sütun sayısı (kod, ad, birim, fiyat, kategori)
//   satirNo      : kullanıcıya gösterilen satır numarası (1 numaralı satır
//                  başlıktır, bu yüzden i + 1)
// Döngü i = 1'den başlar: 0. satır başlıktır ve atlanır.
// kod/ad/birim boş olamaz; kategori boş olabilir (= kategorisiz).
QVector<CsvItemRow> csvSatirlariniAyristir(const QString &icerik, QString *errorOut)
{
    // Excel'in yazdığı UTF-8 dosyalar BOM (U+FEFF) ile başlar. Temizlenmezse
    // ilk sütunun adı "\uFEFFkod" olur; başlık atlandığı için bu doğrudan
    // görünmez ama tırnaklı bir ilk alan bozulurdu.
    QString temiz = icerik;
    if (temiz.startsWith(QChar(0xFEFF)))
        temiz.remove(0, 1);

    const QVector<QVector<QString>> hamSatirlar = csvTokenize(temiz, ayraciTespitEt(temiz));

    // Tamamen boş satırları (örn. dosya sonundaki fazladan boş satır, ya da
    // elle düzenlenmiş bir dosyada araya sıkıştış boş satır) yok say —
    // bunlar bir hata değil, CSV dosyalarında sık rastlanan bir durum.
    QVector<QVector<QString>> satirlar;
    for (const auto &s : hamSatirlar) {
        if (s.size() == 1 && s.first().trimmed().isEmpty())
            continue;
        satirlar.append(s);
    }

    if (satirlar.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Dosya boş.");
        return {};
    }

    constexpr int SUTUN_SAYISI = 5;
    QVector<CsvItemRow> sonuc;

    // i=0 başlık satırıdır, atlanır.
    for (int i = 1; i < satirlar.size(); ++i) {
        const QVector<QString> &s = satirlar[i];
        const int satirNo = i + 1; // kullanıcıya gösterilen (1: başlık) satır no

        if (s.size() != SUTUN_SAYISI) {
            if (errorOut) {
                *errorOut = QStringLiteral("Satır %1: %2 sütun bekleniyordu, %3 bulundu.")
                                .arg(satirNo)
                                .arg(SUTUN_SAYISI)
                                .arg(s.size());
            }
            return {};
        }

        const QString kod = s[0].trimmed();
        const QString ad = s[1].trimmed();
        const QString birim = s[2].trimmed();
        const QString fiyatMetni = s[3].trimmed();
        const QString kategoriAdi = s[4].trimmed();

        if (kod.isEmpty() || ad.isEmpty() || birim.isEmpty()) {
            if (errorOut)
                *errorOut = QStringLiteral("Satır %1: kod, ad ve birim boş olamaz.").arg(satirNo);
            return {};
        }

        const std::optional<Money> fiyat = Money::fromString(fiyatMetni);
        if (!fiyat.has_value()) {
            if (errorOut) {
                *errorOut =
                    QStringLiteral("Satır %1: geçersiz fiyat \"%2\".").arg(satirNo).arg(fiyatMetni);
            }
            return {};
        }

        sonuc.append(CsvItemRow{kod, ad, birim, fiyat.value(), kategoriAdi});
    }

    return sonuc;
}


// Kalem listesini CSV metnine döker; csvSatirlariniAyristir() ile gidiş-dönüş
// uyumludur.
//   kategoriAdlari : categoryId -> ad sözlüğü. Kalem başına ayrı sorgu
//                    atılmasın diye hazır olarak dışarıdan verilir.
//   kategori       : kalemin kategori adı; categoryId 0 ise (kategorisiz) boş
// Her alan csvAlanKac()'tan geçirilir; ilk satır sabit başlık satırıdır.
QString csvOlustur(const QVector<Item> &kalemler, const QHash<qint64, QString> &kategoriAdlari)
{
    QStringList satirlar;
    satirlar << QStringLiteral("kod,ad,birim,fiyat,kategori");

    for (const Item &it : kalemler) {
        const QString kategori = it.categoryId != 0 ? kategoriAdlari.value(it.categoryId) : QString();
        const QStringList alanlar = {
            csvAlanKac(it.kod),
            csvAlanKac(it.ad),
            csvAlanKac(it.birim),
            csvAlanKac(it.varsayilanFiyat.toString()),
            csvAlanKac(kategori),
        };
        satirlar << alanlar.join(QLatin1Char(','));
    }

    // BOM: Türkçe Windows'ta Excel, BOM'suz bir UTF-8 dosyayı cp1254 sanıp
    // ş/ğ/ı/İ harflerini bozar. Dosya "Excel'de düzenlenecek" diye
    // tasarlandığı için bu zorunlu.
    // CRLF: Windows araçlarının (Not Defteri dahil) beklediği satır sonu.
    // Kendi ayrıştırıcımız ikisini de zaten kabul ediyor.
    return QChar(0xFEFF) + satirlar.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n");
}
