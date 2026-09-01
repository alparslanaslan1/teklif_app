#pragma once

#include "teklif/core/models.h"

#include <QHash>
#include <QString>
#include <QVector>

// CSV satırının ayrıştırılmış hali — henüz veritabanına dokunmaz, kategori
// adıyla taşınır (id ile değil), çünkü CSV insan tarafından Excel'de
// düzenlenecek bir dosyadır.
struct CsvItemRow
{
    QString kod;
    QString ad;
    QString birim;
    Money fiyat;
    QString kategoriAdi; // boş = kategorisiz
};

// "kod,ad,birim,fiyat,kategori" başlıklı CSV metnini ayrıştırır. Tırnaklı
// alanlar (virgül/tırnak/satır içi yeni satır içerenler) RFC4180'e uygun
// çözülür. Herhangi bir satır 5 sütundan farklıysa, gerekli alanlardan biri
// boşsa ya da fiyat ayrıştırılamıyorsa TÜM ayrıştırma başarısız sayılır ve
// errorOut'a hangi satırda ne olduğu yazılır — yarım bir içe aktarmadan
// iyidir. Başlık satırı atlanır (varlığı doğrulanmaz).
QVector<CsvItemRow> csvSatirlariniAyristir(const QString &icerik, QString *errorOut);

// Kalemleri CSV metnine döker. kategoriAdlari, categoryId -> ad eşlemesidir;
// categoryId 0 olan (kategorisiz) kalemler için kategori sütunu boş kalır.
QString csvOlustur(const QVector<Item> &kalemler, const QHash<qint64, QString> &kategoriAdlari);
