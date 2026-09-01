#include <QtTest/QtTest>

#include "core/csv.h"

// csv.cpp'nin dogrudan birim testleri. Daha once yalnizca depo uzerinden
// dolayli test ediliyordu; tirnakli alan, satir ici yeni satir ve Excel
// uyumu gibi asil zor kisimlar kapsanmiyordu.

class TestCsv : public QObject
{
    Q_OBJECT

private slots:
    void parseBasicRows();
    void parseQuotedFieldWithComma();
    void parseQuotedFieldWithNewline();
    void parseEscapedQuotes();
    void parseSkipsBlankLines();
    void parseRejectsWrongColumnCount();
    void parseRejectsEmptyRequiredFields();
    void parseRejectsBadPrice();
    void parseAcceptsEmptyCategory();

    // --- Excel uyumu ---
    void exportStartsWithBom();
    void exportUsesCrLf();
    void exportQuotesPriceField();
    void importStripsBom();
    void importDetectsSemicolonDelimiter();
    void importAcceptsExcelStyleFile();
    void roundTripPreservesTurkishChars();

private:
    static Item mkItem(const QString &kod, const QString &ad, const QString &birim,
                        qint64 fiyatKurus, qint64 catId = 0)
    {
        Item it;
        it.kod = kod; it.ad = ad; it.birim = birim;
        it.varsayilanFiyat = Money(fiyatKurus);
        it.categoryId = catId;
        return it;
    }
};

void TestCsv::parseBasicRows()
{
    const QString csv = QStringLiteral(
        "kod,ad,birim,fiyat,kategori\n"
        "K-001,Alçıpan,adet,\"180,00\",Kaba\n"
        "K-002,İşçilik,saat,\"350,50\",\n");

    QString err;
    const auto rows = csvSatirlariniAyristir(csv, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows[0].kod, QStringLiteral("K-001"));
    QCOMPARE(rows[0].ad, QStringLiteral("Alçıpan"));
    QCOMPARE(rows[0].fiyat.toString(), QStringLiteral("180,00"));
    QCOMPARE(rows[0].kategoriAdi, QStringLiteral("Kaba"));
    QCOMPARE(rows[1].fiyat.toString(), QStringLiteral("350,50"));
    QVERIFY(rows[1].kategoriAdi.isEmpty());
}

void TestCsv::parseQuotedFieldWithComma()
{
    const QString csv = QStringLiteral(
        "kod,ad,birim,fiyat,kategori\n"
        "K-1,\"Levha, 12 mm\",adet,\"10,00\",\n");
    QString err;
    const auto rows = csvSatirlariniAyristir(csv, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows[0].ad, QStringLiteral("Levha, 12 mm"));
}

void TestCsv::parseQuotedFieldWithNewline()
{
    // Tirnak icinde GERCEK satir sonu: naif satir bolme burada yanlis sonuc verirdi.
    const QString csv = QStringLiteral(
        "kod,ad,birim,fiyat,kategori\n"
        "K-1,\"Birinci satır\nİkinci satır\",adet,\"10,00\",\n");
    QString err;
    const auto rows = csvSatirlariniAyristir(csv, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QCOMPARE(rows.size(), 1);
    QVERIFY(rows[0].ad.contains(QLatin1Char('\n')));
}

void TestCsv::parseEscapedQuotes()
{
    const QString csv = QStringLiteral(
        "kod,ad,birim,fiyat,kategori\n"
        "K-1,\"12\"\" boru\",adet,\"10,00\",\n");
    QString err;
    const auto rows = csvSatirlariniAyristir(csv, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QCOMPARE(rows[0].ad, QStringLiteral("12\" boru"));
}

void TestCsv::parseSkipsBlankLines()
{
    const QString csv = QStringLiteral(
        "kod,ad,birim,fiyat,kategori\n"
        "K-1,Ad,adet,\"10,00\",\n"
        "\n"
        "K-2,Ad2,adet,\"20,00\",\n"
        "\n");
    QString err;
    const auto rows = csvSatirlariniAyristir(csv, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QCOMPARE(rows.size(), 2);
}

void TestCsv::parseRejectsWrongColumnCount()
{
    const QString csv = QStringLiteral("kod,ad,birim,fiyat,kategori\nK-1,Ad,adet\n");
    QString err;
    const auto rows = csvSatirlariniAyristir(csv, &err);
    QVERIFY(rows.isEmpty());
    QVERIFY(err.contains(QStringLiteral("2"))); // kullaniciya gosterilen satir no
}

void TestCsv::parseRejectsEmptyRequiredFields()
{
    const QString csv = QStringLiteral("kod,ad,birim,fiyat,kategori\n,Ad,adet,\"1,00\",\n");
    QString err;
    QVERIFY(csvSatirlariniAyristir(csv, &err).isEmpty());
    QVERIFY(!err.isEmpty());
}

void TestCsv::parseRejectsBadPrice()
{
    const QString csv = QStringLiteral("kod,ad,birim,fiyat,kategori\nK-1,Ad,adet,abc,\n");
    QString err;
    QVERIFY(csvSatirlariniAyristir(csv, &err).isEmpty());
    QVERIFY(err.contains(QStringLiteral("fiyat")));
}

void TestCsv::parseAcceptsEmptyCategory()
{
    const QString csv = QStringLiteral("kod,ad,birim,fiyat,kategori\nK-1,Ad,adet,\"1,00\",\n");
    QString err;
    const auto rows = csvSatirlariniAyristir(csv, &err);
    QCOMPARE(rows.size(), 1);
    QVERIFY(rows[0].kategoriAdi.isEmpty());
}

// ---------------------------------------------------------------------------

void TestCsv::exportStartsWithBom()
{
    // BOM olmadan Turkce Windows'ta Excel dosyayi cp1254 sanip s/g/i
    // harflerini bozar.
    const QString csv = csvOlustur({mkItem("K-1", "İşçilik", "saat", 35000)}, {});
    QVERIFY2(csv.startsWith(QChar(0xFEFF)), "cikti BOM ile baslamiyor");
}

void TestCsv::exportUsesCrLf()
{
    const QString csv = csvOlustur({mkItem("K-1", "Ad", "adet", 100)}, {});
    QVERIFY2(csv.contains(QStringLiteral("\r\n")), "satir sonu CRLF degil");
}

void TestCsv::exportQuotesPriceField()
{
    // Fiyat "1.234,56" biciminde virgul icerir; tirnaklanmazsa virgul
    // ayracli bir dosyada iki sutuna bolunurdu.
    const QString csv = csvOlustur({mkItem("K-1", "Ad", "adet", 123456)}, {});
    QVERIFY2(csv.contains(QStringLiteral("\"1.234,56\"")), qPrintable(csv));
}

void TestCsv::importStripsBom()
{
    const QString csv = QChar(0xFEFF)
        + QStringLiteral("kod,ad,birim,fiyat,kategori\r\nK-1,Ad,adet,\"10,00\",\r\n");
    QString err;
    const auto rows = csvSatirlariniAyristir(csv, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows[0].kod, QStringLiteral("K-1"));
}

void TestCsv::importDetectsSemicolonDelimiter()
{
    // Turkce Windows'ta Excel dosyayi ';' ile ayirarak kaydeder.
    const QString csv = QStringLiteral(
        "kod;ad;birim;fiyat;kategori\r\n"
        "K-1;Alçıpan;adet;180,00;Kaba\r\n");
    QString err;
    const auto rows = csvSatirlariniAyristir(csv, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows[0].ad, QStringLiteral("Alçıpan"));
    // ';' ayracinda fiyat tirnaksiz da olsa bolunmez.
    QCOMPARE(rows[0].fiyat.toString(), QStringLiteral("180,00"));
}

void TestCsv::importAcceptsExcelStyleFile()
{
    // Excel'in tipik ciktisi: BOM + CRLF + noktali virgul.
    const QString csv = QChar(0xFEFF)
        + QStringLiteral("kod;ad;birim;fiyat;kategori\r\n"
                          "K-1;İşçilik;saat;350,00;Hizmet\r\n"
                          "K-2;Şap;m2;\"1.250,00\";Kaba\r\n");
    QString err;
    const auto rows = csvSatirlariniAyristir(csv, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows[0].ad, QStringLiteral("İşçilik"));
    QCOMPARE(rows[1].fiyat.toString(), QStringLiteral("1.250,00"));
    QCOMPARE(rows[1].kategoriAdi, QStringLiteral("Kaba"));
}

void TestCsv::roundTripPreservesTurkishChars()
{
    QHash<qint64, QString> kategoriler;
    kategoriler.insert(7, QStringLiteral("Kaba İşler"));

    const QVector<Item> kalemler = {
        mkItem("K-1", "İşçilik", "saat", 35000, 7),
        mkItem("K-2", "Alçıpan Levha, 12 mm", "adet", 123456, 0),
        mkItem("K-3", "Şap \"özel\"", "m2", 99, 7),
    };

    const QString csv = csvOlustur(kalemler, kategoriler);
    QString err;
    const auto rows = csvSatirlariniAyristir(csv, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QCOMPARE(rows.size(), 3);

    QCOMPARE(rows[0].ad, QStringLiteral("İşçilik"));
    QCOMPARE(rows[0].kategoriAdi, QStringLiteral("Kaba İşler"));
    QCOMPARE(rows[1].ad, QStringLiteral("Alçıpan Levha, 12 mm")); // virgullu ad
    QCOMPARE(rows[1].fiyat.toString(), QStringLiteral("1.234,56"));
    QCOMPARE(rows[2].ad, QStringLiteral("Şap \"özel\""));         // tirnakli ad
    QCOMPARE(rows[2].fiyat.toString(), QStringLiteral("0,99"));
}

QTEST_APPLESS_MAIN(TestCsv)
#include "test_csv.moc"
