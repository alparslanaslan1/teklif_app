#pragma once

#include "models.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <optional>

// customers tablosunun deposu.
//
// Bağlantıyı nesne içinde tutar (bkz. RepoItems üzerindeki not).
class RepoCustomers
{
public:
    explicit RepoCustomers(QSqlDatabase db);

    // Yeni müşteri ekler. Başarılıysa customer.id veritabanının verdiği id
    // ile doldurulur.
    bool add(Customer &customer, QString *errorOut = nullptr);

    // Var olan bir müşteriyi id'sine göre tamamen günceller. id bulunamazsa
    // hata döner.
    bool update(const Customer &customer, QString *errorOut = nullptr);

    // Müşteriyi SİLMEZ, aktif/pasif yapar. Gerçek DELETE hiçbir yerde
    // çağrılmaz: quotes.customer_id üzerinde ON DELETE RESTRICT var, yani
    // teklifi olan bir müşteri zaten silinemez; silinebilenler için de
    // geçmişi bozmamak adına aynı kural uygulanır.
    bool setActive(qint64 id, bool aktif, QString *errorOut = nullptr);

    // Tek müşteri okur. Bulunamazsa std::nullopt.
    // Pasif müşteriler de döner — eski bir teklifin müşterisi pasife alınmış
    // olabilir ve belge antetinde yine de görünmesi gerekir.
    std::optional<Customer> get(qint64 id, QString *errorOut = nullptr) const;

    // includeInactive false ise pasif müşteriler listeye girmez. Unvana göre
    // TÜRKÇE alfabetik sıralı döner. Sorgu başarısız olursa boş liste döner
    // ve errorOut doldurulur.
    QVector<Customer> listAll(bool includeInactive = false, QString *errorOut = nullptr) const;

    // Unvan, yetkili, telefon ve vergi numarasında arar.
    // Eşleştirme SQL'de değil C++ tarafında yapılır: LIKE Türkçe harfleri
    // katlayamaz, "sukru" yazan kullanıcı "Şükrü"yü bulamazdı
    // (bkz. core/turkish.h). Aranan boşsa listAll ile aynı sonucu verir.
    QVector<Customer> search(const QString &aranan, bool includeInactive = false,
                              QString *errorOut = nullptr) const;

    // Müşteriye ait teklif sayısı. Pasife alma öncesi kullanıcıyı
    // bilgilendirmek için ("bu müşterinin 12 teklifi var").
    // Hata durumunda -1 döner.
    int quoteCount(qint64 customerId, QString *errorOut = nullptr) const;

private:
    QSqlDatabase m_db;
};
