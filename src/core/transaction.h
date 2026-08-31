#pragma once

#include <QSqlDatabase>
#include <QString>

// Veritabanı transaction'ı için RAII sarmalayıcı.
//
// NEDEN VAR: Elle yazılan BEGIN/COMMIT/ROLLBACK üçlüsünde iki hata sürekli
// tekrarlanıyordu — (1) BEGIN'in başarılı olup olmadığına bakılmaması,
// (2) bir hata dalında ROLLBACK'in unutulması. Bu sınıfta ikisi de yapısal
// olarak imkânsız: BEGIN başarısızsa isActive() false döner, commit()
// çağrılmadan kapsam dışına çıkılırsa yıkıcı ROLLBACK eder.
//
// KULLANIM:
//   Transaction tx(db);
//   if (!tx.isActive()) { *errorOut = tx.lastError(); return false; }
//   ... işler ...                       // her erken return güvenli: ROLLBACK olur
//   return tx.commit(errorOut);
//
// İÇ İÇE KULLANILAMAZ: SQLite iç içe transaction desteklemez. Zaten açık bir
// transaction varken kurulursa BEGIN başarısız olur ve isActive() false
// döner — sessizce dıştakine katılıp onu ROLLBACK etme riski böylece ortadan
// kalkar.
class Transaction
{
public:
    // BEGIN IMMEDIATE çalıştırır. Yazma kilidi hemen alınır; iki eşzamanlı
    // yazma denemesinde biri en baştan hata alır, işin ortasında değil.
    explicit Transaction(QSqlDatabase db);

    // commit() çağrılmadıysa ROLLBACK eder.
    ~Transaction();

    Transaction(const Transaction &) = delete;
    Transaction &operator=(const Transaction &) = delete;

    // BEGIN başarılı oldu ve transaction hâlâ açık mı.
    bool isActive() const { return m_active; }

    // COMMIT eder. Başarılıysa yıkıcı artık ROLLBACK yapmaz.
    bool commit(QString *errorOut = nullptr);

    // Açık transaction'ı elle geri alır. Genelde gerekmez — yıkıcı zaten yapar.
    void rollback();

    // Son hata metni (BEGIN, COMMIT veya ROLLBACK'ten).
    QString lastError() const { return m_error; }

private:
    QSqlDatabase m_db;
    bool m_active = false;
    QString m_error;
};
