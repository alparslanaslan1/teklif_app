#include <QtTest/QtTest>
#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QSignalSpy>

#include "teklif/ui/item_search.h"

namespace {

// QTest::keyClicks(widget, QString) Türkçe karakterlerde (ş, ı, ğ gibi
// Latin-1 dışı kod noktalarında) qasciikey.cpp içindeki ASCII-only tabloya
// takılıp assert ile çöküyor. Bunun yerine ham QKeyEvent'i doğrudan
// gönderiyoruz — QLineEdit metni key() koduna değil event->text()'e göre
// ekler, bu yüzden Qt::Key_unknown ile bile doğru çalışır.
void typeUnicodeText(QWidget *w, const QString &text)
{
    for (const QChar &ch : text) {
        QKeyEvent press(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString(ch));
        QApplication::sendEvent(w, &press);
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_unknown, Qt::NoModifier, QString(ch));
        QApplication::sendEvent(w, &release);
    }
}

Item mk(const QString &kod, const QString &ad, const QString &birim, qint64 fiyatKurus)
{
    Item it;
    it.kod = kod;
    it.ad = ad;
    it.birim = birim;
    it.varsayilanFiyat = Money(fiyatKurus);
    return it;
}

// "iş" ile arandığında "İşçilik" ve "İş İskelesi Kurulumu" baştan eşleşir
// (bu sırayla), "Alçıpan Levha" hiç eşleşmez.
QVector<Item> ornekKatalog()
{
    return {
        mk(QStringLiteral("ISC-01"), QStringLiteral("İşçilik"), QStringLiteral("saat"), 35000),
        mk(QStringLiteral("ISC-02"), QStringLiteral("İş İskelesi Kurulumu"), QStringLiteral("adet"), 12000),
        mk(QStringLiteral("ALC-01"), QStringLiteral("Alçıpan Levha"), QStringLiteral("adet"), 18000),
    };
}

} // namespace

class TestItemSearch : public QObject
{
    Q_OBJECT

private slots:
    void typingFiltersAndShowsPopup();
    void downArrowMovesSelectionAndClampsAtEnd();
    void upArrowClampsAtStart();
    void enterChoosesEmitsSignalAndClears();
    void escapeHidesPopupWithoutChoosing();
    void emptyResultHidesPopup();
};

void TestItemSearch::typingFiltersAndShowsPopup()
{
    ItemSearch w;
    w.setCatalog(ornekKatalog());
    w.show(); // popup->isVisible() ata zincirinin görünür olmasını gerektirir
    auto *edit = w.findChild<QLineEdit *>(QStringLiteral("itemSearchEdit"));
    auto *popup = w.findChild<QListWidget *>(QStringLiteral("itemSearchPopup"));
    QVERIFY(edit);
    QVERIFY(popup);
    QVERIFY(!popup->isVisible());

    typeUnicodeText(edit, QStringLiteral("iş"));

    QVERIFY(popup->isVisible());
    QCOMPARE(popup->count(), 2); // İşçilik + İş İskelesi Kurulumu; Alçıpan hariç
}

void TestItemSearch::downArrowMovesSelectionAndClampsAtEnd()
{
    ItemSearch w;
    w.setCatalog(ornekKatalog());
    w.show(); // popup->isVisible() ata zincirinin görünür olmasını gerektirir
    auto *edit = w.findChild<QLineEdit *>(QStringLiteral("itemSearchEdit"));
    auto *popup = w.findChild<QListWidget *>(QStringLiteral("itemSearchPopup"));

    typeUnicodeText(edit, QStringLiteral("iş"));
    QCOMPARE(popup->currentRow(), 0);

    QTest::keyClick(edit, Qt::Key_Down);
    QCOMPARE(popup->currentRow(), 1);

    QTest::keyClick(edit, Qt::Key_Down); // son satırda, taşma yok
    QCOMPARE(popup->currentRow(), 1);
}

void TestItemSearch::upArrowClampsAtStart()
{
    ItemSearch w;
    w.setCatalog(ornekKatalog());
    w.show(); // popup->isVisible() ata zincirinin görünür olmasını gerektirir
    auto *edit = w.findChild<QLineEdit *>(QStringLiteral("itemSearchEdit"));
    auto *popup = w.findChild<QListWidget *>(QStringLiteral("itemSearchPopup"));

    typeUnicodeText(edit, QStringLiteral("iş"));
    QTest::keyClick(edit, Qt::Key_Up); // ilk satırda, taşma yok
    QCOMPARE(popup->currentRow(), 0);
}

void TestItemSearch::enterChoosesEmitsSignalAndClears()
{
    ItemSearch w;
    w.setCatalog(ornekKatalog());
    w.show(); // popup->isVisible() ata zincirinin görünür olmasını gerektirir
    auto *edit = w.findChild<QLineEdit *>(QStringLiteral("itemSearchEdit"));
    auto *popup = w.findChild<QListWidget *>(QStringLiteral("itemSearchPopup"));
    QSignalSpy spy(&w, &ItemSearch::itemChosen);

    typeUnicodeText(edit, QStringLiteral("iş"));
    QTest::keyClick(edit, Qt::Key_Down); // İş İskelesi Kurulumu'nu seç
    QTest::keyClick(edit, Qt::Key_Return);

    QCOMPARE(spy.count(), 1);
    const Item secilen = qvariant_cast<Item>(spy.at(0).at(0));
    QCOMPARE(secilen.kod, QStringLiteral("ISC-02"));
    QVERIFY(edit->text().isEmpty()); // arama kutusu kendiliğinden temizlendi
    QVERIFY(!popup->isVisible());
}

void TestItemSearch::escapeHidesPopupWithoutChoosing()
{
    ItemSearch w;
    w.setCatalog(ornekKatalog());
    w.show(); // popup->isVisible() ata zincirinin görünür olmasını gerektirir
    auto *edit = w.findChild<QLineEdit *>(QStringLiteral("itemSearchEdit"));
    auto *popup = w.findChild<QListWidget *>(QStringLiteral("itemSearchPopup"));
    QSignalSpy spy(&w, &ItemSearch::itemChosen);

    typeUnicodeText(edit, QStringLiteral("iş"));
    QVERIFY(popup->isVisible());

    QTest::keyClick(edit, Qt::Key_Escape);

    QVERIFY(!popup->isVisible());
    QVERIFY(edit->text().isEmpty());
    QCOMPARE(spy.count(), 0); // satır eklenmedi
}

void TestItemSearch::emptyResultHidesPopup()
{
    ItemSearch w;
    w.setCatalog(ornekKatalog());
    w.show(); // popup->isVisible() ata zincirinin görünür olmasını gerektirir
    auto *edit = w.findChild<QLineEdit *>(QStringLiteral("itemSearchEdit"));
    auto *popup = w.findChild<QListWidget *>(QStringLiteral("itemSearchPopup"));

    QTest::keyClicks(edit, QStringLiteral("xyz"));
    QVERIFY(!popup->isVisible());
}

QTEST_MAIN(TestItemSearch)
#include "test_item_search.moc"
