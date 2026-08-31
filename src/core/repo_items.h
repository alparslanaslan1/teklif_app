#pragma once

#include "models.h"

#include <QHash>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

class RepoItems
{
public:
    // Yeni kalem ekler. Başarılıysa item.id veritabanının verdiği id ile
    // doldurulur. kod alanı UNIQUE'tir; çakışma ham SQLite metni yerine
    // "Bu kod zaten kayıtlı: <kod>" biçiminde anlaşılır bir hatayla döner.
    static bool add(QSqlDatabase &db, Item &item, QString *errorOut);

    // Var olan bir kalemi id'sine göre tamamen günceller.
    static bool update(QSqlDatabase &db, const Item &item, QString *errorOut);

    // Kalemi SİLMEZ, sadece aktif/pasif durumunu değiştirir. Programın
    // hiçbir yerinde gerçek DELETE çağrılmaz: geçmiş tekliflerdeki satırlar
    // fiyatı kopyaladığı için silme onları bozmaz, ama katalogdan
    // kaybolması kafa karıştırır (bkz. proje planı).
    static bool setActive(QSqlDatabase &db, qint64 id, bool aktif, QString *errorOut);

    // includeInactive false ise pasif kalemler listeye girmez. Ada göre
    // TÜRKÇE alfabetik sıralı döner (SQL'in BINARY collation'ı Türkçede
    // yanlış sıralar — bkz. turkish.h).
    // Sorgu başarısız olursa boş liste döner ve errorOut doldurulur; "katalog
    // boş" ile "sorgu patladı" birbirinden ayırt edilebilmelidir.
    static QVector<Item> listAll(QSqlDatabase &db, bool includeInactive = false,
                                  QString *errorOut = nullptr);

    // İçe aktarma ÖNCE tüm satırları doğrular (csv.h), SONRA tek bir
    // transaction içinde ekler; herhangi bir satır (bozuk CSV veya
    // çakışan kod) başarısız olursa TEK satır bile eklemeden geri alınır.
    // Kategori adı katalogda yoksa otomatik oluşturulur.
    static bool importCsv(QSqlDatabase &db, const QString &csvContent, QString *errorOut);

    // Aktif + pasif tüm kalemleri (kategori adlarıyla birlikte) CSV'ye döker.
    static QString exportCsv(QSqlDatabase &db, QString *errorOut);

private:
    // ad'a sahip kategoriyi bulur, yoksa oluşturur. Hata olursa -1 döner.
    static qint64 categoryIdForName(QSqlDatabase &db, const QString &ad, QString *errorOut);
    static QHash<qint64, QString> categoryNameMap(QSqlDatabase &db);
};
