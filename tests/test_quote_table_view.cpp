#include <QtTest/QtTest>

#include "ui/quote_line_model.h"
#include "ui/quote_table_view.h"

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
};

void TestQuoteTableView::tabSkipsReadOnlyColumnsWithinRow()
{
    QuoteLineModel model;
    model.addLine(mk(QStringLiteral("A")), 1.0, Money(1000), QString());
    model.addLine(mk(QStringLiteral("B")), 1.0, Money(1000), QString());

    QuoteTableView view;
    view.setModel(&model);
    view.setCurrentIndex(model.index(0, QuoteLineModel::ColAciklama));

    QTest::keyClick(&view, Qt::Key_Tab);
    // ColBirim (salt okunur) atlanır, doğrudan ColMiktar'a geçer.
    QCOMPARE(view.currentIndex().row(), 0);
    QCOMPARE(view.currentIndex().column(), int(QuoteLineModel::ColMiktar));

    QTest::keyClick(&view, Qt::Key_Tab);
    QCOMPARE(view.currentIndex().column(), int(QuoteLineModel::ColBirimFiyat));
}

void TestQuoteTableView::tabWrapsToNextRowFirstEditableColumn()
{
    QuoteLineModel model;
    model.addLine(mk(QStringLiteral("A")), 1.0, Money(1000), QString());
    model.addLine(mk(QStringLiteral("B")), 1.0, Money(1000), QString());

    QuoteTableView view;
    view.setModel(&model);
    view.setCurrentIndex(model.index(0, QuoteLineModel::ColBirimFiyat));

    QTest::keyClick(&view, Qt::Key_Tab);
    // Satır 0'ın son düzenlenebilir sütunundan sonra: ColTutar (salt okunur)
    // atlanır, satır 1'in ilk düzenlenebilirine (Açıklama) geçilir.
    QCOMPARE(view.currentIndex().row(), 1);
    QCOMPARE(view.currentIndex().column(), int(QuoteLineModel::ColAciklama));
}

void TestQuoteTableView::shiftTabSkipsBackward()
{
    QuoteLineModel model;
    model.addLine(mk(QStringLiteral("A")), 1.0, Money(1000), QString());

    QuoteTableView view;
    view.setModel(&model);
    view.setCurrentIndex(model.index(0, QuoteLineModel::ColBirimFiyat));

    QTest::keyClick(&view, Qt::Key_Backtab); // Shift+Tab
    QCOMPARE(view.currentIndex().column(), int(QuoteLineModel::ColMiktar));
}

QTEST_MAIN(TestQuoteTableView)
#include "test_quote_table_view.moc"
