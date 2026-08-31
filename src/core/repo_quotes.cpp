#include "repo_quotes.h"

#include "quote_status.h"
#include "settings.h"
#include "transaction.h"
#include "turkish.h"

#include <QSqlError>
#include <QSqlQuery>

#include <utility>

RepoQuotes::RepoQuotes(QSqlDatabase db) : m_db(std::move(db)) {}


// Sıradaki teklif numarasını üretir ve sayacı artırır ("000001", "000002" ...).
// İsmindeki "Locked", çağrıldığı yerde ZATEN açık bir transaction bulunması
// gerektiğini anlatır: sayacın okunması ve yazılması aynı transaction içinde
// olduğu için art arda iki kayıt asla aynı numarayı alamaz.
//   mevcut  : settings tablosundaki sayacın son değeri. Kayıt yoksa (ilk
//             teklif) 0 kabul edilir; bu bir hata değildir.
//   sonraki : mevcut + 1. UPSERT ile settings'e geri yazılır — kayıt yoksa
//             oluşturulur, varsa güncellenir.
// Numara sürekli artar, yıl bazında sıfırlanmaz. 999999'dan sonra kendiliğinden
// 7 haneye taşar; rightJustified minimumu garanti eder, üst sınır koymaz.
QString RepoQuotes::nextQuoteNumberLocked(QString *errorOut)
{
    // settings tablosuna doğrudan SQL yazmak yerine Settings üzerinden gidilir:
    // anahtar adı tek yerde tanımlıdır ve teklif deposunun ayarlar şemasını
    // bilmesi gerekmez.
    Settings settings(m_db);

    QString okuErr;
    const auto mevcutMetin = settings.value(Settings::keyQuoteCounter(), &okuErr);
    if (!okuErr.isEmpty()) {
        if (errorOut)
            *errorOut = okuErr;
        return QString();
    }

    // Anahtar yoksa bu ilk tekliftir; sayaç 0'dan başlar.
    bool ok = true;
    const qint64 mevcut = mevcutMetin.has_value() ? mevcutMetin->toLongLong(&ok) : 0;
    if (!ok) {
        if (errorOut)
            *errorOut = QStringLiteral("Teklif no sayacı bozuk: %1").arg(*mevcutMetin);
        return QString();
    }

    const qint64 sonraki = mevcut + 1;
    if (!settings.setInt(Settings::keyQuoteCounter(), sonraki, errorOut))
        return QString();

    // Hane sayısı ayarlanabilir (varsayılan 6). Ayar bozuk ya da aralık
    // dışıysa varsayılana düşülür — numara üretimi hiçbir koşulda
    // başarısız olmamalı.
    // Sayaç sınırı AŞARSA numara kendiliğinden uzar: rightJustified minimumu
    // garanti eder, üst sınır koymaz. Yani hane sayısını küçültmek eski
    // teklifleri bozmaz, onlar zaten metin olarak saklanıyor.
    const qint64 ayarlanan =
        settings.intValueOr(Settings::keyQuoteNoDigits(), kDefaultQuoteNoDigits);
    const int hane = static_cast<int>(qBound(static_cast<qint64>(kMinQuoteNoDigits), ayarlanan,
                                              static_cast<qint64>(kMaxQuoteNoDigits)));

    return QString::number(sonraki).rightJustified(hane, QLatin1Char('0'));
}

bool RepoQuotes::insertLines(qint64 quoteId, const QVector<QuoteLine> &lines,
                              QString *errorOut)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO quote_lines (quote_id, sira, aciklama, birim, miktar, birim_fiyat, satir_notu, tutar) "
        "VALUES (:qid, :sira, :aciklama, :birim, :miktar, :fiyat, :notu, :tutar)"));

    for (const QuoteLine &l : lines) {
        q.bindValue(QStringLiteral(":qid"), quoteId);
        q.bindValue(QStringLiteral(":sira"), l.sira);
        q.bindValue(QStringLiteral(":aciklama"), l.aciklama);
        q.bindValue(QStringLiteral(":birim"), l.birim);
        q.bindValue(QStringLiteral(":miktar"), l.miktar);
        q.bindValue(QStringLiteral(":fiyat"), l.birimFiyat.kurus());
        q.bindValue(QStringLiteral(":notu"), l.satirNotu);
        q.bindValue(QStringLiteral(":tutar"), l.tutar.kurus());
        if (!q.exec()) {
            if (errorOut)
                *errorOut = q.lastError().text();
            return false;
        }
    }
    return true;
}


// Yeni teklifi başlığı, tüm satırları ve numara atamasıyla birlikte TEK bir
// transaction içinde kaydeder. Başarılıysa quote.id ve quote.teklifNo
// doldurulur — parametre bu yüzden referanstır.
//   teklifNo : numara üretimi de aynı transaction'ın parçasıdır; sonraki
//              adımlardan biri başarısız olursa sayaç da geri alınır ve numara
//              boşa harcanmaz
//   quoteId  : başlık INSERT'inden dönen id; satırlar buna bağlanır
// tarih, QDate'ten Qt::ISODate metnine ("2026-08-25") çevrilerek yazılır.
// quote.id ve quote.teklifNo COMMIT'ten SONRA atanır, yani bu alanlar yalnızca
// kayıt gerçekten kalıcı olduysa dolar.
bool RepoQuotes::add(Quote &quote, QString *errorOut)
{
    // Transaction RAII: aşağıdaki her erken return otomatik ROLLBACK eder.
    Transaction tx(m_db);
    if (!tx.isActive()) {
        if (errorOut)
            *errorOut = tx.lastError();
        return false;
    }

    // Numara üretimi de aynı transaction'ın parçasıdır: sonraki adımlardan
    // biri başarısız olursa sayaç da geri alınır, numara boşa harcanmaz.
    const QString teklifNo = nextQuoteNumberLocked(errorOut);
    if (teklifNo.isEmpty())
        return false;

    QSqlQuery ins(m_db);
    ins.prepare(QStringLiteral(
        "INSERT INTO quotes (teklif_no, customer_id, tarih, gecerlilik_gun, proje_basligi, proje_notu, "
        "durum, sartlar_metni, ara_toplam, kdv_orani, kdv_tutari, genel_toplam) "
        "VALUES (:no, :cust, :tarih, :gecerlilik, :baslik, :notu, :durum, :sartlar, :ara, :kdvo, :kdvt, :genel)"));
    ins.bindValue(QStringLiteral(":no"), teklifNo);
    ins.bindValue(QStringLiteral(":cust"), quote.customerId);
    ins.bindValue(QStringLiteral(":tarih"), quote.tarih.toString(Qt::ISODate));
    ins.bindValue(QStringLiteral(":gecerlilik"), quote.gecerlilikGun);
    ins.bindValue(QStringLiteral(":baslik"), quote.projeBasligi);
    ins.bindValue(QStringLiteral(":notu"), quote.projeNotu);
    ins.bindValue(QStringLiteral(":durum"), quote.durum);
    ins.bindValue(QStringLiteral(":sartlar"), quote.sartlarMetni);
    ins.bindValue(QStringLiteral(":ara"), quote.araToplam.kurus());
    ins.bindValue(QStringLiteral(":kdvo"), quote.kdvOraniYuzde);
    ins.bindValue(QStringLiteral(":kdvt"), quote.kdvTutari.kurus());
    ins.bindValue(QStringLiteral(":genel"), quote.genelToplam.kurus());

    if (!ins.exec()) {
        if (errorOut)
            *errorOut = ins.lastError().text();
        return false;
    }

    const qint64 quoteId = ins.lastInsertId().toLongLong();

    if (!insertLines(quoteId, quote.satirlar, errorOut))
        return false;

    if (!tx.commit(errorOut))
        return false;

    // Alanlar yalnızca kayıt gerçekten kalıcı olduysa doldurulur.
    quote.id = quoteId;
    quote.teklifNo = teklifNo;
    return true;
}

bool RepoQuotes::update(const Quote &quote, QString *errorOut)
{
    Transaction tx(m_db);
    if (!tx.isActive()) {
        if (errorOut)
            *errorOut = tx.lastError();
        return false;
    }

    QSqlQuery upd(m_db);
    upd.prepare(QStringLiteral(
        "UPDATE quotes SET customer_id=:cust, tarih=:tarih, gecerlilik_gun=:gecerlilik, "
        "proje_basligi=:baslik, proje_notu=:notu, durum=:durum, sartlar_metni=:sartlar, "
        "ara_toplam=:ara, kdv_orani=:kdvo, kdv_tutari=:kdvt, genel_toplam=:genel, "
        "guncelleme=datetime('now') WHERE id=:id"));
    upd.bindValue(QStringLiteral(":cust"), quote.customerId);
    upd.bindValue(QStringLiteral(":tarih"), quote.tarih.toString(Qt::ISODate));
    upd.bindValue(QStringLiteral(":gecerlilik"), quote.gecerlilikGun);
    upd.bindValue(QStringLiteral(":baslik"), quote.projeBasligi);
    upd.bindValue(QStringLiteral(":notu"), quote.projeNotu);
    upd.bindValue(QStringLiteral(":durum"), quote.durum);
    upd.bindValue(QStringLiteral(":sartlar"), quote.sartlarMetni);
    upd.bindValue(QStringLiteral(":ara"), quote.araToplam.kurus());
    upd.bindValue(QStringLiteral(":kdvo"), quote.kdvOraniYuzde);
    upd.bindValue(QStringLiteral(":kdvt"), quote.kdvTutari.kurus());
    upd.bindValue(QStringLiteral(":genel"), quote.genelToplam.kurus());
    upd.bindValue(QStringLiteral(":id"), quote.id);

    if (!upd.exec()) {
        if (errorOut)
            *errorOut = upd.lastError().text();
        return false;
    }

    // Var olmayan bir id sessizce "başarılı" sayılmamalı: UPDATE 0 satır
    // etkiler, DELETE 0 satır siler ve satırsız bir teklifte hiçbir şey
    // patlamadan true dönerdi.
    if (upd.numRowsAffected() == 0) {
        if (errorOut)
            *errorOut = QStringLiteral("Güncellenecek teklif bulunamadı (id %1).").arg(quote.id);
        return false;
    }

    QSqlQuery del(m_db);
    del.prepare(QStringLiteral("DELETE FROM quote_lines WHERE quote_id=:id"));
    del.bindValue(QStringLiteral(":id"), quote.id);
    if (!del.exec()) {
        if (errorOut)
            *errorOut = del.lastError().text();
        return false;
    }

    if (!insertLines(quote.id, quote.satirlar, errorOut))
        return false;

    return tx.commit(errorOut);
}

std::optional<Quote> RepoQuotes::get(qint64 id, QString *errorOut) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM quotes WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    if (!q.exec() || !q.next()) {
        if (errorOut)
            *errorOut = q.lastError().text().isEmpty() ? QStringLiteral("Teklif bulunamadı.")
                                                         : q.lastError().text();
        return std::nullopt;
    }

    Quote quote;
    quote.id = q.value(QStringLiteral("id")).toLongLong();
    quote.teklifNo = q.value(QStringLiteral("teklif_no")).toString();
    quote.customerId = q.value(QStringLiteral("customer_id")).toLongLong();
    quote.tarih = QDate::fromString(q.value(QStringLiteral("tarih")).toString(), Qt::ISODate);
    quote.gecerlilikGun = q.value(QStringLiteral("gecerlilik_gun")).toInt();
    quote.projeBasligi = q.value(QStringLiteral("proje_basligi")).toString();
    quote.projeNotu = q.value(QStringLiteral("proje_notu")).toString();
    quote.durum = q.value(QStringLiteral("durum")).toString();
    quote.sartlarMetni = q.value(QStringLiteral("sartlar_metni")).toString();
    quote.araToplam = Money(q.value(QStringLiteral("ara_toplam")).toLongLong());
    quote.kdvOraniYuzde = q.value(QStringLiteral("kdv_orani")).toInt();
    quote.kdvTutari = Money(q.value(QStringLiteral("kdv_tutari")).toLongLong());
    quote.genelToplam = Money(q.value(QStringLiteral("genel_toplam")).toLongLong());

    QSqlQuery lq(m_db);
    lq.prepare(QStringLiteral("SELECT * FROM quote_lines WHERE quote_id = :id ORDER BY sira"));
    lq.bindValue(QStringLiteral(":id"), id);
    // Satır sorgusu başarısız olursa teklifi SATIRSIZ döndürmek, kullanıcıya
    // "teklifiniz boş" demek olurdu. Hata artık bildiriliyor.
    if (!lq.exec()) {
        if (errorOut)
            *errorOut = lq.lastError().text();
        return std::nullopt;
    }
    {
        while (lq.next()) {
            QuoteLine l;
            l.id = lq.value(QStringLiteral("id")).toLongLong();
            l.sira = lq.value(QStringLiteral("sira")).toInt();
            l.aciklama = lq.value(QStringLiteral("aciklama")).toString();
            l.birim = lq.value(QStringLiteral("birim")).toString();
            l.miktar = lq.value(QStringLiteral("miktar")).toDouble();
            l.birimFiyat = Money(lq.value(QStringLiteral("birim_fiyat")).toLongLong());
            l.satirNotu = lq.value(QStringLiteral("satir_notu")).toString();
            l.tutar = Money(lq.value(QStringLiteral("tutar")).toLongLong());
            quote.satirlar.append(l);
        }
    }

    return quote;
}

QVector<QuoteSummary> RepoQuotes::list(const QuoteFilter &filtre, QString *errorOut) const
{
    QVector<QuoteSummary> sonuc;

    // Müşteri unvanı JOIN ile gelir: liste satırı başına ayrı sorgu atmak
    // (N+1) 500 teklifte 501 sorgu demekti.
    // LEFT JOIN, INNER değil — müşterisi bir şekilde kaybolmuş bir teklif
    // listeden düşmemeli, unvansız görünmeli.
    QString sql = QStringLiteral(
        "SELECT q.id, q.teklif_no, q.customer_id, q.tarih, q.durum, q.genel_toplam, "
        "       q.proje_basligi, c.unvan AS musteri_unvan "
        "FROM quotes q LEFT JOIN customers c ON c.id = q.customer_id WHERE 1=1");

    if (filtre.customerId != 0)
        sql += QStringLiteral(" AND q.customer_id = :cust");
    // tarih ISO metni ("2026-08-25") olarak saklandığı için metin
    // karşılaştırması kronolojik sırayla birebir örtüşür; ayrı bir tarih
    // dönüşümüne gerek yoktur. Sınırlar DAHİLDİR (>=, <=).
    if (filtre.tarihBaslangic.isValid())
        sql += QStringLiteral(" AND q.tarih >= :bas");
    if (filtre.tarihBitis.isValid())
        sql += QStringLiteral(" AND q.tarih <= :bit");
    if (!filtre.durum.isEmpty())
        sql += QStringLiteral(" AND q.durum = :durum");

    // En yeni önce; aynı tarihte birden fazla teklif varsa numaraya göre.
    sql += QStringLiteral(" ORDER BY q.tarih DESC, q.teklif_no DESC");

    QSqlQuery q(m_db);
    q.prepare(sql);
    if (filtre.customerId != 0)
        q.bindValue(QStringLiteral(":cust"), filtre.customerId);
    if (filtre.tarihBaslangic.isValid())
        q.bindValue(QStringLiteral(":bas"), filtre.tarihBaslangic.toString(Qt::ISODate));
    if (filtre.tarihBitis.isValid())
        q.bindValue(QStringLiteral(":bit"), filtre.tarihBitis.toString(Qt::ISODate));
    if (!filtre.durum.isEmpty())
        q.bindValue(QStringLiteral(":durum"), filtre.durum);

    if (!q.exec()) {
        if (errorOut)
            *errorOut = q.lastError().text();
        return sonuc;
    }

    // Metin araması SQL'de DEĞİL burada: LIKE Türkçe harfleri katlayamaz,
    // "sukru" yazan kullanıcı "Şükrü"yü bulamazdı (bkz. core/turkish.h).
    const QString anahtar = turkishSearchNormalize(filtre.aranan.trimmed());

    while (q.next()) {
        QuoteSummary s;
        s.id = q.value(QStringLiteral("id")).toLongLong();
        s.teklifNo = q.value(QStringLiteral("teklif_no")).toString();
        s.customerId = q.value(QStringLiteral("customer_id")).toLongLong();
        s.customerUnvan = q.value(QStringLiteral("musteri_unvan")).toString();
        s.tarih = QDate::fromString(q.value(QStringLiteral("tarih")).toString(), Qt::ISODate);
        s.durum = q.value(QStringLiteral("durum")).toString();
        s.genelToplam = Money(q.value(QStringLiteral("genel_toplam")).toLongLong());

        if (!anahtar.isEmpty()) {
            const QString alan = turkishSearchNormalize(
                s.teklifNo + QLatin1Char(' ') + s.customerUnvan + QLatin1Char(' ')
                + q.value(QStringLiteral("proje_basligi")).toString());
            if (!alan.contains(anahtar))
                continue;
        }
        sonuc.append(s);
    }
    return sonuc;
}

bool RepoQuotes::setStatus(qint64 id, const QString &durum, QString *errorOut)
{
    // Bilinmeyen bir durum metni veritabanına girerse arşiv filtresi onu
    // hiçbir zaman göstermez; yazarken doğrulamak tek koruma.
    if (!QuoteStatus::isValid(durum)) {
        if (errorOut)
            *errorOut = QStringLiteral("Geçersiz teklif durumu: %1").arg(durum);
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE quotes SET durum=:durum, guncelleme=datetime('now') WHERE id=:id"));
    q.bindValue(QStringLiteral(":durum"), durum);
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec()) {
        if (errorOut)
            *errorOut = q.lastError().text();
        return false;
    }
    if (q.numRowsAffected() == 0) {
        if (errorOut)
            *errorOut = QStringLiteral("Teklif bulunamadı (id %1).").arg(id);
        return false;
    }
    return true;
}

std::optional<Quote> RepoQuotes::duplicate(qint64 id, QString *errorOut)
{
    const auto kaynak = get(id, errorOut);
    if (!kaynak.has_value())
        return std::nullopt;

    Quote kopya = kaynak.value();

    // Kopya YENİ bir belgedir: kimliği, numarası ve tarihi kendisinin olur.
    // add() içinde numara sayaçla aynı transaction'da atanır.
    kopya.id = 0;
    kopya.teklifNo.clear();
    kopya.tarih = QDate::currentDate();
    kopya.durum = QuoteStatus::taslak();

    // Satır kimlikleri de sıfırlanır; aksi halde insertLines kaynak satırların
    // id'leriyle yazmaya çalışırdı.
    for (QuoteLine &l : kopya.satirlar)
        l.id = 0;

    if (!add(kopya, errorOut))
        return std::nullopt;

    return kopya;
}

Money RepoQuotes::customerTotal(qint64 customerId, QString *errorOut) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT COALESCE(SUM(genel_toplam), 0) FROM quotes WHERE customer_id = :id"));
    q.bindValue(QStringLiteral(":id"), customerId);

    if (!q.exec() || !q.next()) {
        if (errorOut)
            *errorOut = q.lastError().text();
        return Money(0);
    }
    return Money(q.value(0).toLongLong());
}
