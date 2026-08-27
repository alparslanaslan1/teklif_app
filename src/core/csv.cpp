#include "csv.h"

#include <QStringList>

namespace {

// RFC4180'e uygun düşük seviye tokenizer: tırnaklı alan içindeki virgül ve
// satır sonlarını metin sayar, çift tırnağı kaçış olarak çözer (""->").
// Satır bazlı split() KULLANILMAZ — tırnaklı bir alan içinde gerçek bir
// satır sonu (\n) olabilir, o zaman naif satır bölme yanlış sonuç verir.

// ═══ csvTokenize() ════════════════════════════════════════════════════════
// NE YAPAR : Ham CSV metnini satır/alan matrisine böler (RFC4180 uyumlu).
//            Henüz hiçbir anlam yüklemez — sadece parçalara ayırır.
//
// NEDEN split() DEĞİL: Tırnak içindeki bir alan GERÇEK satır sonu (\n) ya da
//   virgül içerebilir. icerik.split('\n') yapsaydık böyle bir alan ikiye
//   bölünürdü. Bu yüzden karakter karakter, DURUM MAKİNESİ ile ilerliyoruz.
//
// DURUM MAKİNESİ — tek değişken: `tirnakIcinde`
//   ┌─ tirnakIcinde == TRUE  (alanın içindeyiz, her şey metindir)
//   │    '"' + sonraki de '"'  -> alana tek '"' ekle, i += 2   (kaçış: "" -> ")
//   │    '"' tek başına        -> tirnakIcinde = false          (alan bitti)
//   │    başka her karakter    -> aynen alana ekle (virgül ve \n DAHİL)
//   └─ tirnakIcinde == FALSE (normal akış)
//        '"'  -> tirnakIcinde = true (alan başlıyor)
//        ','  -> alanı kapat, satıra ekle, yeni alana geç
//        '\r' -> YUTULUR (Windows \r\n'de \n zaten satırı bitirecek)
//        '\n' -> satiriBitir(): son alanı da ekle, satırı listeye at, sıfırla
//        diğer-> alana ekle
//
// SON ADIM : Dosya satır sonuyla bitmemişse (son satırda \n yoksa) elde kalan
//            alan/satır kaybolmasın diye satiriBitir() bir kez daha çağrılır.
//
// DEBUG    : Sütun sayısı tutmuyorsa döngü içinde durumu bastırın:
//              qDebug() << i << c << tirnakIcinde << satir.size() << alan;
//            Tipik bulgular:
//            • tirnakIcinde sürekli true kalıyor -> dosyada KAPANMAMIŞ tırnak var.
//              (DİKKAT: bu fonksiyon bunu hata olarak BİLDİRMEZ, sessizce
//               dosyanın kalanını tek alan sayar.)
//            • satir.size() beklenenden fazla -> tırnaksız bir alanın içinde
//              virgül var (ör. fiyat "1.234,56" tırnaksız yazılmış).
QVector<QVector<QString>> csvTokenize(const QString &icerik)
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
        if (c == QLatin1Char(',')) {
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


// ═══ csvAlanKac() ═════════════════════════════════════════════════════════
// NE YAPAR : csvTokenize()'ın TERSİ. Bir alanı CSV'ye yazılabilir hale getirir.
//
// ADIM ADIM:
//   1) Alan ',' '"' '\n' veya '\r' İÇERİYOR MU diye bakılır.
//      İçermiyorsa hiçbir şey yapılmadan aynen döner. (hızlı yol)
//   2) İçeriyorsa: önce içindeki her '"' ikiye katlanır ("" kaçışı),
//      sonra alanın tamamı '"' ile sarılır.
//
// DEBUG    : Excel'de sütunlar kayıyorsa bu fonksiyonun ÇAĞRILDIĞINDAN emin
//            olun:  qDebug() << alan << "->" << csvAlanKac(alan);
//            Örnek:  Ali "Usta", Ltd.  ->  "Ali ""Usta"", Ltd."
// TUZAK    : Fiyat "1.234,56" biçiminde VİRGÜL içerdiği için 1. adımda
//            tırnaklanır — bu doğrudur. Ama Türkçe Excel varsayılan ayracı
//            ';' beklediğinden dosyayı yine de tek sütun görebilir.
QString csvAlanKac(const QString &alan)
{
    if (alan.contains(QLatin1Char(',')) || alan.contains(QLatin1Char('"'))
        || alan.contains(QLatin1Char('\n')) || alan.contains(QLatin1Char('\r'))) {
        QString kacan = alan;
        kacan.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QLatin1Char('"') + kacan + QLatin1Char('"');
    }
    return alan;
}

} // namespace


// ═══ csvSatirlariniAyristir() ═════════════════════════════════════════════
// NE YAPAR : Tokenize edilmiş CSV'yi DOĞRULAR ve CsvItemRow listesine çevirir.
//            "HEPSİ YA DA HİÇBİRİ": tek bir satır bile bozuksa TÜM ayrıştırma
//            başarısız sayılır ve boş liste döner. Yarım içe aktarma olmaz.
//
// ADIM ADIM:
//   1) csvTokenize() ile ham matris alınır.
//   2) TAMAMEN BOŞ satırlar elenir (tek alanlı ve o alan boşsa). Dosya
//      sonundaki fazladan satır sonu bu yüzden hata üretmez.
//   3) Hiç satır kalmadıysa -> "Dosya boş."                      [ÇIKIŞ 1]
//   4) i = 1'DEN başlanır: i = 0 BAŞLIK satırıdır ve KOŞULSUZ atlanır.
//      (Başlığın gerçekten başlık olduğu DOĞRULANMAZ — başlıksız bir dosya
//       verirseniz ilk kaleminiz sessizce kaybolur.)
//   5) Her satır için sırayla:
//      a) Sütun sayısı 5 değilse -> "Satır N: 5 sütun bekleniyordu"  [ÇIKIŞ 2]
//      b) 5 alan trimmed() edilir: kod, ad, birim, fiyatMetni, kategoriAdi
//      c) kod/ad/birim'den biri boşsa -> hata                       [ÇIKIŞ 3]
//         (kategori boş OLABİLİR = kategorisiz)
//      d) Money::fromString(fiyatMetni) -> nullopt ise hata         [ÇIKIŞ 4]
//      e) Hepsi geçtiyse sonuç listesine eklenir.
//
// DEBUG    : Hata mesajındaki satır numarası KULLANICIYA GÖRE'dir
//            (satirNo = i + 1, çünkü 1 numaralı satır başlıktır). Dosyayı
//            editörde açıp o numaraya bakabilirsiniz.
//            İçe aktarma sessizce eksik kalıyorsa 2. ve 4. adımı bastırın:
//              qDebug() << hamSatirlar.size() << satirlar.size() << sonuc.size();
//            hamSatirlar > satirlar ise boş satır elendi (normal).
//            satirlar - 1 != sonuc.size() ise bir yerde erken çıkılmıştır.
QVector<CsvItemRow> csvSatirlariniAyristir(const QString &icerik, QString *errorOut)
{
    const QVector<QVector<QString>> hamSatirlar = csvTokenize(icerik);

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


// ═══ csvOlustur() ═════════════════════════════════════════════════════════
// NE YAPAR : Kalem listesini dışa aktarılabilir CSV metnine çevirir.
//            csvSatirlariniAyristir() ile GİDİŞ-DÖNÜŞ uyumludur.
//
// ADIM ADIM:
//   1) İlk satıra sabit başlık yazılır: "kod,ad,birim,fiyat,kategori"
//   2) Her kalem için:
//      a) categoryId != 0 ise kategoriAdlari haritasından ad bulunur.
//         0 ise (kategorisiz) sütun BOŞ bırakılır.
//         DİKKAT: QHash::value() bulamazsa da boş string döner — silinmiş bir
//         kategori ile kategorisiz kalem çıktıda AYIRT EDİLEMEZ.
//      b) 5 alanın hepsi csvAlanKac()'tan geçirilir.
//      c) Virgülle birleştirilip listeye eklenir.
//   3) Satırlar '\n' ile birleştirilir, sona bir '\n' daha eklenir.
//
// DEBUG    : Çıktı bozuk görünüyorsa ham metni bastırın:
//              qDebug().noquote() << csvOlustur(...).left(500);
// EXCEL TUZAĞI: Çıktıda UTF-8 BOM YOK ve satır sonu sadece '\n'. Türkçe
//            Windows'ta Excel dosyayı cp1254 sanıp ş/ğ/ı harflerini bozar.
//            Excel'de test ediyorsanız önce dosyayı Not Defteri ile
//            "UTF-8 (BOM'lu)" olarak kaydedip deneyin — sorun kaybolursa
//            kaynak burasıdır.
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

    return satirlar.join(QStringLiteral("\n")) + QStringLiteral("\n");
}
