#include <QtTest/QtTest>
#include <QSignalSpy>

#include "ui/quote_line_model.h"

namespace {

Item mkItem(const QString &ad, const QString &birim, qint64 fiyatKurus = 0)
{
    Item it;
    it.ad = ad;
    it.birim = birim;
    it.varsayilanFiyat = Money(fiyatKurus);
    return it;
}

} // namespace

class TestQuoteLineModel : public QObject
{
    Q_OBJECT

private slots:
    void addLineUpdatesTableAndTotals();
    void removeMiddleRowRenumbers();
    void editMiktarRecomputesTutar();
    void editMiktarCommaAccepted();
    void editMiktarDotRejected();
    void editMiktarLetterRejected();
    void editMiktarZeroRejected();
    void editMiktarNegativeRejected();
    void editBirimFiyatNegativeRejected();
    void tutarColumnNotEditable();
    void siraAndBirimColumnsNotEditable();
    void editAciklamaDoesNotTouchSourceItem();
    void editAciklamaEmptyRejected();
};

void TestQuoteLineModel::addLineUpdatesTableAndTotals()
{
    QuoteLineModel model;
    QSignalSpy spy(&model, &QuoteLineModel::totalsMayHaveChanged);

    const Item it = mkItem(QStringLiteral("Alçıpan Levha"), QStringLiteral("adet"), 18000);
    model.addLine(it, 24.0, it.varsayilanFiyat, QString());

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.lines().first().aciklama, QStringLiteral("Alçıpan Levha"));
    QCOMPARE(model.lines().first().sira, 1);
    QCOMPARE(model.lines().first().tutar.toString(), QStringLiteral("4.320,00"));
    QCOMPARE(spy.count(), 1);
}

void TestQuoteLineModel::removeMiddleRowRenumbers()
{
    QuoteLineModel model;
    model.addLine(mkItem(QStringLiteral("A"), QStringLiteral("adet"), 100), 1.0, Money(100), QString());
    model.addLine(mkItem(QStringLiteral("B"), QStringLiteral("adet"), 100), 1.0, Money(100), QString());
    model.addLine(mkItem(QStringLiteral("C"), QStringLiteral("adet"), 100), 1.0, Money(100), QString());

    model.removeLine(1); // ortadaki (B)

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.lines()[0].aciklama, QStringLiteral("A"));
    QCOMPARE(model.lines()[0].sira, 1);
    QCOMPARE(model.lines()[1].aciklama, QStringLiteral("C"));
    QCOMPARE(model.lines()[1].sira, 2); // 3'ten 2'ye yeniden numaralandı
}

void TestQuoteLineModel::editMiktarRecomputesTutar()
{
    QuoteLineModel model;
    model.addLine(mkItem(QStringLiteral("A"), QStringLiteral("adet"), 10000), 1.0, Money(10000), QString());

    QSignalSpy spy(&model, &QuoteLineModel::totalsMayHaveChanged);
    const QModelIndex idx = model.index(0, QuoteLineModel::ColMiktar);
    QVERIFY(model.setData(idx, QStringLiteral("3"), Qt::EditRole));

    QCOMPARE(model.lines().first().miktar, 3.0);
    QCOMPARE(model.lines().first().tutar.toString(), QStringLiteral("300,00"));
    QCOMPARE(spy.count(), 1);
}

void TestQuoteLineModel::editMiktarCommaAccepted()
{
    QuoteLineModel model;
    model.addLine(mkItem(QStringLiteral("A"), QStringLiteral("m2")), 1.0, Money(10000), QString());
    const QModelIndex idx = model.index(0, QuoteLineModel::ColMiktar);
    QVERIFY(model.setData(idx, QStringLiteral("12,5"), Qt::EditRole));
    QCOMPARE(model.lines().first().miktar, 12.5);
}

void TestQuoteLineModel::editMiktarDotRejected()
{
    // BİLEREK reddediliyor: nokta sadece binlik ayraç, ondalık değil.
    // Money::fromString ve parseTurkishNumber ile TUTARLI davranış.
    QuoteLineModel model;
    model.addLine(mkItem(QStringLiteral("A"), QStringLiteral("m2")), 1.0, Money(10000), QString());
    const double eskiMiktar = model.lines().first().miktar;
    const QModelIndex idx = model.index(0, QuoteLineModel::ColMiktar);
    QVERIFY(!model.setData(idx, QStringLiteral("12.5"), Qt::EditRole));
    QCOMPARE(model.lines().first().miktar, eskiMiktar); // eski değer korunmuş
}

void TestQuoteLineModel::editMiktarLetterRejected()
{
    QuoteLineModel model;
    model.addLine(mkItem(QStringLiteral("A"), QStringLiteral("adet")), 5.0, Money(1000), QString());
    const QModelIndex idx = model.index(0, QuoteLineModel::ColMiktar);
    QVERIFY(!model.setData(idx, QStringLiteral("abc"), Qt::EditRole));
    QCOMPARE(model.lines().first().miktar, 5.0);
}

void TestQuoteLineModel::editMiktarZeroRejected()
{
    QuoteLineModel model;
    model.addLine(mkItem(QStringLiteral("A"), QStringLiteral("adet")), 5.0, Money(1000), QString());
    const QModelIndex idx = model.index(0, QuoteLineModel::ColMiktar);
    QVERIFY(!model.setData(idx, QStringLiteral("0"), Qt::EditRole));
    QCOMPARE(model.lines().first().miktar, 5.0);
}

void TestQuoteLineModel::editMiktarNegativeRejected()
{
    QuoteLineModel model;
    model.addLine(mkItem(QStringLiteral("A"), QStringLiteral("adet")), 5.0, Money(1000), QString());
    const QModelIndex idx = model.index(0, QuoteLineModel::ColMiktar);
    QVERIFY(!model.setData(idx, QStringLiteral("-3"), Qt::EditRole));
    QCOMPARE(model.lines().first().miktar, 5.0);
}

void TestQuoteLineModel::editBirimFiyatNegativeRejected()
{
    QuoteLineModel model;
    model.addLine(mkItem(QStringLiteral("A"), QStringLiteral("adet")), 5.0, Money(1000), QString());
    const QModelIndex idx = model.index(0, QuoteLineModel::ColBirimFiyat);
    QVERIFY(!model.setData(idx, QStringLiteral("-10,00"), Qt::EditRole));
    QCOMPARE(model.lines().first().birimFiyat.kurus(), qint64(1000));
}

void TestQuoteLineModel::tutarColumnNotEditable()
{
    QuoteLineModel model;
    model.addLine(mkItem(QStringLiteral("A"), QStringLiteral("adet")), 5.0, Money(1000), QString());
    const QModelIndex idx = model.index(0, QuoteLineModel::ColTutar);
    QVERIFY(!(model.flags(idx) & Qt::ItemIsEditable));
    QVERIFY(!model.setData(idx, QStringLiteral("999,00"), Qt::EditRole));
}

void TestQuoteLineModel::siraAndBirimColumnsNotEditable()
{
    QuoteLineModel model;
    model.addLine(mkItem(QStringLiteral("A"), QStringLiteral("adet")), 5.0, Money(1000), QString());
    QVERIFY(!(model.flags(model.index(0, QuoteLineModel::ColSira)) & Qt::ItemIsEditable));
    QVERIFY(!(model.flags(model.index(0, QuoteLineModel::ColBirim)) & Qt::ItemIsEditable));
}

void TestQuoteLineModel::editAciklamaDoesNotTouchSourceItem()
{
    QuoteLineModel model;
    const Item kaynak = mkItem(QStringLiteral("Orijinal Ad"), QStringLiteral("adet"), 1000);
    model.addLine(kaynak, 1.0, Money(1000), QString());

    const QModelIndex idx = model.index(0, QuoteLineModel::ColAciklama);
    QVERIFY(model.setData(idx, QStringLiteral("Değiştirilmiş Ad"), Qt::EditRole));

    QCOMPARE(model.lines().first().aciklama, QStringLiteral("Değiştirilmiş Ad"));
    // kaynak Item (katalog) DEĞİŞMEDİ — model kendi kopyasını tutuyor.
    QCOMPARE(kaynak.ad, QStringLiteral("Orijinal Ad"));
}

void TestQuoteLineModel::editAciklamaEmptyRejected()
{
    QuoteLineModel model;
    model.addLine(mkItem(QStringLiteral("A"), QStringLiteral("adet")), 1.0, Money(1000), QString());
    const QModelIndex idx = model.index(0, QuoteLineModel::ColAciklama);
    QVERIFY(!model.setData(idx, QStringLiteral("   "), Qt::EditRole));
    QCOMPARE(model.lines().first().aciklama, QStringLiteral("A"));
}

QTEST_APPLESS_MAIN(TestQuoteLineModel)
#include "test_quote_line_model.moc"
