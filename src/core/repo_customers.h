#pragma once

#include "models.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>

// Part 4'te sadece teklif ekranındaki müşteri seçim listesini doldurmak
// için gereken minimum işlevsellik. Tam müşteri yönetimi (güncelleme,
// arşiv bağlantısı, arama) Part 6'da eklenecek — orada bu sınıf genişler,
// yeniden yazılmaz.
class RepoCustomers
{
public:
    static bool add(QSqlDatabase &db, Customer &customer, QString *errorOut);

    // includeInactive false ise pasif müşteriler listeye girmez. Unvana göre
    // TÜRKÇE alfabetik sıralı döner. Sorgu başarısız olursa boş liste döner
    // ve errorOut doldurulur.
    static QVector<Customer> listAll(QSqlDatabase &db, bool includeInactive = false,
                                      QString *errorOut = nullptr);
};
