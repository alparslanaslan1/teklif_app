#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlDatabase>
#include <QTableView>
#include <QTemporaryDir>

#include "teklif/core/db.h"
#include "teklif/core/repo_items.h"
#include "teklif/ui/item_table_model.h"
#include "teklif/ui/mainwindow.h"
#include "teklif/ui/page_catalog.h"
#include "teklif/ui/page_quote.h"

// Part 3'un arayuz tarafi: katalog ekrani.

class TestPageCatalog : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void listsActiveItemsOnly();
    void showInactiveRevealsThem();
    void searchFiltersTable();
    void categoryFilterNarrowsList();
    void addNewItemThroughForm();
    void newCategoryTypedInFormIsCreated();
    void editExistingItemKeepsActiveFlag();
    void duplicateCodeIsRejected();
    void invalidPriceIsRejected();
    void deactivateKeepsItemAndShowsIt();
    void catalogChangeReachesQuotePage();

private:
    QTemporaryDir *m_dir = nullptr;
    QString m_conn;
    QSqlDatabase m_db;

    qint64 addItem(const QString &kod, const QString &ad, qint64 fiyat = 10000,
                    const QString &kategori = QString())
    {
        RepoItems repo(m_db);
        Item it;
        it.kod = kod; it.ad = ad; it.birim = QStringLiteral("adet");
        it.varsayilanFiyat = Money(fiyat);
        if (!kategori.isEmpty())
            it.categoryId = repo.ensureCategory(kategori);
        QString e;
        repo.add(it, &e);
        return it.id;
    }
};

void TestPageCatalog::init()
{
    m_dir = new QTemporaryDir();
    QVERIFY(m_dir->isValid());
    m_conn = QStringLiteral("pc_%1").arg(QDateTime::currentMSecsSinceEpoch());
    QString err;
    QVERIFY2(Db::openAndMigrate(m_dir->filePath(QStringLiteral("t.db")), &err, m_conn), qPrintable(err));
    m_db = QSqlDatabase::database(m_conn);
}

void TestPageCatalog::cleanup()
{
    m_db = QSqlDatabase();
    QSqlDatabase::database(m_conn).close();
    QSqlDatabase::removeDatabase(m_conn);
    delete m_dir;
    m_dir = nullptr;
}

void TestPageCatalog::listsActiveItemsOnly()
{
    addItem(QStringLiteral("K-1"), QStringLiteral("Alçıpan"));
    const qint64 pasif = addItem(QStringLiteral("K-2"), QStringLiteral("Boya"));
    QVERIFY(RepoItems(m_db).setActive(pasif, false));

    PageCatalog page(m_db);
    QCOMPARE(page.model()->rowCount(), 1);
    QCOMPARE(page.model()->at(0).ad, QStringLiteral("Alçıpan"));
}

void TestPageCatalog::showInactiveRevealsThem()
{
    addItem(QStringLiteral("K-1"), QStringLiteral("Alçıpan"));
    const qint64 pasif = addItem(QStringLiteral("K-2"), QStringLiteral("Boya"));
    QVERIFY(RepoItems(m_db).setActive(pasif, false));

    PageCatalog page(m_db);
    auto *check = page.findChild<QCheckBox *>(QStringLiteral("katalogPasifCheck"));
    QVERIFY(check);
    check->setChecked(true);
    QCOMPARE(page.model()->rowCount(), 2);
}

void TestPageCatalog::searchFiltersTable()
{
    addItem(QStringLiteral("K-1"), QStringLiteral("İşçilik"));
    addItem(QStringLiteral("K-2"), QStringLiteral("Alçıpan"));

    PageCatalog page(m_db);
    auto *arama = page.findChild<QLineEdit *>(QStringLiteral("katalogAramaEdit"));
    QVERIFY(arama);

    // Turkce klavyesi olmayan kullanici ASCII yazarak bulabilmeli.
    arama->setText(QStringLiteral("iscilik"));
    QCOMPARE(page.model()->rowCount(), 1);
    QCOMPARE(page.model()->at(0).ad, QStringLiteral("İşçilik"));

    arama->setText(QStringLiteral("K-2"));  // kod uzerinden de aranir
    QCOMPARE(page.model()->rowCount(), 1);

    arama->clear();
    QCOMPARE(page.model()->rowCount(), 2);
}

void TestPageCatalog::categoryFilterNarrowsList()
{
    addItem(QStringLiteral("K-1"), QStringLiteral("Alçıpan"), 10000, QStringLiteral("Kaba"));
    addItem(QStringLiteral("K-2"), QStringLiteral("Boya"), 10000, QStringLiteral("İnce"));
    addItem(QStringLiteral("K-3"), QStringLiteral("Şap"), 10000, QStringLiteral("Kaba"));

    PageCatalog page(m_db);
    QCOMPARE(page.model()->rowCount(), 3);

    auto *filtre = page.findChild<QComboBox *>(QStringLiteral("katalogKategoriFiltre"));
    QVERIFY(filtre);
    const int idx = filtre->findText(QStringLiteral("Kaba"));
    QVERIFY(idx > 0);
    filtre->setCurrentIndex(idx);

    QCOMPARE(page.model()->rowCount(), 2);
}

void TestPageCatalog::addNewItemThroughForm()
{
    PageCatalog page(m_db);

    auto *kod = page.findChild<QLineEdit *>(QStringLiteral("katalogKodEdit"));
    auto *ad = page.findChild<QLineEdit *>(QStringLiteral("katalogAdEdit"));
    auto *birim = page.findChild<QLineEdit *>(QStringLiteral("katalogBirimEdit"));
    auto *fiyat = page.findChild<QLineEdit *>(QStringLiteral("katalogFiyatEdit"));
    auto *kaydet = page.findChild<QPushButton *>(QStringLiteral("katalogKaydetButton"));
    QVERIFY(kod && ad && birim && fiyat && kaydet);

    kod->setText(QStringLiteral("YENI-1"));
    ad->setText(QStringLiteral("Şap Dökümü"));
    birim->setText(QStringLiteral("m2"));
    fiyat->setText(QStringLiteral("1.250,50"));
    kaydet->click();

    QCOMPARE(page.model()->rowCount(), 1);
    const auto kayit = RepoItems(m_db).get(page.selectedItemId());
    QVERIFY(kayit.has_value());
    QCOMPARE(kayit->ad, QStringLiteral("Şap Dökümü"));
    QCOMPARE(kayit->varsayilanFiyat.toString(), QStringLiteral("1.250,50"));
    QVERIFY(kayit->aktif);
}

void TestPageCatalog::newCategoryTypedInFormIsCreated()
{
    // Kategori kutusu duzenlenebilir: listede olmayan bir ad yazilirsa
    // kaydederken otomatik olusturulmali, ayri bir kategori ekrani gerekmeden.
    PageCatalog page(m_db);

    page.findChild<QLineEdit *>(QStringLiteral("katalogKodEdit"))->setText(QStringLiteral("K-9"));
    page.findChild<QLineEdit *>(QStringLiteral("katalogAdEdit"))->setText(QStringLiteral("Kalem"));
    page.findChild<QLineEdit *>(QStringLiteral("katalogBirimEdit"))->setText(QStringLiteral("adet"));
    page.findChild<QLineEdit *>(QStringLiteral("katalogFiyatEdit"))->setText(QStringLiteral("10,00"));
    page.findChild<QComboBox *>(QStringLiteral("katalogKategoriCombo"))
        ->setCurrentText(QStringLiteral("Yepyeni Kategori"));
    page.findChild<QPushButton *>(QStringLiteral("katalogKaydetButton"))->click();

    const auto kategoriler = RepoItems(m_db).listCategories();
    bool bulundu = false;
    for (const Category &c : kategoriler)
        if (c.ad == QStringLiteral("Yepyeni Kategori"))
            bulundu = true;
    QVERIFY2(bulundu, "formda yazilan kategori olusturulmadi");

    const auto kayit = RepoItems(m_db).get(page.selectedItemId());
    QVERIFY(kayit.has_value());
    QVERIFY(kayit->categoryId != 0);
}

void TestPageCatalog::editExistingItemKeepsActiveFlag()
{
    const qint64 id = addItem(QStringLiteral("K-1"), QStringLiteral("Eski Ad"));
    QVERIFY(RepoItems(m_db).setActive(id, false));

    PageCatalog page(m_db);
    page.findChild<QCheckBox *>(QStringLiteral("katalogPasifCheck"))->setChecked(true);
    page.selectItemById(id);
    QCOMPARE(page.selectedItemId(), id);

    page.findChild<QLineEdit *>(QStringLiteral("katalogAdEdit"))->setText(QStringLiteral("Yeni Ad"));
    page.findChild<QPushButton *>(QStringLiteral("katalogKaydetButton"))->click();

    const auto kayit = RepoItems(m_db).get(id);
    QVERIFY(kayit.has_value());
    QCOMPARE(kayit->ad, QStringLiteral("Yeni Ad"));
    // Aktiflik bayragi ayri dugmede; kaydetme onu sifirlamamali.
    QVERIFY2(!kayit->aktif, "kaydetme pasif kalemi aktife dondurdu");
}

void TestPageCatalog::duplicateCodeIsRejected()
{
    addItem(QStringLiteral("K-1"), QStringLiteral("Var Olan"));

    PageCatalog page(m_db);
    page.findChild<QPushButton *>(QStringLiteral("katalogYeniButton"))->click();
    page.findChild<QLineEdit *>(QStringLiteral("katalogKodEdit"))->setText(QStringLiteral("K-1"));
    page.findChild<QLineEdit *>(QStringLiteral("katalogAdEdit"))->setText(QStringLiteral("Cakisan"));
    page.findChild<QLineEdit *>(QStringLiteral("katalogBirimEdit"))->setText(QStringLiteral("adet"));

    // Kaydet bir uyari kutusu acar; testte kutuyu beklemeden dogrulamak icin
    // dogrudan depoya bakiyoruz: ikinci kalem eklenmemis olmali.
    QTimer::singleShot(0, [] {
        for (QWidget *w : QApplication::topLevelWidgets())
            if (auto *box = qobject_cast<QMessageBox *>(w))
                box->accept();
    });
    page.findChild<QPushButton *>(QStringLiteral("katalogKaydetButton"))->click();

    QCOMPARE(RepoItems(m_db).listAll(true).size(), 1);
}

void TestPageCatalog::invalidPriceIsRejected()
{
    PageCatalog page(m_db);
    page.findChild<QLineEdit *>(QStringLiteral("katalogKodEdit"))->setText(QStringLiteral("K-1"));
    page.findChild<QLineEdit *>(QStringLiteral("katalogAdEdit"))->setText(QStringLiteral("Ad"));
    page.findChild<QLineEdit *>(QStringLiteral("katalogBirimEdit"))->setText(QStringLiteral("adet"));
    page.findChild<QLineEdit *>(QStringLiteral("katalogFiyatEdit"))->setText(QStringLiteral("abc"));

    QTimer::singleShot(0, [] {
        for (QWidget *w : QApplication::topLevelWidgets())
            if (auto *box = qobject_cast<QMessageBox *>(w))
                box->accept();
    });
    page.findChild<QPushButton *>(QStringLiteral("katalogKaydetButton"))->click();

    QCOMPARE(RepoItems(m_db).listAll(true).size(), 0);
}

void TestPageCatalog::deactivateKeepsItemAndShowsIt()
{
    const qint64 id = addItem(QStringLiteral("K-1"), QStringLiteral("Kalem"));

    PageCatalog page(m_db);
    page.selectItemById(id);
    page.findChild<QPushButton *>(QStringLiteral("katalogPasifButton"))->click();

    // Kalem SILINMEZ, pasif olur.
    const auto kayit = RepoItems(m_db).get(id);
    QVERIFY(kayit.has_value());
    QVERIFY(!kayit->aktif);

    // Pasife alinan kalem varsayilan filtrede kaybolurdu; kullanici ne
    // oldugunu gorebilsin diye pasifler otomatik gosterilir.
    auto *check = page.findChild<QCheckBox *>(QStringLiteral("katalogPasifCheck"));
    QVERIFY2(check->isChecked(), "pasife alinca kalem gozden kayboldu");
    QCOMPARE(page.model()->rowCount(), 1);
}

void TestPageCatalog::catalogChangeReachesQuotePage()
{
    // Katalogda eklenen kalem, teklif ekranindaki arama kutusunda hemen
    // bulunabilmeli: MainWindow catalogChanged sinyalini bagliyor.
    MainWindow w(m_db);

    w.catalogPage()->findChild<QLineEdit *>(QStringLiteral("katalogKodEdit"))
        ->setText(QStringLiteral("YENI"));
    w.catalogPage()->findChild<QLineEdit *>(QStringLiteral("katalogAdEdit"))
        ->setText(QStringLiteral("Sonradan Eklenen"));
    w.catalogPage()->findChild<QLineEdit *>(QStringLiteral("katalogBirimEdit"))
        ->setText(QStringLiteral("adet"));
    w.catalogPage()->findChild<QLineEdit *>(QStringLiteral("katalogFiyatEdit"))
        ->setText(QStringLiteral("100,00"));
    w.catalogPage()->findChild<QPushButton *>(QStringLiteral("katalogKaydetButton"))->click();

    // Teklif ekranindaki arama kutusuna yaz: yeni kalem cikmali.
    auto *edit = w.quotePage()->findChild<QLineEdit *>(QStringLiteral("itemSearchEdit"));
    QVERIFY(edit);
    QTest::keyClicks(edit, QStringLiteral("Sonradan"));

    auto *popup = w.quotePage()->findChild<QListWidget *>(QStringLiteral("itemSearchPopup"));
    QVERIFY(popup);
    QVERIFY2(popup->count() > 0, "katalogda eklenen kalem teklif aramasinda cikmadi");
}

QTEST_MAIN(TestPageCatalog)
#include "test_page_catalog.moc"
