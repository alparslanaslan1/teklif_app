#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QSqlDatabase>

#include "teklif/core/db.h"
#include "teklif/core/repo_customers.h"

namespace {

void closeAndRemove(const QString &connectionName)
{
    QSqlDatabase::database(connectionName).close();
    QSqlDatabase::removeDatabase(connectionName);
}

} // namespace

class TestRepoCustomers : public QObject
{
    Q_OBJECT

private slots:
    void addAssignsId();
    void listOrderedByUnvan();
    void inactiveHiddenByDefault();
};

void TestRepoCustomers::addAssignsId()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = QStringLiteral("cust_add");
    QString err;
    QVERIFY2(Db::openAndMigrate(dir.filePath(QStringLiteral("t.db")), &err, conn), qPrintable(err));

    {
        QSqlDatabase db = QSqlDatabase::database(conn);
        Customer c;
        c.unvan = QStringLiteral("Ahmet Yılmaz");
        c.telefon = QStringLiteral("0532 000 00 00");
        QString addErr;
        QVERIFY2(RepoCustomers(db).add(c, &addErr), qPrintable(addErr));
        QVERIFY(c.id > 0);
    }
    closeAndRemove(conn);
}

void TestRepoCustomers::listOrderedByUnvan()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = QStringLiteral("cust_order");
    QString err;
    QVERIFY2(Db::openAndMigrate(dir.filePath(QStringLiteral("t.db")), &err, conn), qPrintable(err));

    {
        QSqlDatabase db = QSqlDatabase::database(conn);
        for (const QString &unvan :
             {QStringLiteral("Yıldız İnşaat"), QStringLiteral("Ahmet Yılmaz"), QStringLiteral("Deniz Yapı")}) {
            Customer c;
            c.unvan = unvan;
            QString e;
            QVERIFY2(RepoCustomers(db).add(c, &e), qPrintable(e));
        }

        const QVector<Customer> liste = RepoCustomers(db).listAll();
        QCOMPARE(liste.size(), 3);
        QCOMPARE(liste[0].unvan, QStringLiteral("Ahmet Yılmaz"));
        QCOMPARE(liste[1].unvan, QStringLiteral("Deniz Yapı"));
        QCOMPARE(liste[2].unvan, QStringLiteral("Yıldız İnşaat"));
    }
    closeAndRemove(conn);
}

void TestRepoCustomers::inactiveHiddenByDefault()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString conn = QStringLiteral("cust_inactive");
    QString err;
    QVERIFY2(Db::openAndMigrate(dir.filePath(QStringLiteral("t.db")), &err, conn), qPrintable(err));

    {
        QSqlDatabase db = QSqlDatabase::database(conn);
        Customer c;
        c.unvan = QStringLiteral("Pasif Müşteri");
        c.aktif = false;
        QString e;
        QVERIFY2(RepoCustomers(db).add(c, &e), qPrintable(e));

        QCOMPARE(RepoCustomers(db).listAll(/*includeInactive=*/false).size(), 0);
        QCOMPARE(RepoCustomers(db).listAll(/*includeInactive=*/true).size(), 1);
    }
    closeAndRemove(conn);
}

QTEST_MAIN(TestRepoCustomers)
#include "test_repo_customers.moc"
