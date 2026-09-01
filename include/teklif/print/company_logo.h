#pragma once

#include "teklif/core/settings.h"

#include <QImage>
#include <QString>

// Firma logosunun saklanması ve okunması.
//
// NEDEN core/ DEĞİL print/: logo bir QImage'dır ve core bilinçli olarak
// QtGui'ye bağımlı değil (bkz. CMakeLists — teklif_core yalnızca Core ve Sql
// ile bağlanır). Logo yalnızca belge çiziminde ve ayarlar ekranında
// kullanıldığı için ikisinin de eriştiği print katmanında yaşıyor.
//
// NEDEN AYRI TABLO DEĞİL settings: logo tek bir kayıt; kendi tablosunu
// hak etmiyor ve settings zaten yedekle birlikte taşınıyor. Şema
// değişmediği için migration da gerekmiyor.
namespace CompanyLogo {

// Saklanan logonun en büyük kenarı. Kullanıcı 4000 piksellik bir fotoğraf
// seçerse veritabanı şişer ve her açılışta gereksiz veri okunur; belge
// üzerinde zaten en fazla birkaç santimetre yer kaplayacağı için bu çözünürlük
// fazlasıyla yeter.
constexpr int kMaxBoyut = 600;

// Ayarlardaki logoyu okur. Logo yoksa ya da veri bozuksa null QImage döner —
// "logo yok" hata değil, olağan durumdur.
QImage load(const Settings &settings);

// Logoyu kMaxBoyut'a sığacak şekilde ölçekler, PNG olarak kodlar ve
// ayarlara yazar. Şeffaflık korunur (PNG), böylece antette beyaz bir kutu
// oluşmaz.
bool save(Settings &settings, const QImage &logo, QString *errorOut = nullptr);

// Logoyu kaldırır. Antet kendiliğinden logosuz düzene döner.
bool clear(Settings &settings, QString *errorOut = nullptr);

} // namespace CompanyLogo
