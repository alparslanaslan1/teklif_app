#include <QtTest/QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "teklif/core/db.h"
#include "teklif/core/transaction.h"

class TestTransaction : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void commitPersists();
    void destructorRollsBackWhenNotCommitted();
    void earlyReturnRollsBack();
    void nestedTransactionIsRejected();
    void commitAfterRollbackFails();
    void explicitRollbackWorks();

private:
    QTemporaryDir *m_dir = nullptr;
    QString m_conn;
    QSqlDatabase m_db;

    int customerCount()
    {
        QSqlQuery q(m_db);
        return (q.exec(QStringLiteral("SELECT COUNT(*) FROM customers")) && q.next())
                   ? q.value(0).toInt() : -1;
    }
    bool insertCustomer(const QString &unvan)
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral("INSERT INTO customers (unvan) VALUES (:u)"));
        q.bindValue(QStringLiteral(":u"), unvan);
        return q.exec();
    }
};

void TestTransaction::init()
{
    m_dir = new QTemporaryDir();
    QVERIFY(m_dir->isValid());
    m_conn = QStringLiteral("tx_%1").arg(QDateTime::currentMSecsSinceEpoch());
    QString err;
    QVERIFY2(Db::openAndMigrate(m_dir->filePath(QStringLiteral("t.db")), &err, m_conn), qPrintable(err));
    m_db = QSqlDatabase::database(m_conn);
}

void TestTransaction::cleanup()
{
    m_db = QSqlDatabase();
    QSqlDatabase::database(m_conn).close();
    QSqlDatabase::removeDatabase(m_conn);
    delete m_dir;
    m_dir = nullptr;
}

void TestTransaction::commitPersists()
{
    {
        Transaction tx(m_db);
        QVERIFY2(tx.isActive(), qPrintable(tx.lastError()));
        QVERIFY(insertCustomer(QStringLiteral("Kalıcı")));
        QString err;
        QVERIFY2(tx.commit(&err), qPrintable(err));
    }
    QCOMPARE(customerCount(), 1);
}

void TestTransaction::destructorRollsBackWhenNotCommitted()
{
    {
        Transaction tx(m_db);
        QVERIFY(tx.isActive());
        QVERIFY(insertCustomer(QStringLiteral("Geri alinacak")));
        // commit() YOK — kapsamdan cikinca yikici ROLLBACK etmeli.
    }
    QCOMPARE(customerCount(), 0);
}

void TestTransaction::earlyReturnRollsBack()
{
    // Transaction'in asil varlik sebebi: hata dalinda ROLLBACK unutulamaz.
    auto islem = [this]() -> bool {
        Transaction tx(m_db);
        if (!tx.isActive())
            return false;
        if (!insertCustomer(QStringLiteral("Birinci")))
            return false;
        return false; // "bir sey ters gitti" — commit edilmeden cikiliyor
    };

    QVERIFY(!islem());
    QCOMPARE(customerCount(), 0);
}

void TestTransaction::nestedTransactionIsRejected()
{
    Transaction dis(m_db);
    QVERIFY(dis.isActive());

    // SQLite ic ice transaction desteklemez. Ic transaction sessizce
    // distakine katilip onu ROLLBACK etmemeli; isActive() false donmeli.
    Transaction ic(m_db);
    QVERIFY(!ic.isActive());
    QVERIFY(!ic.lastError().isEmpty());

    // Ictekinin yikicisi distakini bozmamali: dis hala islevsel olmali.
    QVERIFY(insertCustomer(QStringLiteral("Dis islem")));
    QString err;
    QVERIFY2(dis.commit(&err), qPrintable(err));
    QCOMPARE(customerCount(), 1);
}

void TestTransaction::commitAfterRollbackFails()
{
    Transaction tx(m_db);
    QVERIFY(tx.isActive());
    tx.rollback();
    QVERIFY(!tx.isActive());

    QString err;
    QVERIFY(!tx.commit(&err));
    QVERIFY(!err.isEmpty());
}

void TestTransaction::explicitRollbackWorks()
{
    {
        Transaction tx(m_db);
        QVERIFY(insertCustomer(QStringLiteral("Elle geri alinan")));
        tx.rollback();
    }
    QCOMPARE(customerCount(), 0);
}

QTEST_MAIN(TestTransaction)
#include "test_transaction.moc"
