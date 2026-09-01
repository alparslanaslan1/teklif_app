#pragma once

#include <QString>
#include <QUrl>
#include <optional>

// Sunucudaki latest.json'un bellek içi karşılığı.
//
// Dosya biçimi (GitHub Releases'e ya da herhangi bir statik sunucuya konur):
//   {
//     "version":    "0.2.0",
//     "minVersion": "0.1.0",
//     "url":        "https://.../teklif-0.2.0-win64.zip",
//     "sha256":     "a1b2c3...",
//     "notes":      "Teklif listesi ekranı eklendi"
//   }
struct UpdateInfo
{
    QString version;    // yayınlanan sürüm
    QString minVersion; // bundan eski sürümler güncellemeden devam EDEMEZ (boş = zorunlu değil)
    QUrl url;           // güncelleme paketinin (zip) adresi
    QString sha256;     // paketin beklenen özeti; indirme sonrası doğrulanır
    QString notes;      // kullanıcıya gösterilecek kısa değişiklik notu
};

// İki sürüm numarasını sayısal olarak karşılaştırır.
// Döner:  a < b ise -1,  a == b ise 0,  a > b ise 1.
//
// Nokta ile ayrılmış parçalar SAYI olarak karşılaştırılır, metin olarak değil —
// aksi halde "0.10.0" < "0.9.0" gibi yanlış bir sonuç çıkardı. Parça sayıları
// farklıysa eksik olanlar 0 sayılır ("1.2" == "1.2.0").
// Sayıya çevrilemeyen parça 0 kabul edilir; böylece bozuk bir sürüm metni
// karşılaştırmayı çökertmez.
int compareVersions(const QString &a, const QString &b);

// latest.json içeriğini ayrıştırır. Zorunlu alanlar: version, url.
// Eksik/bozuk bir alanda std::nullopt döner ve errorOut'a sebep yazılır —
// bozuk bir manifest yüzünden program asla çökmemeli, sadece güncelleme atlanmalı.
std::optional<UpdateInfo> parseUpdateManifest(const QByteArray &json, QString *errorOut);
