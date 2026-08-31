#include "theme.h"

#include <QApplication>
#include <QFont>

namespace Theme {
namespace {

// Programın açılıştaki temel yazı boyutu. Ölçekleme her zaman BUNUN üzerine
// uygulanır; bir önceki ölçeğin üzerine uygulansaydı %110 iki kez
// seçildiğinde yazı %121'e çıkardı (bileşik büyüme).
double basePointSize()
{
    static const double taban = [] {
        const double p = QApplication::font().pointSizeF();
        // Bazı platformlarda font piksel cinsinden tanımlıdır ve
        // pointSizeF() -1 döner; o durumda makul bir varsayılana düşülür.
        return p > 0 ? p : 9.0;
    }();
    return taban;
}

int g_mevcut = kDefaultScale;

} // namespace

void applyUiScale(int yuzde)
{
    const int olcek = qBound(kMinScale, yuzde, kMaxScale);

    QFont f = QApplication::font();
    f.setPointSizeF(basePointSize() * olcek / 100.0);
    QApplication::setFont(f);

    g_mevcut = olcek;
}

int currentScale()
{
    return g_mevcut;
}

void applyFromSettings(const Settings &settings)
{
    applyUiScale(static_cast<int>(settings.intValueOr(Settings::keyUiScale(), kDefaultScale)));
}

bool setAndStore(Settings &settings, int yuzde, QString *errorOut)
{
    const int olcek = qBound(kMinScale, yuzde, kMaxScale);
    applyUiScale(olcek);
    return settings.setInt(Settings::keyUiScale(), olcek, errorOut);
}

} // namespace Theme
