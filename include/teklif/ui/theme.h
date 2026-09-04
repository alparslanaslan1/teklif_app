#pragma once

#include "teklif/core/settings.h"

// Uygulamanın görünümü: renk paleti, stil sayfası ve arayüz ölçeği.
//
// ARAYÜZ ÖLÇEĞİ BELGE YAZI BOYUTUNDAN AYRIDIR: ölçek yalnızca ekrandaki
// yazı/menü/tablo boyutunu etkiler, yazdırılan belgeye hiç dokunmaz. Belge
// boyutu Settings::keyDocumentFontPt() ile ayrılır ve DocumentContext.fontPt'ye
// gider. İkisi karıştırılırsa kullanıcı ekranı büyüttüğünde çıktı da büyür —
// istenmeyen ve fark edilmesi zor bir yan etki.
namespace Theme {

constexpr int kMinScale = 85;
constexpr int kMaxScale = 150;
constexpr int kDefaultScale = 100;
constexpr int kScaleStep = 10;

// Tüm uygulamaya stil sayfasını ve temel yazı tipini uygular.
//
// NEDEN QSS, NEDEN QPalette DEĞİL: köşe yuvarlaklığı, hücre iç boşluğu,
// odak çerçevesi gibi şeyler palet ile ifade edilemez; ayrıca Windows'un
// yerel çizimi platformdan platforma değişirken QSS her yerde aynı sonucu
// verir — ekranda gördüğümüz ile kullanıcının gördüğü aynı olur.
//
// Program açılışında, pencereler kurulmadan ÖNCE çağrılır.
void applyStyle();

// Uygulama genelindeki yazı tipini verilen yüzdeye göre ölçekler.
// Ölçek aralık dışındaysa sınıra çekilir.
void applyUiScale(int yuzde);

// Son uygulanan ölçek. Henüz uygulanmadıysa kDefaultScale.
int currentScale();

// Stil sayfasını kurar ve ayarlardaki ölçeği uygular. Program açılışında
// çağrılır.
void applyFromSettings(const Settings &settings);

// Ölçeği hem uygular hem ayarlara yazar (yeniden başlatınca korunsun).
bool setAndStore(Settings &settings, int yuzde, QString *errorOut = nullptr);

} // namespace Theme
