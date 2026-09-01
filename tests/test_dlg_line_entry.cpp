#include <QtTest/QtTest>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include "teklif/ui/dlg_line_entry.h"

namespace {

Item mkItem()
{
    Item it;
    it.ad = QStringLiteral("İşçilik");
    it.birim = QStringLiteral("saat");
    it.varsayilanFiyat = Money::fromString(QStringLiteral("350,00")).value();
    return it;
}

QLineEdit *miktarEdit(LineEntryDialog &dlg)
{
    return dlg.findChild<QLineEdit *>(QStringLiteral("miktarEdit"));
}
QLineEdit *fiyatEdit(LineEntryDialog &dlg)
{
    return dlg.findChild<QLineEdit *>(QStringLiteral("fiyatEdit"));
}
QLabel *hataLabel(LineEntryDialog &dlg)
{
    return dlg.findChild<QLabel *>(QStringLiteral("hataLabel"));
}
void clickOk(LineEntryDialog &dlg)
{
    auto *box = dlg.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
    QTest::mouseClick(box->button(QDialogButtonBox::Ok), Qt::LeftButton);
}

} // namespace

class TestDlgLineEntry : public QObject
{
    Q_OBJECT

private slots:
    void defaultsPrefilledFromCatalog();
    void validInputAccepts();
    void zeroMiktarRejected();
    void negativeFiyatRejected();
    void letterMiktarRejected();
    void escapeRejectsWithoutAccepting();
};

void TestDlgLineEntry::defaultsPrefilledFromCatalog()
{
    LineEntryDialog dlg(mkItem());
    dlg.show(); // isVisible() ata zincirinin görünür olmasını gerektirir
    QCOMPARE(miktarEdit(dlg)->text(), QStringLiteral("1"));
    QCOMPARE(fiyatEdit(dlg)->text(), QStringLiteral("350,00")); // katalogdan ön dolu
}

void TestDlgLineEntry::validInputAccepts()
{
    LineEntryDialog dlg(mkItem());
    dlg.show(); // isVisible() ata zincirinin görünür olmasını gerektirir
    miktarEdit(dlg)->clear();
    QTest::keyClicks(miktarEdit(dlg), QStringLiteral("10"));

    clickOk(dlg);

    QCOMPARE(dlg.result(), int(QDialog::Accepted));
    QCOMPARE(dlg.miktar(), 10.0);
    QCOMPARE(dlg.birimFiyat().toString(), QStringLiteral("350,00"));
}

void TestDlgLineEntry::zeroMiktarRejected()
{
    LineEntryDialog dlg(mkItem());
    dlg.show(); // isVisible() ata zincirinin görünür olmasını gerektirir
    miktarEdit(dlg)->clear();
    QTest::keyClicks(miktarEdit(dlg), QStringLiteral("0"));

    clickOk(dlg);

    QVERIFY(dlg.result() != int(QDialog::Accepted)); // dialog kapanmadı
    QVERIFY(hataLabel(dlg)->isVisible());
    QCOMPARE(dlg.miktar(), 0.0); // hiç set edilmedi
}

void TestDlgLineEntry::negativeFiyatRejected()
{
    LineEntryDialog dlg(mkItem());
    dlg.show(); // isVisible() ata zincirinin görünür olmasını gerektirir
    fiyatEdit(dlg)->clear();
    QTest::keyClicks(fiyatEdit(dlg), QStringLiteral("-10,00"));

    clickOk(dlg);

    QVERIFY(dlg.result() != int(QDialog::Accepted));
    QVERIFY(hataLabel(dlg)->isVisible());
}

void TestDlgLineEntry::letterMiktarRejected()
{
    LineEntryDialog dlg(mkItem());
    dlg.show(); // isVisible() ata zincirinin görünür olmasını gerektirir
    miktarEdit(dlg)->clear();
    QTest::keyClicks(miktarEdit(dlg), QStringLiteral("abc"));

    clickOk(dlg);

    QVERIFY(dlg.result() != int(QDialog::Accepted));
    QVERIFY(hataLabel(dlg)->isVisible());
}

void TestDlgLineEntry::escapeRejectsWithoutAccepting()
{
    LineEntryDialog dlg(mkItem());
    dlg.show(); // isVisible() ata zincirinin görünür olmasını gerektirir
    QTest::keyClick(&dlg, Qt::Key_Escape);
    QCOMPARE(dlg.result(), int(QDialog::Rejected));
}

QTEST_MAIN(TestDlgLineEntry)
#include "test_dlg_line_entry.moc"
