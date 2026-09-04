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

    // Transaction davranisini olcmek icin en basit tablo yeter; categories
    // tek sutunlu ve baska hicbir seye bagli degil, bu yuzden testin konusu
    // (commit/rollback) disinda hicbir sey karismaz.
    int rowCount()
    {
        QSqlQuery q(m_db);
        return (q.exec(QStringLiteral("SELECT COUNT(*) FROM categories")) && q.next())
                   ? q.value(0).toInt() : -1;
    }
    bool insertRow(const QString &ad)
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral("INSERT INTO categories (ad) VALUES (:u)"));
        q.bindValue(QStringLiteral(":u"), ad);
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
        QVERIFY(insertRow(QStringLiteral("Kalıcı")));
        QString err;
        QVERIFY2(tx.commit(&err), qPrintable(err));
    }
    QCOMPARE(rowCount(), 1);
}

void TestTransaction::destructorRollsBackWhenNotCommitted()
{
    {
        Transaction tx(m_db);
        QVERIFY(tx.isActive());
        QVERIFY(insertRow(QStringLiteral("Geri alinacak")));
        // commit() YOK — kapsamdan cikinca yikici ROLLBACK etmeli.
    }
    QCOMPARE(rowCount(), 0);
}

void TestTransaction::earlyReturnRollsBack()
{
    // Transaction'in asil varlik sebebi: hata dalinda ROLLBACK unutulamaz.
    auto islem = [this]() -> bool {
        Transaction tx(m_db);
        if (!tx.isActive())
            return false;
        if (!insertRow(QStringLiteral("Birinci")))
            return false;
        return false; // "bir sey ters gitti" — commit edilmeden cikiliyor
    };

    QVERIFY(!islem());
    QCOMPARE(rowCount(), 0);
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
    QVERIFY(insertRow(QStringLiteral("Dis islem")));
    QString err;
    QVERIFY2(dis.commit(&err), qPrintable(err));
    QCOMPARE(rowCount(), 1);
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
        QVERIFY(insertRow(QStringLiteral("Elle geri alinan")));
        tx.rollback();
    }
    QCOMPARE(rowCount(), 0);
}

QTEST_MAIN(TestTransaction)
#include "test_transaction.moc"
