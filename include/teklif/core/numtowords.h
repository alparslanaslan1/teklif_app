#pragma once

#include "teklif/core/money.h"

#include <QString>
#include <QtGlobal>

// Bir tam sayıyı Türkçe yazıya döker: 1234 -> "binikiyüzotuzdört".
// Yalnızca sayının kendisi için; TL/kuruş birleştirmesi tutarYaziyla()
// içinde. Kelimeler boşluksuz bitişik yazılır (bu, teklif/sözleşme
// belgelerinde yerleşik Türkçe yazım geleneği — "bin iki yüz" değil
// "binikiyüz").
QString sayiYaziyla(qint64 n);

// Kuruş cinsinden bir tutarı "... TL ... kuruş" biçiminde yazıya döker.
// Kuruş kısmı sıfırsa atlanır: 100,00 TL -> "yüz TL" (asla "yüz TL sıfır
// kuruş" değil).
QString tutarYaziyla(const Money &tutar);
