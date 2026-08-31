#pragma once

#include "models.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>

// customers tablosunun deposu.
//
// Bağlantıyı nesne içinde tutar (bkz. RepoItems üzerindeki not).
// Tam müşteri yönetimi (güncelleme, pasife alma, tekil okuma, arama) Part 6'da
// eklenecek — orada bu sınıf genişler, yeniden yazılmaz.
class RepoCustomers
{
public:
    explicit RepoCustomers(QSqlDatabase db);

    // Yeni müşteri ekler. Başarılıysa customer.id veritabanının verdiği id
    // ile doldurulur.
    bool add(Customer &customer, QString *errorOut = nullptr);

    // includeInactive false ise pasif müşteriler listeye girmez. Unvana göre
    // TÜRKÇE alfabetik sıralı döner. Sorgu başarısız olursa boş liste döner
    // ve errorOut doldurulur.
    QVector<Customer> listAll(bool includeInactive = false, QString *errorOut = nullptr) const;

private:
    QSqlDatabase m_db;
};
