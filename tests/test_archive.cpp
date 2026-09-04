#include <QtTest/QtTest>
#include <QElapsedTimer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "teklif/core/db.h"
#include "teklif/core/quote_status.h"
#include "teklif/core/repo_quotes.h"

// Arsiv listesi, durum degisimi, kopyalama ve silme davranislari.
//
// Ayri bir musteri deposu YOK: musteri bilgisi teklifin kendi sutunlarinda
// durur (bkz. core/models.h), bu yuzden testler de teklif uzerinden gider.

class TestArchive : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // --- teklifteki musteri bilgisi ---
    void customerFieldsRoundTrip();
    void customerIsFrozenPerQuote();

    // --- arsiv listesi ---
    void listReturnsNewestFirst();
    void listCarriesCustomerName();
    void listFiltersByStatus();
    void listDateRangeIsInclusive();
    void listTextSearchIsTurkishAware();
    void listPerformance500Quotes();

    // --- durum ve kopyalama ---
    void setStatusChangesStatus();
    void setStatusRejectsUnknownValue();
    void duplicateCreatesIndependentCopy();
    void duplicateDoesNotTouchOriginal();
    void removeDeletesQuoteAndItsLines();
    void removeUnknownIdFails();

private:
    QTemporaryDir *m_dir = nullptr;
    QString m_conn;
    QSqlDatabase m_db;

    qint64 addQuote(const QString &musteri, const QDate &tarih, qint64 toplamKurus,
                     const QString &proje = QString())
    {
        Quote q;
        q.musteri.unvan = musteri;
        q.tarih = tarih;
        q.projeBasligi = proje;
        q.genelToplam = Money(toplamKurus);
        QuoteLine l;
        l.sira = 1;
        l.aciklama = QStringLiteral("Kalem");
        l.birim = QStringLiteral("adet");
        l.miktar = 1;
        l.birimFiyat = Money(toplamKurus);
        l.tutar = Money(toplamKurus);
        q.satirlar.append(l);
        QString e;
        if (!RepoQuotes(m_db).add(q, &e))
            qWarning() << "teklif eklenemedi:" << e;
        return q.id;
    }
};

void TestArchive::init()
{
    m_dir = new QTemporaryDir();
    QVERIFY(m_dir->isValid());
    m_conn = QStringLiteral("ar_%1_%2")
                 .arg(QDateTime::currentMSecsSinceEpoch())
                 .arg(reinterpret_cast<quintptr>(this));
    QString err;
    QVERIFY2(Db::openAndMigrate(m_dir->filePath(QStringLiteral("t.db")), &err, m_conn), qPrintable(err));
    m_db = QSqlDatabase::database(m_conn);
}

void TestArchive::cleanup()
{
    m_db = QSqlDatabase();
    QSqlDatabase::database(m_conn).close();
    QSqlDatabase::removeDatabase(m_conn);
    delete m_dir;
    m_dir = nullptr;
}

// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------

// Musterinin butun alanlari teklifle birlikte yazilip geri okunmali.
// Bir alan INSERT ya da SELECT tarafinda unutulursa sessizce bos doner ve
// bu ancak belge basildiginda fark edilirdi.
void TestArchive::customerFieldsRoundTrip()
{
    Quote q;
    q.musteri.unvan = QStringLiteral("Şükrü Çelik İnşaat Ltd. Şti.");
    q.musteri.yetkili = QStringLiteral("Şükrü Çelik");
    q.musteri.telefon = QStringLiteral("0532 111 22 33");
    q.musteri.email = QStringLiteral("sukru@ornek.com");
    q.musteri.adres = QStringLiteral("İncilli Mah. Karakol Cd. No: 36/B Karasu");
    q.musteri.vergiDairesi = QStringLiteral("Karasu");
    q.musteri.vergiNo = QStringLiteral("1234567890");
    q.tarih = QDate(2026, 8, 25);

    RepoQuotes repo(m_db);
    QString err;
    QVERIFY2(repo.add(q, &err), qPrintable(err));

    const auto okundu = repo.get(q.id, &err);
    QVERIFY2(okundu.has_value(), qPrintable(err));
    QCOMPARE(okundu->musteri.unvan, q.musteri.unvan);
    QCOMPARE(okundu->musteri.yetkili, q.musteri.yetkili);
    QCOMPARE(okundu->musteri.telefon, q.musteri.telefon);
    QCOMPARE(okundu->musteri.email, q.musteri.email);
    QCOMPARE(okundu->musteri.adres, q.musteri.adres);
    QCOMPARE(okundu->musteri.vergiDairesi, q.musteri.vergiDairesi);
    QCOMPARE(okundu->musteri.vergiNo, q.musteri.vergiNo);
}

// Musteri bilgisinin teklifin ICINE yazilmasinin butun sebebi bu: ayni
// musteriye verilmis iki tekliften birinin adresini duzeltmek digerini
// DEGISTIRMEMELI. Ortak bir musteri kaydi olsaydi bir yil onceki belgenin
// cikitisi bugun farkli basilirdi.
void TestArchive::customerIsFrozenPerQuote()
{
    RepoQuotes repo(m_db);
    const qint64 eski = addQuote(QStringLiteral("Ahmet Yılmaz"), QDate(2025, 3, 10), 10000);
    const qint64 yeni = addQuote(QStringLiteral("Ahmet Yılmaz"), QDate(2026, 8, 25), 20000);

    auto guncel = repo.get(yeni);
    QVERIFY(guncel.has_value());
    guncel->musteri.adres = QStringLiteral("Yeni Adres 1");
    QString err;
    QVERIFY2(repo.update(*guncel, &err), qPrintable(err));

    QCOMPARE(repo.get(yeni)->musteri.adres, QStringLiteral("Yeni Adres 1"));
    QVERIFY(repo.get(eski)->musteri.adres.isEmpty());
}

void TestArchive::listReturnsNewestFirst()
{
    const QString c = QStringLiteral("Musteri");
    addQuote(c, QDate(2026, 1, 15), 10000);
    addQuote(c, QDate(2026, 8, 25), 20000);
    addQuote(c, QDate(2026, 5, 10), 30000);

    const auto liste = RepoQuotes(m_db).list(QuoteFilter{});
    QCOMPARE(liste.size(), 3);
    QCOMPARE(liste[0].tarih, QDate(2026, 8, 25));
    QCOMPARE(liste[1].tarih, QDate(2026, 5, 10));
    QCOMPARE(liste[2].tarih, QDate(2026, 1, 15));
}

void TestArchive::listCarriesCustomerName()
{
    // Unvan teklifin kendi sutunundan gelir; bos gelirse arsiv ekrani
    // musterisiz gorunur.
    const QString c = QStringLiteral("Şükrü Çelik İnşaat");
    addQuote(c, QDate(2026, 8, 1), 10000);

    const auto liste = RepoQuotes(m_db).list(QuoteFilter{});
    QCOMPARE(liste.size(), 1);
    QCOMPARE(liste[0].musteriUnvan, QStringLiteral("Şükrü Çelik İnşaat"));
    QCOMPARE(liste[0].genelToplam.toString(), QStringLiteral("100,00"));
}

void TestArchive::listFiltersByStatus()
{
    RepoQuotes repo(m_db);
    const QString c = QStringLiteral("Musteri");
    const qint64 q1 = addQuote(c, QDate(2026, 8, 1), 10000);
    addQuote(c, QDate(2026, 8, 2), 10000);

    QVERIFY(repo.setStatus(q1, QuoteStatus::onaylandi()));

    QuoteFilter f;
    f.durum = QuoteStatus::onaylandi();
    QCOMPARE(repo.list(f).size(), 1);

    f.durum = QuoteStatus::taslak();
    QCOMPARE(repo.list(f).size(), 1);
}

void TestArchive::listDateRangeIsInclusive()
{
    // SINIR TARIHLERI DAHIL: 1-31 Agustos araligi 1 ve 31 Agustos'u da icerir.
    const QString c = QStringLiteral("Musteri");
    addQuote(c, QDate(2026, 7, 31), 10000); // aralik disi (once)
    addQuote(c, QDate(2026, 8, 1), 10000);  // tam alt sinir
    addQuote(c, QDate(2026, 8, 15), 10000); // ic
    addQuote(c, QDate(2026, 8, 31), 10000); // tam ust sinir
    addQuote(c, QDate(2026, 9, 1), 10000);  // aralik disi (sonra)

    QuoteFilter f;
    f.tarihBaslangic = QDate(2026, 8, 1);
    f.tarihBitis = QDate(2026, 8, 31);
    const auto liste = RepoQuotes(m_db).list(f);
    QCOMPARE(liste.size(), 3);
    QCOMPARE(liste.last().tarih, QDate(2026, 8, 1));
    QCOMPARE(liste.first().tarih, QDate(2026, 8, 31));
}

void TestArchive::listTextSearchIsTurkishAware()
{
    const QString a = QStringLiteral("Şükrü Çelik");
    const QString b = QStringLiteral("Mehmet Yılmaz");
    addQuote(a, QDate(2026, 8, 1), 10000, QStringLiteral("Ofis Tadilatı"));
    addQuote(b, QDate(2026, 8, 2), 10000, QStringLiteral("Depo Çatısı"));

    RepoQuotes repo(m_db);
    QuoteFilter f;

    f.aranan = QStringLiteral("sukru");
    QCOMPARE(repo.list(f).size(), 1);

    // Proje basliginda da aranir.
    f.aranan = QStringLiteral("catisi");
    QCOMPARE(repo.list(f).size(), 1);

    // Teklif numarasiyla da bulunur.
    f.aranan = QStringLiteral("000001");
    QCOMPARE(repo.list(f).size(), 1);

    f.aranan = QStringLiteral("bulunmaz");
    QCOMPARE(repo.list(f).size(), 0);
}

void TestArchive::listPerformance500Quotes()
{
    const QString c = QStringLiteral("Musteri");
    for (int i = 0; i < 500; ++i)
        addQuote(c, QDate(2026, 1, 1).addDays(i % 365), 10000 + i);

    QElapsedTimer t;
    t.start();
    const auto liste = RepoQuotes(m_db).list(QuoteFilter{});
    const qint64 gecen = t.elapsed();

    QCOMPARE(liste.size(), 500);
    // Roadmap hedefi 200 ms. Sinir bilerek genis tutuldu: yuklu bir CI
    // makinesinde dar bir sinir testi rastgele kirmizi yakar, oysa burada
    // yakalanmak istenen sey N+1 sorgu gibi buyuklukce farkli bir hatadir.
    QVERIFY2(gecen < 1000,
             qPrintable(QStringLiteral("500 teklif listesi %1 ms surdu").arg(gecen)));
}

// ---------------------------------------------------------------------------

void TestArchive::setStatusChangesStatus()
{
    RepoQuotes repo(m_db);
    const QString c = QStringLiteral("Musteri");
    const qint64 id = addQuote(c, QDate(2026, 8, 1), 10000);

    // Yeni teklif Taslak olarak baslar.
    QCOMPARE(repo.get(id)->durum, QuoteStatus::taslak());

    QString err;
    QVERIFY2(repo.setStatus(id, QuoteStatus::gonderildi(), &err), qPrintable(err));
    QCOMPARE(repo.get(id)->durum, QuoteStatus::gonderildi());
}

void TestArchive::setStatusRejectsUnknownValue()
{
    RepoQuotes repo(m_db);
    const QString c = QStringLiteral("Musteri");
    const qint64 id = addQuote(c, QDate(2026, 8, 1), 10000);

    QString err;
    QVERIFY(!repo.setStatus(id, QStringLiteral("Bilinmeyen"), &err));
    QVERIFY(!err.isEmpty());
    // Durum degismemis olmali.
    QCOMPARE(repo.get(id)->durum, QuoteStatus::taslak());

    // Olmayan id de reddedilmeli.
    QVERIFY(!repo.setStatus(9999, QuoteStatus::onaylandi(), &err));
}

void TestArchive::duplicateCreatesIndependentCopy()
{
    RepoQuotes repo(m_db);
    const QString c = QStringLiteral("Musteri");
    const qint64 id = addQuote(c, QDate(2025, 3, 10), 50000, QStringLiteral("Eski Proje"));
    QVERIFY(repo.setStatus(id, QuoteStatus::onaylandi()));

    QString err;
    const auto kopya = repo.duplicate(id, &err);
    QVERIFY2(kopya.has_value(), qPrintable(err));

    const auto orijinal = repo.get(id);
    QVERIFY(orijinal.has_value());

    QVERIFY(kopya->id != orijinal->id);
    QVERIFY(kopya->teklifNo != orijinal->teklifNo);   // yeni numara
    QCOMPARE(kopya->tarih, QDate::currentDate());      // bugunun tarihi
    QCOMPARE(kopya->durum, QuoteStatus::taslak());     // durum sifirlanir
    QCOMPARE(kopya->musteri.unvan, orijinal->musteri.unvan);
    QCOMPARE(kopya->projeBasligi, orijinal->projeBasligi);
    QCOMPARE(kopya->satirlar.size(), orijinal->satirlar.size());
    QCOMPARE(kopya->satirlar.first().birimFiyat, orijinal->satirlar.first().birimFiyat);
    // Satir kimlikleri kaynaktan kopyalanmamali.
    QVERIFY(kopya->satirlar.first().id != orijinal->satirlar.first().id);
}

void TestArchive::duplicateDoesNotTouchOriginal()
{
    RepoQuotes repo(m_db);
    const QString c = QStringLiteral("Musteri");
    const qint64 id = addQuote(c, QDate(2025, 3, 10), 50000, QStringLiteral("Eski Proje"));
    QVERIFY(repo.setStatus(id, QuoteStatus::onaylandi()));

    auto kopya = repo.duplicate(id);
    QVERIFY(kopya.has_value());

    // Kopyayi degistir.
    kopya->projeBasligi = QStringLiteral("Yeni Proje");
    kopya->satirlar.first().birimFiyat = Money(99900);
    kopya->satirlar.first().tutar = Money(99900);
    QString err;
    QVERIFY2(repo.update(*kopya, &err), qPrintable(err));

    // Orijinal hic etkilenmemeli.
    const auto orijinal = repo.get(id);
    QVERIFY(orijinal.has_value());
    QCOMPARE(orijinal->projeBasligi, QStringLiteral("Eski Proje"));
    QCOMPARE(orijinal->durum, QuoteStatus::onaylandi());
    QCOMPARE(orijinal->satirlar.first().birimFiyat.toString(), QStringLiteral("500,00"));
}

void TestArchive::removeDeletesQuoteAndItsLines()
{
    RepoQuotes repo(m_db);
    const QString c = QStringLiteral("Musteri");
    const qint64 id = addQuote(c, QDate(2026, 8, 1), 10000);
    const qint64 kalan = addQuote(c, QDate(2026, 8, 2), 20000);

    // Satirlar gercekten var.
    QSqlQuery say(m_db);
    say.prepare(QStringLiteral("SELECT COUNT(*) FROM quote_lines WHERE quote_id = :id"));
    say.bindValue(QStringLiteral(":id"), id);
    QVERIFY(say.exec() && say.next());
    QVERIFY(say.value(0).toInt() > 0);

    QString err;
    QVERIFY2(repo.remove(id, &err), qPrintable(err));

    // Teklif gitti.
    QVERIFY(!repo.get(id).has_value());

    // Satirlari da gitti: quote_lines uzerinde ON DELETE CASCADE var ve
    // PRAGMA foreign_keys acik olmadan bu SESSIZCE calismaz.
    say.bindValue(QStringLiteral(":id"), id);
    QVERIFY(say.exec() && say.next());
    QCOMPARE(say.value(0).toInt(), 0);

    // Diger teklif etkilenmemeli.
    QVERIFY(repo.get(kalan).has_value());
    QCOMPARE(repo.list(QuoteFilter{}).size(), 1);
}

void TestArchive::removeUnknownIdFails()
{
    // Olmayan bir id sessizce "basarili" sayilmamali.
    QString err;
    QVERIFY(!RepoQuotes(m_db).remove(9999, &err));
    QVERIFY(!err.isEmpty());
}

QTEST_MAIN(TestArchive)
#include "test_archive.moc"
