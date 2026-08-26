#include "repo_quotes.h"

#include <QSqlError>
#include <QSqlQuery>

namespace {

bool rollback(QSqlDatabase &db)
{
    QSqlQuery rb(db);
    return rb.exec(QStringLiteral("ROLLBACK"));
}

} // namespace

QString RepoQuotes::nextQuoteNumberLocked(QSqlDatabase &db, QString *errorOut)
{
    QSqlQuery oku(db);
    oku.prepare(QStringLiteral("SELECT value FROM settings WHERE key = 'teklif_no_sayac'"));
    if (!oku.exec()) {
        if (errorOut)
            *errorOut = oku.lastError().text();
        return QString();
    }

    qint64 mevcut = 0;
    if (oku.next())
        mevcut = oku.value(0).toLongLong();

    const qint64 sonraki = mevcut + 1;

    QSqlQuery yaz(db);
    yaz.prepare(QStringLiteral("INSERT INTO settings (key, value) VALUES ('teklif_no_sayac', :v) "
                                "ON CONFLICT(key) DO UPDATE SET value = :v"));
    yaz.bindValue(QStringLiteral(":v"), QString::number(sonraki));
    if (!yaz.exec()) {
        if (errorOut)
            *errorOut = yaz.lastError().text();
        return QString();
    }

    // 6 haneli sıfır dolgulu; 999.999'u aşınca kendiliğinden 7 haneye
    // taşar (rightJustified minimumu garanti eder, üst sınır koymaz).
    return QString::number(sonraki).rightJustified(6, QLatin1Char('0'));
}

bool RepoQuotes::insertLines(QSqlDatabase &db, qint64 quoteId, const QVector<QuoteLine> &lines,
                              QString *errorOut)
{
    QSqlQuery q(db);
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

bool RepoQuotes::add(QSqlDatabase &db, Quote &quote, QString *errorOut)
{
    QSqlQuery tx(db);
    tx.exec(QStringLiteral("BEGIN IMMEDIATE"));

    QString numErr;
    const QString teklifNo = nextQuoteNumberLocked(db, &numErr);
    if (teklifNo.isEmpty()) {
        rollback(db);
        if (errorOut)
            *errorOut = numErr;
        return false;
    }

    QSqlQuery ins(db);
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
        rollback(db);
        if (errorOut)
            *errorOut = ins.lastError().text();
        return false;
    }

    const qint64 quoteId = ins.lastInsertId().toLongLong();

    QString lineErr;
    if (!insertLines(db, quoteId, quote.satirlar, &lineErr)) {
        rollback(db);
        if (errorOut)
            *errorOut = lineErr;
        return false;
    }

    QSqlQuery commit(db);
    if (!commit.exec(QStringLiteral("COMMIT"))) {
        if (errorOut)
            *errorOut = commit.lastError().text();
        return false;
    }

    quote.id = quoteId;
    quote.teklifNo = teklifNo;
    return true;
}

bool RepoQuotes::update(QSqlDatabase &db, const Quote &quote, QString *errorOut)
{
    QSqlQuery tx(db);
    tx.exec(QStringLiteral("BEGIN IMMEDIATE"));

    QSqlQuery upd(db);
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
        rollback(db);
        if (errorOut)
            *errorOut = upd.lastError().text();
        return false;
    }

    QSqlQuery del(db);
    del.prepare(QStringLiteral("DELETE FROM quote_lines WHERE quote_id=:id"));
    del.bindValue(QStringLiteral(":id"), quote.id);
    if (!del.exec()) {
        rollback(db);
        if (errorOut)
            *errorOut = del.lastError().text();
        return false;
    }

    QString lineErr;
    if (!insertLines(db, quote.id, quote.satirlar, &lineErr)) {
        rollback(db);
        if (errorOut)
            *errorOut = lineErr;
        return false;
    }

    QSqlQuery commit(db);
    if (!commit.exec(QStringLiteral("COMMIT"))) {
        if (errorOut)
            *errorOut = commit.lastError().text();
        return false;
    }
    return true;
}

std::optional<Quote> RepoQuotes::get(QSqlDatabase &db, qint64 id, QString *errorOut)
{
    QSqlQuery q(db);
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

    QSqlQuery lq(db);
    lq.prepare(QStringLiteral("SELECT * FROM quote_lines WHERE quote_id = :id ORDER BY sira"));
    lq.bindValue(QStringLiteral(":id"), id);
    if (lq.exec()) {
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
