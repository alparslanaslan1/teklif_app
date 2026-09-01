#pragma once

#include "teklif/core/settings.h"

// Arayüz ölçeği (%85–150).
//
// BELGE YAZI BOYUTUNDAN AYRIDIR: bu yalnızca ekrandaki yazı/menü/tablo
// boyutunu etkiler, yazdırılan belgeye hiç dokunmaz. Belge boyutu
// Settings::keyDocumentFontPt() ile ayrılır ve DocumentContext.fontPt'ye
// gider. İkisi karıştırılırsa kullanıcı ekranı büyüttüğünde çıktı da
// büyür — istenmeyen ve fark edilmesi zor bir yan etki.
namespace Theme {

constexpr int kMinScale = 85;
constexpr int kMaxScale = 150;
constexpr int kDefaultScale = 100;
constexpr int kScaleStep = 10;

// Uygulama genelindeki yazı tipini verilen yüzdeye göre ölçekler.
// Ölçek aralık dışındaysa sınıra çekilir.
void applyUiScale(int yuzde);

// Son uygulanan ölçek. Henüz uygulanmadıysa kDefaultScale.
int currentScale();

// Ayarlardaki ölçeği okuyup uygular. Program açılışında çağrılır.
void applyFromSettings(const Settings &settings);

// Ölçeği hem uygular hem ayarlara yazar (yeniden başlatınca korunsun).
bool setAndStore(Settings &settings, int yuzde, QString *errorOut = nullptr);

} // namespace Theme
