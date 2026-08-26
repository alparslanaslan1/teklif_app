#include "db.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace {

bool execOrFail(QSqlQuery &q, const QString &sql, QString *errorOut)
{
    if (!q.exec(sql)) {
        if (errorOut)
            *errorOut = q.lastError().text();
        return false;
    }
    return true;
}

} // namespace

QString Db::defaultPath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) {
        // QCoreApplication::setOrganizationName/setApplicationName henüz
        // çağrılmamışsa Qt bu yolu türetemez. main.cpp'de bu ikisi mutlaka
        // QApplication oluşturulmadan hemen sonra ayarlanmalı.
        dir = QDir::homePath() + QStringLiteral("/.teklif");
    }
    return dir + QStringLiteral("/teklif.db");
}

int Db::currentVersion(QSqlDatabase &db)
{
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA user_version")) || !q.next())
        return 0;
    return q.value(0).toInt();
}

bool Db::setVersion(QSqlDatabase &db, int version)
{
    QSqlQuery q(db);
    // PRAGMA user_version bir tamsayı literalidir, parametre bind edilemez.
    return q.exec(QStringLiteral("PRAGMA user_version = %1").arg(version));
}

bool Db::backupFile(const QString &path, QString *errorOut)
{
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString backupPath = path + QStringLiteral(".bak-") + stamp;

    if (QFile::exists(backupPath)) {
        // Aynı saniyede iki migration denemesi gibi bir uç durum; üzerine
        // yazmak yerine reddet, sessizce eski yedeği kaybetme.
        if (errorOut)
            *errorOut = QStringLiteral("Yedek dosyası zaten var: %1").arg(backupPath);
        return false;
    }
    if (!QFile::copy(path, backupPath)) {
        if (errorOut)
            *errorOut = QStringLiteral("Yedek alınamadı: %1").arg(backupPath);
        return false;
    }
    return true;
}

bool Db::createV1Schema(QSqlDatabase &db, QString *errorOut)
{
    static const QStringList statements = {
        QStringLiteral(R"(
            CREATE TABLE settings (
                key   TEXT PRIMARY KEY,
                value TEXT
            )
        )"),
        QStringLiteral(R"(
            CREATE TABLE customers (
                id             INTEGER PRIMARY KEY,
                unvan          TEXT NOT NULL,
                yetkili        TEXT,
                telefon        TEXT,
                email          TEXT,
                adres          TEXT,
                vergi_dairesi  TEXT,
                vergi_no       TEXT,
                notlar         TEXT,
                aktif          INTEGER NOT NULL DEFAULT 1,
                olusturma      TEXT NOT NULL DEFAULT (datetime('now'))
            )
        )"),
        QStringLiteral(R"(
            CREATE TABLE categories (
                id INTEGER PRIMARY KEY,
                ad TEXT NOT NULL UNIQUE
            )
        )"),
        QStringLiteral(R"(
            CREATE TABLE items (
                id                INTEGER PRIMARY KEY,
                kod               TEXT NOT NULL UNIQUE,
                ad                TEXT NOT NULL,
                birim             TEXT NOT NULL,
                varsayilan_fiyat  INTEGER NOT NULL DEFAULT 0,
                category_id       INTEGER REFERENCES categories(id) ON DELETE SET NULL,
                aktif             INTEGER NOT NULL DEFAULT 1,
                guncelleme        TEXT NOT NULL DEFAULT (datetime('now'))
            )
        )"),
        QStringLiteral(R"(
            CREATE TABLE quotes (
                id              INTEGER PRIMARY KEY,
                teklif_no       TEXT NOT NULL UNIQUE,
                customer_id     INTEGER NOT NULL REFERENCES customers(id) ON DELETE RESTRICT,
                tarih           TEXT NOT NULL,
                gecerlilik_gun  INTEGER NOT NULL DEFAULT 15,
                proje_basligi   TEXT,
                proje_notu      TEXT,
                durum           TEXT NOT NULL DEFAULT 'Taslak',
                sartlar_metni   TEXT,
                ara_toplam      INTEGER NOT NULL DEFAULT 0,
                kdv_orani       INTEGER NOT NULL DEFAULT 0,
                kdv_tutari      INTEGER NOT NULL DEFAULT 0,
                genel_toplam    INTEGER NOT NULL DEFAULT 0,
                olusturma       TEXT NOT NULL DEFAULT (datetime('now')),
                guncelleme      TEXT NOT NULL DEFAULT (datetime('now'))
            )
        )"),
        QStringLiteral(R"(
            CREATE TABLE quote_lines (
                id           INTEGER PRIMARY KEY,
                quote_id     INTEGER NOT NULL REFERENCES quotes(id) ON DELETE CASCADE,
                sira         INTEGER NOT NULL,
                aciklama     TEXT NOT NULL,
                birim        TEXT NOT NULL,
                miktar       REAL NOT NULL,
                birim_fiyat  INTEGER NOT NULL,
                satir_notu   TEXT,
                tutar        INTEGER NOT NULL
            )
        )"),
        QStringLiteral("CREATE INDEX idx_quotes_customer ON quotes(customer_id)"),
        QStringLiteral("CREATE INDEX idx_quote_lines_quote ON quote_lines(quote_id)"),
        QStringLiteral("CREATE INDEX idx_items_category ON items(category_id)"),
    };

    QSqlQuery q(db);
    for (const QString &sql : statements) {
        if (!execOrFail(q, sql, errorOut))
            return false;
    }
    return true;
}

bool Db::migrateStep(QSqlDatabase &db, int fromVersion, QString *errorOut)
{
    switch (fromVersion) {
    case 0:
        return createV1Schema(db, errorOut);
    // v1 -> v2 gerektiğinde buraya yeni bir "case 1:" eklenecek.
    // Yukarıdaki case 0 SİLİNMEYECEK: kullanıcıda hâlâ v0 dosya olabilir.
    default:
        if (errorOut)
            *errorOut = QStringLiteral("Bilinmeyen şema sürümü: %1").arg(fromVersion);
        return false;
    }
}

bool Db::openAndMigrate(const QString &path, QString *errorOut, const QString &connectionName)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    const bool isNewFile = !QFile::exists(path);

    auto openConnection = [&]() -> bool {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(path);
        if (!db.open()) {
            if (errorOut)
                *errorOut = db.lastError().text();
            return false;
        }
        QSqlQuery pragma(db);
        // SQLite'ta varsayılan kapalıdır; ON DELETE CASCADE/RESTRICT bu
        // açılmadan hiçbir etki yapmaz. Her bağlantıda yeniden ayarlanmalı.
        pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
        return true;
    };

    if (!openConnection())
        return false;

    {
        QSqlDatabase db = QSqlDatabase::database(connectionName);
        const int version = currentVersion(db);
        if (version >= kSchemaVersion)
            return true; // güncel: yedek alınmaz, migration çalışmaz
    }

    if (!isNewFile) {
        // Var olan bir dosyayı yükseltiyoruz: önce bağlantıyı kapat (dosya
        // kilitlenmemiş olsun), ham kopyasını al, sonra yeniden aç.
        QSqlDatabase::database(connectionName).close();
        QSqlDatabase::removeDatabase(connectionName);

        if (!backupFile(path, errorOut))
            return false;

        if (!openConnection())
            return false;
    }

    QSqlDatabase db = QSqlDatabase::database(connectionName);
    int version = currentVersion(db);

    while (version < kSchemaVersion) {
        QSqlQuery tx(db);
        tx.exec(QStringLiteral("BEGIN IMMEDIATE"));

        if (!migrateStep(db, version, errorOut)) {
            QSqlQuery rb(db);
            rb.exec(QStringLiteral("ROLLBACK"));
            return false;
        }

        const int nextVersion = version + 1;
        if (!setVersion(db, nextVersion)) {
            QSqlQuery rb(db);
            rb.exec(QStringLiteral("ROLLBACK"));
            if (errorOut)
                *errorOut = QStringLiteral("Şema sürümü yazılamadı");
            return false;
        }

        QSqlQuery commit(db);
        if (!execOrFail(commit, QStringLiteral("COMMIT"), errorOut))
            return false;

        version = nextVersion;
    }

    return true;
}
