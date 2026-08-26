#pragma once

#include <QString>
#include <optional>

// Türkçe ondalık sayı ayrıştırması: Money::fromString ile AYNI dilbilgisi
// kuralları (nokta binlik ayraç, virgül ondalık ayraç, hatalı gruplama
// reddedilir — örn. "12.34.56" veya salt nokta ondalık "12.5" kabul
// edilmez) ama sonuç kuruşa değil düz double'a döner ve 1-4 ondalık
// basamağa izin verir.
//
// Neden ayrı bir fonksiyon: miktar alanları (0,333 m² gibi) paranın 2
// haneye sabit kuruş kısıtına tabi değil, ama "virgül ondalık, nokta
// binlik" kuralı UYGULAMA GENELİNDE tek ve tutarlı olmalı — teklif
// tablosundaki Miktar hücresi ile popup'taki Miktar alanı bunu kullanır.
std::optional<double> parseTurkishNumber(const QString &metin);
