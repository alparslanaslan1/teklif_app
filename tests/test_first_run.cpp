#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSqlDatabase>
#include <QTemporaryDir>

#include "core/db.h"
#include "core/repo_items.h"
#include "core/settings.h"
#include "ui/dlg_first_run.h"

// Part 9: ilk calistirma sihirbazi.

class TestFirstRun : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void shownOnCleanDatabase();
    void notShownAfterMarked();
    void skipStillMarksAsShown();
    void acceptStoresCompanyInfo();
    void acceptLoadsSampleCatalog();
    void sampleCatalogCanBeDeclined();
    void sampleCatalogIsNotDuplicated();
    void sampleCatalogHasUsableItems();

private:
    QTemporaryDir *m_dir = nullptr;
    QString m_conn;
    QSqlDatabase m_db;
};

void TestFirstRun::init()
{
    m_dir = new QTemporaryDir();
    QVERIFY(m_dir->isValid());
    m_conn = QStringLiteral("fr_%1").arg(QDateTime::currentMSecsSinceEpoch());
    QString err;
    QVERIFY2(Db::openAndMigrate(m_dir->filePath(QStringLiteral("t.db")), &err, m_conn), qPrintable(err));
    m_db = QSqlDatabase::database(m_conn);
}

void TestFirstRun::cleanup()
{
    m_db = QSqlDatabase();
    QSqlDatabase::database(m_conn).close();
    QSqlDatabase::removeDatabase(m_conn);
    delete m_dir;
    m_dir = nullptr;
}

void TestFirstRun::shownOnCleanDatabase()
{
    Settings s(m_db);
    QVERIFY2(FirstRunDialog::shouldShow(s), "temiz veritabaninda sihirbaz gosterilmiyor");
}

void TestFirstRun::notShownAfterMarked()
{
    Settings s(m_db);
    QVERIFY(FirstRunDialog::markShown(s));
    QVERIFY2(!FirstRunDialog::shouldShow(s), "isaretlendikten sonra hala gosteriliyor");
}

void TestFirstRun::skipStillMarksAsShown()
{
    // Kullanici "Simdilik atla" derse her acilista ayni pencereyle
    // karsilasmamali. Firma unvaninin dolulugana bakmak yetmezdi: bos
    // birakip gecen kullanici sonsuza dek sihirbaz gorurdu.
    Settings s(m_db);
    FirstRunDialog dlg(m_db);
    auto *box = dlg.findChild<QDialogButtonBox *>(QStringLiteral("ilkButtonBox"));
    QVERIFY(box);

    QAbstractButton *atla = nullptr;
    for (QAbstractButton *b : box->buttons())
        if (box->buttonRole(b) == QDialogButtonBox::RejectRole)
            atla = b;
    QVERIFY(atla);
    atla->click();

    QVERIFY(s.valueOr(Settings::keyCompanyName()).isEmpty()); // bilgi yazilmadi
    QVERIFY2(!FirstRunDialog::shouldShow(s), "atlandiktan sonra tekrar gosteriliyor");
}

void TestFirstRun::acceptStoresCompanyInfo()
{
    FirstRunDialog dlg(m_db);
    dlg.findChild<QLineEdit *>(QStringLiteral("ilkUnvanEdit"))
        ->setText(QStringLiteral("Karasu Vizyon Doğalgaz"));
    dlg.findChild<QLineEdit *>(QStringLiteral("ilkTelefonEdit"))
        ->setText(QStringLiteral("0264 000 00 00"));
    dlg.findChild<QCheckBox *>(QStringLiteral("ilkOrnekKatalogCheck"))->setChecked(false);

    auto *box = dlg.findChild<QDialogButtonBox *>(QStringLiteral("ilkButtonBox"));
    for (QAbstractButton *b : box->buttons())
        if (box->buttonRole(b) == QDialogButtonBox::AcceptRole)
            b->click();

    Settings s(m_db);
    QCOMPARE(s.valueOr(Settings::keyCompanyName()), QStringLiteral("Karasu Vizyon Doğalgaz"));
    QCOMPARE(s.valueOr(Settings::keyCompanyPhone()), QStringLiteral("0264 000 00 00"));
    QVERIFY(!FirstRunDialog::shouldShow(s));
}

void TestFirstRun::acceptLoadsSampleCatalog()
{
    QCOMPARE(RepoItems(m_db).listAll(true).size(), 0);

    FirstRunDialog dlg(m_db);
    dlg.findChild<QLineEdit *>(QStringLiteral("ilkUnvanEdit"))->setText(QStringLiteral("Firma"));
    QVERIFY(dlg.findChild<QCheckBox *>(QStringLiteral("ilkOrnekKatalogCheck"))->isChecked());

    auto *box = dlg.findChild<QDialogButtonBox *>(QStringLiteral("ilkButtonBox"));
    for (QAbstractButton *b : box->buttons())
        if (box->buttonRole(b) == QDialogButtonBox::AcceptRole)
            b->click();

    QVERIFY2(RepoItems(m_db).listAll(true).size() > 0, "ornek katalog eklenmedi");
}

void TestFirstRun::sampleCatalogCanBeDeclined()
{
    FirstRunDialog dlg(m_db);
    dlg.findChild<QLineEdit *>(QStringLiteral("ilkUnvanEdit"))->setText(QStringLiteral("Firma"));
    dlg.findChild<QCheckBox *>(QStringLiteral("ilkOrnekKatalogCheck"))->setChecked(false);

    auto *box = dlg.findChild<QDialogButtonBox *>(QStringLiteral("ilkButtonBox"));
    for (QAbstractButton *b : box->buttons())
        if (box->buttonRole(b) == QDialogButtonBox::AcceptRole)
            b->click();

    QCOMPARE(RepoItems(m_db).listAll(true).size(), 0);
}

void TestFirstRun::sampleCatalogIsNotDuplicated()
{
    // Katalogda kalem varsa ornekler EKLENMEZ: sihirbaz ikinci kez
    // calistirilsa da (ya da kullanici kendi kalemlerini once girdiyse)
    // katalog ikizlenmemeli.
    RepoItems repo(m_db);
    Item it;
    it.kod = QStringLiteral("KENDI-1");
    it.ad = QStringLiteral("Kendi Kalemim");
    it.birim = QStringLiteral("adet");
    QString err;
    QVERIFY(repo.add(it, &err));

    QVERIFY2(FirstRunDialog::loadSampleCatalog(m_db, &err), qPrintable(err));
    QCOMPARE(repo.listAll(true).size(), 1); // sadece kendi kalemi

    // Iki kez cagirmak da guvenli olmali.
    QVERIFY(FirstRunDialog::loadSampleCatalog(m_db, &err));
    QCOMPARE(repo.listAll(true).size(), 1);
}

void TestFirstRun::sampleCatalogHasUsableItems()
{
    QString err;
    QVERIFY2(FirstRunDialog::loadSampleCatalog(m_db, &err), qPrintable(err));

    RepoItems repo(m_db);
    const QVector<Item> kalemler = repo.listAll();
    QVERIFY(kalemler.size() >= 5);

    // Her kalem gercekten kullanilabilir olmali: zorunlu alanlar dolu,
    // fiyat negatif degil, kategori atanmis.
    for (const Item &it : kalemler) {
        QVERIFY2(!it.kod.isEmpty(), qPrintable(it.ad));
        QVERIFY2(!it.ad.isEmpty(), qPrintable(it.kod));
        QVERIFY2(!it.birim.isEmpty(), qPrintable(it.kod));
        QVERIFY2(!it.varsayilanFiyat.isNegative(), qPrintable(it.kod));
        QVERIFY2(it.categoryId != 0, qPrintable(it.kod));
        QVERIFY(it.aktif);
    }

    // Kategoriler olusmus olmali.
    QVERIFY(repo.listCategories().size() >= 2);

    // Turkce arama ornek kalemleri bulmali.
    ItemFilter f;
    f.aranan = QStringLiteral("iscilik");
    QVERIFY2(!repo.list(f).isEmpty(), "ornek katalogda Turkce arama calismiyor");
}

QTEST_MAIN(TestFirstRun)
#include "test_first_run.moc"
