#include <QtTest/QtTest>
#include <QSqlDatabase>
#include <QTemporaryDir>

#include "core/db.h"
#include "core/settings.h"
#include "update/update_info.h"
#include "update/updater.h"

class TestUpdater : public QObject
{
    Q_OBJECT

private slots:
    void compareVersions_test_data();
    void compareVersions_test();
    void parseValidManifest();
    void parseRejectsMissingVersion();
    void parseRejectsMissingUrl();
    void parseRejectsHttpUrl();
    void parseRejectsGarbage();
    void parseAcceptsOptionalFieldsMissing();
    void mustUpdateLogic();
    void checksumMatches();
    void checksumMismatchFails();
    void checksumSkippedWhenEmpty();
    void checksumMissingFileFails();

    // --- surum atlama ve otomatik denetim ---
    void skipVersionSuppressesThatVersion();
    void newerVersionIsStillOffered();
    void olderVersionStaysSkipped();
    void autoCheckDefaultsToOn();
    void autoCheckCanBeDisabled();
    void launchInstallerRejectsMissingFile();

private:
    QTemporaryDir *m_dir = nullptr;
    QString m_conn;
    QSqlDatabase m_db;
    void openDb();
    void closeDb();
};

void TestUpdater::compareVersions_test_data()
{
    QTest::addColumn<QString>("a");
    QTest::addColumn<QString>("b");
    QTest::addColumn<int>("beklenen");

    QTest::newRow("esit")            << "1.2.3" << "1.2.3" <<  0;
    QTest::newRow("yama buyuk")      << "1.2.4" << "1.2.3" <<  1;
    QTest::newRow("yama kucuk")      << "1.2.3" << "1.2.4" << -1;
    QTest::newRow("ara surum")       << "1.3.0" << "1.2.9" <<  1;
    QTest::newRow("ana surum")       << "2.0.0" << "1.9.9" <<  1;
    // Metin karsilastirmasi olsaydi "0.10.0" < "0.9.0" cikardi — asil tuzak bu.
    QTest::newRow("10 vs 9")         << "0.10.0" << "0.9.0" <<  1;
    QTest::newRow("9 vs 10")         << "0.9.0" << "0.10.0" << -1;
    QTest::newRow("eksik parca esit")<< "1.2"   << "1.2.0" <<  0;
    QTest::newRow("eksik parca kucuk")<< "1.2"  << "1.2.1" << -1;
    QTest::newRow("tek parca")       << "2"     << "1.9.9" <<  1;
    QTest::newRow("bozuk parca")     << "1.x.0" << "1.0.0" <<  0;
    QTest::newRow("bosluklu")        << " 1.2.3 " << "1.2.3" << 0;
}

void TestUpdater::compareVersions_test()
{
    QFETCH(QString, a);
    QFETCH(QString, b);
    QFETCH(int, beklenen);
    QCOMPARE(::compareVersions(a, b), beklenen);
}

void TestUpdater::parseValidManifest()
{
    const QByteArray json = R"({
        "version": "0.2.0",
        "minVersion": "0.1.0",
        "url": "https://ornek.com/teklif-0.2.0.zip",
        "sha256": "ABCDEF0123",
        "notes": "Teklif listesi eklendi"
    })";

    QString err;
    const auto info = parseUpdateManifest(json, &err);
    QVERIFY2(info.has_value(), qPrintable(err));
    QCOMPARE(info->version, QStringLiteral("0.2.0"));
    QCOMPARE(info->minVersion, QStringLiteral("0.1.0"));
    QCOMPARE(info->url.toString(), QStringLiteral("https://ornek.com/teklif-0.2.0.zip"));
    // Ozet her zaman kucuk harfe indirilir ki karsilastirma tutarli olsun.
    QCOMPARE(info->sha256, QStringLiteral("abcdef0123"));
    QCOMPARE(info->notes, QStringLiteral("Teklif listesi eklendi"));
}

void TestUpdater::parseRejectsMissingVersion()
{
    const QByteArray json = R"({"url": "https://ornek.com/a.zip"})";
    QString err;
    QVERIFY(!parseUpdateManifest(json, &err).has_value());
    QVERIFY(!err.isEmpty());
}

void TestUpdater::parseRejectsMissingUrl()
{
    const QByteArray json = R"({"version": "0.2.0"})";
    QString err;
    QVERIFY(!parseUpdateManifest(json, &err).has_value());
    QVERIFY(!err.isEmpty());
}

void TestUpdater::parseRejectsHttpUrl()
{
    // Paket imzalanmadigi icin tasima guvenligi tek koruma: http reddedilmeli.
    const QByteArray json = R"({"version": "0.2.0", "url": "http://ornek.com/a.zip"})";
    QString err;
    QVERIFY(!parseUpdateManifest(json, &err).has_value());
    QVERIFY(err.contains(QStringLiteral("https")));
}

void TestUpdater::parseRejectsGarbage()
{
    // Sunucu HTML hata sayfasi dondurdugunde program cokmemeli.
    QString err;
    QVERIFY(!parseUpdateManifest("<html>404</html>", &err).has_value());
    QVERIFY(!err.isEmpty());
    QVERIFY(!parseUpdateManifest("", &err).has_value());
}

void TestUpdater::parseAcceptsOptionalFieldsMissing()
{
    const QByteArray json = R"({"version": "0.2.0", "url": "https://ornek.com/a.zip"})";
    QString err;
    const auto info = parseUpdateManifest(json, &err);
    QVERIFY2(info.has_value(), qPrintable(err));
    QVERIFY(info->minVersion.isEmpty());
    QVERIFY(info->sha256.isEmpty());
    QVERIFY(info->notes.isEmpty());
}

void TestUpdater::mustUpdateLogic()
{
    UpdateInfo info;
    info.version = QStringLiteral("0.3.0");

    // minVersion yoksa guncelleme hicbir zaman zorunlu degildir.
    info.minVersion.clear();
    QVERIFY(!Updater::mustUpdate(info, QStringLiteral("0.1.0")));

    info.minVersion = QStringLiteral("0.2.0");
    QVERIFY(Updater::mustUpdate(info, QStringLiteral("0.1.9")));  // eski -> zorunlu
    QVERIFY(!Updater::mustUpdate(info, QStringLiteral("0.2.0"))); // tam sinir -> degil
    QVERIFY(!Updater::mustUpdate(info, QStringLiteral("0.2.1"))); // yeni -> degil
}

void TestUpdater::checksumMatches()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString yol = dir.filePath(QStringLiteral("paket.zip"));

    QFile f(yol);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("teklif paketi");
    f.close();

    // "teklif paketi" metninin bilinen SHA-256'si.
    const QString beklenen = QString::fromLatin1(
        QCryptographicHash::hash("teklif paketi", QCryptographicHash::Sha256).toHex());

    QString err;
    QVERIFY2(Updater::verifyChecksum(yol, beklenen, &err), qPrintable(err));
    // Buyuk harfli ozet de kabul edilmeli.
    QVERIFY(Updater::verifyChecksum(yol, beklenen.toUpper(), &err));
}

void TestUpdater::checksumMismatchFails()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString yol = dir.filePath(QStringLiteral("paket.zip"));

    QFile f(yol);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("bozuk icerik");
    f.close();

    QString err;
    QVERIFY(!Updater::verifyChecksum(yol, QStringLiteral("00112233"), &err));
    QVERIFY(!err.isEmpty());
}

void TestUpdater::checksumSkippedWhenEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString yol = dir.filePath(QStringLiteral("paket.zip"));
    QFile f(yol);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("her sey");
    f.close();

    QString err;
    QVERIFY(Updater::verifyChecksum(yol, QString(), &err));
}

void TestUpdater::checksumMissingFileFails()
{
    QString err;
    QVERIFY(!Updater::verifyChecksum(QStringLiteral("/olmayan/dosya.zip"),
                                      QStringLiteral("abc"), &err));
    QVERIFY(!err.isEmpty());
}

void TestUpdater::openDb()
{
    m_dir = new QTemporaryDir();
    QVERIFY(m_dir->isValid());
    m_conn = QStringLiteral("up_%1").arg(QDateTime::currentMSecsSinceEpoch());
    QString err;
    QVERIFY2(Db::openAndMigrate(m_dir->filePath(QStringLiteral("t.db")), &err, m_conn), qPrintable(err));
    m_db = QSqlDatabase::database(m_conn);
}

void TestUpdater::closeDb()
{
    m_db = QSqlDatabase();
    QSqlDatabase::database(m_conn).close();
    QSqlDatabase::removeDatabase(m_conn);
    delete m_dir;
    m_dir = nullptr;
}

void TestUpdater::skipVersionSuppressesThatVersion()
{
    openDb();
    Settings s(m_db);
    QVERIFY(!Updater::isVersionSkipped(s, QStringLiteral("0.2.0")));

    QString err;
    QVERIFY2(Updater::skipVersion(s, QStringLiteral("0.2.0"), &err), qPrintable(err));
    QVERIFY(Updater::isVersionSkipped(s, QStringLiteral("0.2.0")));
    closeDb();
}

void TestUpdater::newerVersionIsStillOffered()
{
    // "Bu surumu atla" SONSUZA DEK sus demek degil: daha yenisi cikarsa
    // kullanici yine haberdar edilmeli.
    openDb();
    Settings s(m_db);
    QVERIFY(Updater::skipVersion(s, QStringLiteral("0.2.0")));

    QVERIFY2(!Updater::isVersionSkipped(s, QStringLiteral("0.3.0")),
             "atlanan surumden yenisi de bastiriliyor");
    QVERIFY(!Updater::isVersionSkipped(s, QStringLiteral("0.2.1")));
    // Surum karsilastirmasi sayisal: 0.10.0 > 0.2.0
    QVERIFY(!Updater::isVersionSkipped(s, QStringLiteral("0.10.0")));
    closeDb();
}

void TestUpdater::olderVersionStaysSkipped()
{
    openDb();
    Settings s(m_db);
    QVERIFY(Updater::skipVersion(s, QStringLiteral("0.3.0")));
    // Atlanan surumden ESKI bir surum de bastirilir (zaten teklif edilmemeli).
    QVERIFY(Updater::isVersionSkipped(s, QStringLiteral("0.2.0")));
    QVERIFY(Updater::isVersionSkipped(s, QStringLiteral("0.3.0")));
    closeDb();
}

void TestUpdater::autoCheckDefaultsToOn()
{
    // Guncellemeyi kacirmak, gereksiz bir soru gormekten pahali.
    openDb();
    Settings s(m_db);
    QVERIFY2(Updater::isAutoCheckEnabled(s), "otomatik denetim varsayilan olarak kapali");
    closeDb();
}

void TestUpdater::autoCheckCanBeDisabled()
{
    openDb();
    Settings s(m_db);
    QString err;
    QVERIFY2(Updater::setAutoCheckEnabled(s, false, &err), qPrintable(err));
    QVERIFY(!Updater::isAutoCheckEnabled(s));

    QVERIFY(Updater::setAutoCheckEnabled(s, true));
    QVERIFY(Updater::isAutoCheckEnabled(s));
    closeDb();
}

void TestUpdater::launchInstallerRejectsMissingFile()
{
    // Olmayan bir dosyayi calistirmaya kalkmak yerine anlamli hata donmeli;
    // aksi halde program kapanir ve kurulum hic baslamaz.
    QString err;
    QVERIFY(!Updater::launchInstaller(QStringLiteral("/olmayan/kurulum.exe"), &err));
    QVERIFY(!err.isEmpty());
}

QTEST_MAIN(TestUpdater)
#include "test_updater.moc"
