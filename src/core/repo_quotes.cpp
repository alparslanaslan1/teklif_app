#include "repo_quotes.h"

#include <QSqlError>
#include <QSqlQuery>

namespace {


// ═══ rollback() ═══════════════════════════════════════════════════════════
// NE YAPAR : Açık transaction'ı geri alır. Bu dosyadaki her hata dalında
//            çağrıldığı için ayrı bir yardımcıya alınmış.
//
// DEBUG    : Dönüş değeri çağıranlar tarafından KULLANILMIYOR. ROLLBACK'in
//            kendisi başarısız olursa (ör. açık transaction yoksa) bu sessizce
//            geçilir. Şüphede:
//              if (!rb.exec(...)) qDebug() << "ROLLBACK FAIL:" << rb.lastError().text();
//            "cannot rollback - no transaction is active" mesajı, eşleşen
//            BEGIN'in hiç çalışmadığını gösterir.
bool rollback(QSqlDatabase &db)
{
    QSqlQuery rb(db);
    return rb.exec(QStringLiteral("ROLLBACK"));
}

} // namespace


// ═══ RepoQuotes::nextQuoteNumberLocked() ══════════════════════════════════
// NE YAPAR : Sıradaki teklif numarasını üretir ve sayacı artırır.
//            "000001", "000002", ... biçiminde 6 haneli sıfır dolgulu.
//
// ÖN KOŞUL — İSMİNDEKİ "Locked" BUNU ANLATIR:
//   Çağrıldığı yerde ZATEN AÇIK bir transaction (BEGIN IMMEDIATE) olmalıdır.
//   Okuma ve yazma AYNI transaction içinde olduğu için iki eşzamanlı kayıt
//   asla aynı numarayı alamaz. Transaction olmadan çağırırsanız numara
//   çakışması OLUR ve teklif_no UNIQUE olduğu için INSERT patlar.
//
// ADIM ADIM:
//   1) settings tablosundan 'teklif_no_sayac' okunur.
//      Kayıt YOKSA (ilk teklif) mevcut = 0 kabul edilir — hata değildir.
//   2) sonraki = mevcut + 1
//   3) UPSERT: INSERT ... ON CONFLICT(key) DO UPDATE SET value = :v
//      Yani kayıt yoksa oluşturur, varsa günceller. Tek ifadede iki iş.
//   4) rightJustified(6, '0') ile sıfır dolgulanır.
//      ÜST SINIR YOKTUR: 999999'dan sonra kendiliğinden 7 haneye taşar
//      ("1000000"). Bu kasıtlıdır, çökme olmaz.
//
// DEBUG    : Numara beklenmedik geliyorsa sayacın DB'deki halini okuyun:
//              SELECT value FROM settings WHERE key='teklif_no_sayac';
//            Kod içinde:  qDebug() << mevcut << "->" << sonraki;
//            • Her seferinde "000001" geliyorsa 3. adımdaki yazma commit
//              edilmiyor demektir (dıştaki transaction ROLLBACK oluyor).
//            • Numara atlıyorsa: teklif kaydı BAŞARISIZ olup ROLLBACK
//              edilmiştir; sayaç da geri alınır, yani atlama OLMAMALI.
//              Atlama görüyorsanız transaction sınırları bozulmuştur.
//
// NOT      : Sayaç YIL BAZINDA SIFIRLANMAZ, sürekli artar (tasarım kararı;
//            testi: sequenceSurvivesAcrossYearBoundary).
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


// ═══ RepoQuotes::insertLines() ════════════════════════════════════════════
// NE YAPAR : Bir teklifin tüm satırlarını quote_lines tablosuna yazar.
//            Hem add() hem update() bunu kullanır.
//
// ADIM ADIM:
//   1) INSERT BİR KEZ hazırlanır (prepare döngünün DIŞINDA) — doğru kalıp
//      budur; her turda sadece bindValue yapılır. (RepoItems::importCsv bunu
//      yapmıyor, karşılaştırmak için bakabilirsiniz.)
//   2) Her satır için değerler bind edilip exec() edilir.
//      • miktar     -> REAL (double olarak yazılır)
//      • birimFiyat -> .kurus() ile tam sayı
//      • tutar      -> .kurus(); ÖNCEDEN hesaplanmış değer yazılır,
//                      DB tarafında yeniden hesaplanmaz
//   3) Herhangi bir satır patlarsa hemen false döner; ÇAĞIRAN taraf
//      ROLLBACK etmekle yükümlüdür (bu fonksiyon rollback YAPMAZ).
//
// ÖN KOŞUL : Açık bir transaction içinde çağrılmalıdır.
//
// DEBUG    : Satırlar kaydolmuyorsa:
//              qDebug() << "quoteId" << quoteId << "satır" << lines.size();
//            • quoteId == 0 ise çağıran taraftaki lastInsertId() boş dönmüştür
//            • "FOREIGN KEY constraint failed" -> quoteId quotes tablosunda YOK
//              (update() sırasında var olmayan bir teklif id'si verilmiş olabilir)
//            Tek satırın hangisinde patladığını görmek için döngüye:
//              qDebug() << l.sira << l.aciklama << q.lastError().text();
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


// ═══ RepoQuotes::add() ════════════════════════════════════════════════════
// NE YAPAR : Yeni teklifi BAŞLIK + TÜM SATIRLARI + NUMARA ATAMA ile birlikte
//            TEK bir transaction içinde kaydeder. Başarılıysa quote.id ve
//            quote.teklifNo doldurulur (parametre bu yüzden referanstır).
//
// ADIM ADIM (5 aşama — hata ayıklarken sırayla ilerleyin):
//   [1] BEGIN IMMEDIATE
//       Dönüş değeri KONTROL EDİLMİYOR (bu dosyadaki bilinen zayıf nokta).
//   [2] nextQuoteNumberLocked() -> teklif no üretilir ve sayaç artırılır.
//       Boş dönerse ROLLBACK + çık.                          [ÇIKIŞ 1]
//       Numara üretimi AYNI transaction'da olduğu için, aşağıdaki adımlardan
//       biri patlarsa sayaç da geri alınır — numara boşa harcanmaz.
//   [3] quotes tablosuna başlık INSERT edilir.
//       tarih QDate -> Qt::ISODate metni ("2026-08-25") olarak yazılır.
//       Patlarsa ROLLBACK + çık.                             [ÇIKIŞ 2]
//   [4] quoteId = lastInsertId(); insertLines() ile satırlar yazılır.
//       Patlarsa ROLLBACK + çık.                             [ÇIKIŞ 3]
//   [5] COMMIT. Ancak COMMIT'ten SONRA quote.id ve quote.teklifNo atanır —
//       yani bu alanlar SADECE kalıcı olarak yazıldıysa dolar.
//
// DEBUG    : Kayıt olmuyorsa aşamaları işaretleyin:
//              qDebug() << "[2] no:" << teklifNo;
//              qDebug() << "[3]" << ins.lastError().text();
//              qDebug() << "[4] id:" << quoteId;
//            Sık görülen hatalar:
//            • "FOREIGN KEY constraint failed" -> customerId geçersiz ya da 0.
//              customers tablosunda o id yok. En sık sebep: müşteri seçilmemiş.
//            • "UNIQUE constraint failed: quotes.teklif_no" -> numara üretimi
//              transaction dışında çalışmış (bkz. nextQuoteNumberLocked ön koşulu)
//            • "NOT NULL constraint failed: quotes.tarih" -> quote.tarih
//              GEÇERSİZ bir QDate. toString(ISODate) boş string döndürmüştür.
//              Kontrol:  qDebug() << quote.tarih.isValid();
//
// EKSİK    : araToplam / kdvTutari / genelToplam çağırandan OLDUĞU GİBİ alınır,
//            satırlarla tutarlı mı diye BAKILMAZ. Arayüzde bir hata olursa
//            veritabanına tutarsız teklif yazılır ve bir daha fark edilmez.
//            Sağlamlaştırmak için burada Calculator::totals ile yeniden
//            hesaplamak gerekir.
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


// ═══ RepoQuotes::update() ═════════════════════════════════════════════════
// NE YAPAR : Var olan teklifi günceller. Başlık alanları UPDATE edilir,
//            SATIRLAR ise TAMAMEN SİLİNİP YENİDEN EKLENİR.
//
// NEDEN SİL-YENİDEN EKLE: "Hangi eski satır hangi yeni satırla eşleşiyor"
//   sorusu hiç ortaya çıkmaz. Kullanıcı satır sildi, ekledi ya da sırayı
//   değiştirdi — hiçbiri fark etmez. Basit ve her durumda doğru.
//   YAN ETKİSİ: quote_lines.id değerleri her kaydetmede DEĞİŞİR. O id'lere
//   dışarıdan referans veren bir kod yazmayın.
//
// ADIM ADIM:
//   [1] BEGIN IMMEDIATE (dönüş değeri kontrol edilmiyor)
//   [2] quotes UPDATE ... WHERE id = :id  (+ guncelleme damgası)
//       Patlarsa ROLLBACK + çık.                             [ÇIKIŞ 1]
//   [3] DELETE FROM quote_lines WHERE quote_id = :id
//       Patlarsa ROLLBACK + çık.                             [ÇIKIŞ 2]
//   [4] insertLines() ile yeni satırlar yazılır.
//       Patlarsa ROLLBACK + çık.                             [ÇIKIŞ 3]
//   [5] COMMIT
//
// ÖNEMLİ TUZAK — SESSİZ BAŞARISIZLIK:
//   [2] adımı numRowsAffected() KONTROL ETMEZ. Var olmayan bir quote.id
//   verirseniz UPDATE 0 satır etkiler, DELETE 0 satır siler ve akış devam eder.
//   • Teklifin satırı VARSA [4] adımı FK ihlaliyle patlar (foreign_keys ON ise)
//     ve doğru şekilde hata döner.
//   • Ama teklifin HİÇ SATIRI YOKSA hiçbir şey patlamaz ve fonksiyon TRUE
//     döner — hiçbir şey yazılmadığı halde "kaydedildi" sanılır.
//   Teşhis:
//              upd.exec(); qDebug() << "etkilenen:" << upd.numRowsAffected();
//   0 görüyorsanız quote.id yanlış demektir.
//
// DEBUG    : Değişiklikler kayboluyorsa önce doğru id'yi güncellediğinizden
//            emin olun:  qDebug() << quote.id << quote.satirlar.size();
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


// ═══ RepoQuotes::get() ════════════════════════════════════════════════════
// NE YAPAR : Teklifi BAŞLIĞI + TÜM SATIRLARIYLA (sira'ya göre sıralı) okur.
//            Bulunamazsa veya hata olursa std::nullopt.
//
// ADIM ADIM:
//   [1] quotes tablosundan başlık okunur.
//       exec() başarısız VEYA next() satır vermezse nullopt döner.
//       DİKKAT: bu iki durum AYIRT EDİLMEZ. Hata metni boşsa
//       "Teklif bulunamadı.", doluysa SQL hatası yazılır — yani mesaja
//       bakarak hangisi olduğunu anlayabilirsiniz.        [ÇIKIŞ 1]
//   [2] Alanlar Quote'a doldurulur:
//       • *_toplam / *_tutari / birim_fiyat sütunları KURUŞ tam sayısıdır
//         -> doğrudan Money(...)
//       • tarih metinden QDate::fromString(..., Qt::ISODate) ile çözülür.
//         GEÇERLİLİK KONTROL EDİLMEZ; bozuk bir metin sessizce geçersiz
//         QDate üretir.  Kontrol:  qDebug() << quote.tarih.isValid();
//   [3] quote_lines ayrı bir sorguyla ORDER BY sira ile okunur ve
//       quote.satirlar'a doldurulur.
//
// ÖNEMLİ TUZAK — SESSİZ SATIR KAYBI:
//   [3] adımı `if (lq.exec())` içine alınmış ama ELSE dalı YOK. Satır sorgusu
//   başarısız olursa hata HİÇ BİLDİRİLMEZ; teklif 0 satırla döner ve
//   kullanıcı teklifini boş görür.
//   Teşhis için geçici olarak:
//              if (!lq.exec()) qDebug() << "SATIR SORGUSU FAIL:" << lq.lastError().text();
//              else qDebug() << "okunan satır:" << quote.satirlar.size();
//   Teklif açıldığında tablo boşsa İLK bakılacak yer burasıdır — ikinci
//   ihtimal, teklifin gerçekten satırsız kaydedilmiş olmasıdır:
//              SELECT COUNT(*) FROM quote_lines WHERE quote_id = <id>;
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
