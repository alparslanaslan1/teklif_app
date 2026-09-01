#include <QtTest/QtTest>
#include <QElapsedTimer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "teklif/core/db.h"
#include "teklif/core/quote_status.h"
#include "teklif/core/repo_customers.h"
#include "teklif/core/repo_quotes.h"

// Part 6 (Musteriler ve arsiv) kapsamindaki davranislar.

class TestArchive : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // --- musteri deposu ---
    void updateChangesFields();
    void updateUnknownIdFails();
    void setActiveHidesFromDefaultList();
    void getReturnsInactiveCustomer();
    void searchFindsTurkishChars();
    void searchMatchesContactAndPhone();
    void searchEmptyReturnsAll();
    void quoteCountReflectsQuotes();
    void customerWithQuotesCannotBeDeleted();

    // --- arsiv listesi ---
    void listReturnsNewestFirst();
    void listCarriesCustomerName();
    void listFiltersByCustomer();
    void listFiltersByStatus();
    void listDateRangeIsInclusive();
    void listTextSearchIsTurkishAware();
    void listPerformance500Quotes();

    // --- durum ve kopyalama ---
    void setStatusChangesStatus();
    void setStatusRejectsUnknownValue();
    void duplicateCreatesIndependentCopy();
    void duplicateDoesNotTouchOriginal();
    void customerTotalSumsQuotes();
    void removeDeletesQuoteAndItsLines();
    void removeUnknownIdFails();

private:
    QTemporaryDir *m_dir = nullptr;
    QString m_conn;
    QSqlDatabase m_db;

    qint64 addCustomer(const QString &unvan, const QString &yetkili = QString(),
                        const QString &telefon = QString())
    {
        Customer c;
        c.unvan = unvan;
        c.yetkili = yetkili;
        c.telefon = telefon;
        QString e;
        const bool ok = RepoCustomers(m_db).add(c, &e);
        if (!ok)
            qWarning() << "musteri eklenemedi:" << e;
        return c.id;
    }

    qint64 addQuote(qint64 custId, const QDate &tarih, qint64 toplamKurus,
                     const QString &proje = QString())
    {
        Quote q;
        q.customerId = custId;
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

void TestArchive::updateChangesFields()
{
    RepoCustomers repo(m_db);
    const qint64 id = addCustomer(QStringLiteral("Eski Unvan"));

    auto c = repo.get(id);
    QVERIFY(c.has_value());
    c->unvan = QStringLiteral("Yeni Unvan");
    c->yetkili = QStringLiteral("Ayşe Demir");
    c->vergiNo = QStringLiteral("1234567890");

    QString err;
    QVERIFY2(repo.update(*c, &err), qPrintable(err));

    const auto okundu = repo.get(id);
    QVERIFY(okundu.has_value());
    QCOMPARE(okundu->unvan, QStringLiteral("Yeni Unvan"));
    QCOMPARE(okundu->yetkili, QStringLiteral("Ayşe Demir"));
    QCOMPARE(okundu->vergiNo, QStringLiteral("1234567890"));
}

void TestArchive::updateUnknownIdFails()
{
    // Olmayan bir id sessizce "basarili" sayilmamali.
    Customer c;
    c.id = 9999;
    c.unvan = QStringLiteral("Hayalet");
    QString err;
    QVERIFY(!RepoCustomers(m_db).update(c, &err));
    QVERIFY(!err.isEmpty());
}

void TestArchive::setActiveHidesFromDefaultList()
{
    RepoCustomers repo(m_db);
    const qint64 id = addCustomer(QStringLiteral("Pasif Olacak"));

    QString err;
    QVERIFY2(repo.setActive(id, false, &err), qPrintable(err));
    QCOMPARE(repo.listAll(/*includeInactive=*/false).size(), 0);
    QCOMPARE(repo.listAll(/*includeInactive=*/true).size(), 1);
}

void TestArchive::getReturnsInactiveCustomer()
{
    // Eski bir teklifin musterisi pasife alinmis olabilir; belge antetinde
    // yine de gorunmeli, bu yuzden get() aktiflige BAKMAZ.
    RepoCustomers repo(m_db);
    const qint64 id = addCustomer(QStringLiteral("Pasif Musteri"));
    QVERIFY(repo.setActive(id, false));

    const auto c = repo.get(id);
    QVERIFY(c.has_value());
    QCOMPARE(c->unvan, QStringLiteral("Pasif Musteri"));
    QVERIFY(!c->aktif);
}

void TestArchive::searchFindsTurkishChars()
{
    RepoCustomers repo(m_db);
    addCustomer(QStringLiteral("Şükrü Çelik İnşaat"));
    addCustomer(QStringLiteral("Mehmet Yılmaz"));

    // Turkce klavyesi olmayan kullanici ASCII yazarak da bulabilmeli.
    QCOMPARE(repo.search(QStringLiteral("sukru")).size(), 1);
    QCOMPARE(repo.search(QStringLiteral("SUKRU")).size(), 1);
    QCOMPARE(repo.search(QStringLiteral("Şükrü")).size(), 1);
    QCOMPARE(repo.search(QStringLiteral("insaat")).size(), 1);
    QCOMPARE(repo.search(QStringLiteral("yilmaz")).size(), 1);
    QCOMPARE(repo.search(QStringLiteral("bulunmaz")).size(), 0);
}

void TestArchive::searchMatchesContactAndPhone()
{
    // Kullanici musteriyi bazen firma adiyla, bazen yetkilinin adiyla hatirlar.
    RepoCustomers repo(m_db);
    addCustomer(QStringLiteral("ABC Ltd."), QStringLiteral("Ahmet Öztürk"),
                 QStringLiteral("0312 555 44 33"));

    QCOMPARE(repo.search(QStringLiteral("ozturk")).size(), 1);
    QCOMPARE(repo.search(QStringLiteral("555 44")).size(), 1);
}

void TestArchive::searchEmptyReturnsAll()
{
    RepoCustomers repo(m_db);
    addCustomer(QStringLiteral("Bir"));
    addCustomer(QStringLiteral("Iki"));
    QCOMPARE(repo.search(QString()).size(), 2);
    QCOMPARE(repo.search(QStringLiteral("   ")).size(), 2);
}

void TestArchive::quoteCountReflectsQuotes()
{
    RepoCustomers repo(m_db);
    const qint64 id = addCustomer(QStringLiteral("Musteri"));
    QCOMPARE(repo.quoteCount(id), 0);

    addQuote(id, QDate(2026, 8, 1), 10000);
    addQuote(id, QDate(2026, 8, 2), 20000);
    QCOMPARE(repo.quoteCount(id), 2);
}

void TestArchive::customerWithQuotesCannotBeDeleted()
{
    // Semada quotes.customer_id ON DELETE RESTRICT; teklifi olan musteri
    // silinemez. Program zaten DELETE cagirmiyor ama korumanin GERCEKTEN
    // etkin oldugunu dogrulamak gerekir (PRAGMA foreign_keys unutulursa
    // sessizce etkisiz kalirdi).
    const qint64 id = addCustomer(QStringLiteral("Teklifli Musteri"));
    addQuote(id, QDate(2026, 8, 1), 10000);

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM customers WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    QVERIFY2(!q.exec(), "teklifi olan musteri silinebildi - FK korumasi calismiyor");
}

// ---------------------------------------------------------------------------

void TestArchive::listReturnsNewestFirst()
{
    const qint64 c = addCustomer(QStringLiteral("Musteri"));
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
    // JOIN calismiyorsa unvan bos gelir ve arsiv ekrani musterisiz gorunur.
    const qint64 c = addCustomer(QStringLiteral("Şükrü Çelik İnşaat"));
    addQuote(c, QDate(2026, 8, 1), 10000);

    const auto liste = RepoQuotes(m_db).list(QuoteFilter{});
    QCOMPARE(liste.size(), 1);
    QCOMPARE(liste[0].customerUnvan, QStringLiteral("Şükrü Çelik İnşaat"));
    QCOMPARE(liste[0].genelToplam.toString(), QStringLiteral("100,00"));
}

void TestArchive::listFiltersByCustomer()
{
    const qint64 a = addCustomer(QStringLiteral("A Musteri"));
    const qint64 b = addCustomer(QStringLiteral("B Musteri"));
    addQuote(a, QDate(2026, 8, 1), 10000);
    addQuote(a, QDate(2026, 8, 2), 10000);
    addQuote(b, QDate(2026, 8, 3), 10000);

    QuoteFilter f;
    f.customerId = a;
    QCOMPARE(RepoQuotes(m_db).list(f).size(), 2);
}

void TestArchive::listFiltersByStatus()
{
    RepoQuotes repo(m_db);
    const qint64 c = addCustomer(QStringLiteral("Musteri"));
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
    const qint64 c = addCustomer(QStringLiteral("Musteri"));
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
    const qint64 a = addCustomer(QStringLiteral("Şükrü Çelik"));
    const qint64 b = addCustomer(QStringLiteral("Mehmet Yılmaz"));
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
    const qint64 c = addCustomer(QStringLiteral("Musteri"));
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
    const qint64 c = addCustomer(QStringLiteral("Musteri"));
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
    const qint64 c = addCustomer(QStringLiteral("Musteri"));
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
    const qint64 c = addCustomer(QStringLiteral("Musteri"));
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
    QCOMPARE(kopya->customerId, orijinal->customerId);
    QCOMPARE(kopya->projeBasligi, orijinal->projeBasligi);
    QCOMPARE(kopya->satirlar.size(), orijinal->satirlar.size());
    QCOMPARE(kopya->satirlar.first().birimFiyat, orijinal->satirlar.first().birimFiyat);
    // Satir kimlikleri kaynaktan kopyalanmamali.
    QVERIFY(kopya->satirlar.first().id != orijinal->satirlar.first().id);
}

void TestArchive::duplicateDoesNotTouchOriginal()
{
    RepoQuotes repo(m_db);
    const qint64 c = addCustomer(QStringLiteral("Musteri"));
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

void TestArchive::customerTotalSumsQuotes()
{
    RepoQuotes repo(m_db);
    const qint64 a = addCustomer(QStringLiteral("A"));
    const qint64 b = addCustomer(QStringLiteral("B"));
    addQuote(a, QDate(2026, 8, 1), 10000);
    addQuote(a, QDate(2026, 8, 2), 25050);
    addQuote(b, QDate(2026, 8, 3), 99900);

    QCOMPARE(repo.customerTotal(a).toString(), QStringLiteral("350,50"));
    QCOMPARE(repo.customerTotal(b).toString(), QStringLiteral("999,00"));
    // Teklifi olmayan musteri: 0, hata degil.
    QCOMPARE(repo.customerTotal(9999).toString(), QStringLiteral("0,00"));
}

void TestArchive::removeDeletesQuoteAndItsLines()
{
    RepoQuotes repo(m_db);
    const qint64 c = addCustomer(QStringLiteral("Musteri"));
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
