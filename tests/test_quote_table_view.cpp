#include <QtTest/QtTest>
#include <QPushButton>
#include <QVBoxLayout>

#include "teklif/ui/quote_line_model.h"
#include "teklif/ui/quote_table_view.h"

namespace {

Item mk(const QString &ad)
{
    Item it;
    it.ad = ad;
    it.birim = QStringLiteral("adet");
    it.varsayilanFiyat = Money(1000);
    return it;
}

} // namespace

class TestQuoteTableView : public QObject
{
    Q_OBJECT

private slots:
    void tabSkipsReadOnlyColumnsWithinRow();
    void tabWrapsToNextRowFirstEditableColumn();
    void shiftTabSkipsBackward();
    void tabAtLastCellLeavesTable();
    void shiftTabAtFirstCellLeavesTable();
};

void TestQuoteTableView::tabSkipsReadOnlyColumnsWithinRow()
{
    QuoteLineModel model;
    model.addLine(mk(QStringLiteral("A")), 1.0, Money(1000), QString());
    model.addLine(mk(QStringLiteral("B")), 1.0, Money(1000), QString());

    QuoteTableView view;
    view.setModel(&model);
    view.setCurrentIndex(model.index(0, QuoteLineModel::ColBirimFiyat));

    QTest::keyClick(&view, Qt::Key_Tab);
    // ColTutar HESAPLANIR, elle yazılamaz; atlanıp ColNot'a geçilir.
    QCOMPARE(view.currentIndex().row(), 0);
    QCOMPARE(view.currentIndex().column(), int(QuoteLineModel::ColNot));
}

void TestQuoteTableView::tabWrapsToNextRowFirstEditableColumn()
{
    QuoteLineModel model;
    model.addLine(mk(QStringLiteral("A")), 1.0, Money(1000), QString());
    model.addLine(mk(QStringLiteral("B")), 1.0, Money(1000), QString());

    QuoteTableView view;
    view.setModel(&model);
    view.setCurrentIndex(model.index(0, QuoteLineModel::ColNot));

    QTest::keyClick(&view, Qt::Key_Tab);
    // Satır 0'ın SON düzenlenebilir sütunundan sonra satır 1'in ilkine
    // (Açıklama) geçilir; ColSira hesaplanır, atlanır.
    QCOMPARE(view.currentIndex().row(), 1);
    QCOMPARE(view.currentIndex().column(), int(QuoteLineModel::ColAciklama));
}

void TestQuoteTableView::shiftTabSkipsBackward()
{
    QuoteLineModel model;
    model.addLine(mk(QStringLiteral("A")), 1.0, Money(1000), QString());

    QuoteTableView view;
    view.setModel(&model);
    view.setCurrentIndex(model.index(0, QuoteLineModel::ColNot));

    QTest::keyClick(&view, Qt::Key_Backtab); // Shift+Tab
    // Geriye giderken de ColTutar atlanır.
    QCOMPARE(view.currentIndex().column(), int(QuoteLineModel::ColBirimFiyat));
}

void TestQuoteTableView::tabAtLastCellLeavesTable()
{
    // ODAK TABLOYA HAPSOLMAMALI. Eskiden son hucrede Tab imleci ilk satira
    // sariyordu; kullanici klavyeyle Kaydet dugmesine hic ulasamiyordu.
    QWidget kap;
    auto *lay = new QVBoxLayout(&kap);
    auto *view = new QuoteTableView(&kap);
    auto *sonraki = new QPushButton(QStringLiteral("Kaydet"), &kap);
    lay->addWidget(view);
    lay->addWidget(sonraki);

    QuoteLineModel model;
    Item it;
    it.ad = QStringLiteral("Kalem"); it.birim = QStringLiteral("adet");
    model.addLine(it, 1.0, Money(1000), QString());
    view->setModel(&model);

    kap.show();
    QVERIFY(QTest::qWaitForWindowExposed(&kap));

    // Tek satirin SON duzenlenebilir hucresi.
    view->setFocus();
    view->setCurrentIndex(model.index(0, QuoteLineModel::ColNot));
    QVERIFY(view->hasFocus());

    QTest::keyClick(view, Qt::Key_Tab);

    // Imlec sarmamali ve odak tabloyu terk etmeli.
    QCOMPARE(view->currentIndex().row(), 0);
    QCOMPARE(view->currentIndex().column(), int(QuoteLineModel::ColNot));
    QVERIFY2(!view->hasFocus(), "odak hala tabloda - Tab ile cikilamiyor");
}

void TestQuoteTableView::shiftTabAtFirstCellLeavesTable()
{
    QWidget kap;
    auto *lay = new QVBoxLayout(&kap);
    auto *onceki = new QPushButton(QStringLiteral("Ara"), &kap);
    auto *view = new QuoteTableView(&kap);
    lay->addWidget(onceki);
    lay->addWidget(view);

    QuoteLineModel model;
    Item it;
    it.ad = QStringLiteral("Kalem"); it.birim = QStringLiteral("adet");
    model.addLine(it, 1.0, Money(1000), QString());
    view->setModel(&model);

    kap.show();
    QVERIFY(QTest::qWaitForWindowExposed(&kap));

    // Tek satirin ILK duzenlenebilir hucresi.
    view->setFocus();
    view->setCurrentIndex(model.index(0, QuoteLineModel::ColAciklama));
    QVERIFY(view->hasFocus());

    QTest::keyClick(view, Qt::Key_Backtab);

    QCOMPARE(view->currentIndex().column(), int(QuoteLineModel::ColAciklama));
    QVERIFY2(!view->hasFocus(), "odak hala tabloda - Shift+Tab ile cikilamiyor");
}

QTEST_MAIN(TestQuoteTableView)
#include "test_quote_table_view.moc"
