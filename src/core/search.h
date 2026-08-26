#pragma once

#include "models.h"

#include <QString>
#include <QVector>

// Arama eşleştirmesi için bir metni sadeleştirir: Türkçe İ/I/ı ve Ş/Ğ/Ü/Ö/Ç
// harflerinin hepsi ASCII karşılıklarına indirgenir (İ,I,ı,i -> i; ş -> s;
// ğ -> g; ü -> u; ö -> o; ç -> c). Bu SADECE arama eşleştirmesi için —
// ekranda gösterilecek metni asla bu fonksiyondan geçirme.
//
// Amaç: Türkçe klavyesi olmayan/emin olmayan bir kullanıcı "is" yazınca da
// "İşçilik" bulunsun. Kod, Qt'nin varsayılan (yerel ayarsız) toLower()
// metodunun 'İ' (U+0130) karakterini TEK bir 'i' değil, 'i' + birleşen
// nokta (iki kod noktası, U+0069 U+0307) yapmasını hesaba katar — bu yüzden
// 'İ' toLower() çağrılmadan ÖNCE elle 'i'ye çevrilir.
QString turkceAramaNormalize(const QString &metin);

// items içinde arama yapar. Önce ADI VEYA KODU aranan metinle BAŞLAYAN
// kalemler, sonra aranan metni İÇEREN (ama başlamayan) kalemler gelir;
// her iki grup da kendi içinde orijinal sırayı korur (stabil sıralama).
// Pasif kalemler hiçbir zaman sonuçta çıkmaz. Aranan metin boşsa (veya
// sadece boşluksa) boş liste döner — arama kutusu boşken sonuç listesinin
// kendiliğinden tüm katalogla dolmaması için.
//
// Katalogda önceden yüklenmiş QVector<Item> üzerinde çalışır — her
// tuş vuruşunda veritabanına gitmez, arama arayüzde anında hissettirilsin
// diye bellek içi bir işlemdir.
QVector<Item> itemAra(const QVector<Item> &katalog, const QString &aranan);
