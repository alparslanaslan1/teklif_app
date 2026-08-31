#pragma once

#include "models.h"

#include <QHash>
#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <optional>

// Katalog listesini daraltan ölçütler. QuoteFilter ile aynı kalıp: boş
// bırakılan her alan "bu ölçüte bakma" demektir.
struct ItemFilter
{
    bool includeInactive = false; // true ise pasif kalemler de listelenir
    qint64 categoryId = 0;        // 0 = tüm kategoriler
    QString aranan;               // Türkçe duyarlı arama (ad ve kod içinde)
};

// items (katalog) tablosunun deposu.
//
// Bağlantıyı nesne içinde tutar; her çağrıya QSqlDatabase geçirilmez.
// Böylece çağıran taraf (ekranlar) bir kez kurar, sonra sadece iş çağrılarını
// yapar — ve testler ile ileride farklı bir veri kaynağı, aynı arayüzü
// uygulayan başka bir sınıfla yerine konabilir.
//
// KOPYALANABİLİR: QSqlDatabase zaten bir tutamaç (handle); kopyalamak yeni
// bağlantı açmaz, aynı bağlantıya işaret eder.
class RepoItems
{
public:
    explicit RepoItems(QSqlDatabase db);

    // Yeni kalem ekler. Başarılıysa item.id veritabanının verdiği id ile
    // doldurulur. kod alanı UNIQUE'tir; çakışma ham SQLite metni yerine
    // "Bu kod zaten kayıtlı: <kod>" biçiminde anlaşılır bir hatayla döner.
    bool add(Item &item, QString *errorOut = nullptr);

    // Var olan bir kalemi id'sine göre tamamen günceller. id bulunamazsa
    // hata döner (sessizce "başarılı" saymaz).
    bool update(const Item &item, QString *errorOut = nullptr);

    // Kalemi SİLMEZ, sadece aktif/pasif durumunu değiştirir. Programın
    // hiçbir yerinde gerçek DELETE çağrılmaz: geçmiş tekliflerdeki satırlar
    // fiyatı kopyaladığı için silme onları bozmaz, ama katalogdan
    // kaybolması kafa karıştırır.
    bool setActive(qint64 id, bool aktif, QString *errorOut = nullptr);

    // Filtreye uyan kalemleri ada göre TÜRKÇE alfabetik sıralı döner
    // (SQL'in BINARY collation'ı Türkçede yanlış sıralar — bkz. turkish.h).
    // Sorgu başarısız olursa boş liste döner ve errorOut doldurulur.
    QVector<Item> list(const ItemFilter &filtre, QString *errorOut = nullptr) const;

    // list() için kısayol: yalnızca aktiflik ölçütüyle tüm katalog.
    // Teklif ekranı arama indeksini bununla besler.
    QVector<Item> listAll(bool includeInactive = false, QString *errorOut = nullptr) const;

    // Tek kalem okur. Bulunamazsa std::nullopt.
    // Pasif kalemler de döner — katalog ekranında pasif bir kalem seçilip
    // yeniden aktife alınabilmelidir.
    std::optional<Item> get(qint64 id, QString *errorOut = nullptr) const;

    // Kategorileri ada göre TÜRKÇE sıralı döner.
    QVector<Category> listCategories(QString *errorOut = nullptr) const;

    // ad'a sahip kategoriyi bulur, yoksa OLUŞTURUR.
    // Döner: >0 kategori id'si, 0 ad boş (kategorisiz), -1 hata.
    // Katalog ekranı da CSV içe aktarımı da aynı yolu kullanır ki kategori
    // eşleştirme kuralları (trim, büyük/küçük harf) tek yerde kalsın.
    qint64 ensureCategory(const QString &ad, QString *errorOut = nullptr);

    // İçe aktarma ÖNCE tüm satırları doğrular (csv.h), SONRA tek bir
    // transaction içinde ekler; herhangi bir satır (bozuk CSV veya
    // çakışan kod) başarısız olursa TEK satır bile eklemeden geri alınır.
    // Kategori adı katalogda yoksa otomatik oluşturulur.
    bool importCsv(const QString &csvContent, QString *errorOut = nullptr);

    // Aktif + pasif tüm kalemleri (kategori adlarıyla birlikte) CSV'ye döker.
    QString exportCsv(QString *errorOut = nullptr) const;

private:
    QSqlDatabase m_db;

    QHash<qint64, QString> categoryNameMap() const;
};
