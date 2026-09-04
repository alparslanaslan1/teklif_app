#pragma once

#include <QSqlDatabase>
#include <QString>

// Veritabanı bağlantısı ve şema göçü (migration).
//
// Kural: kullanıcının diskindeki .db dosyası her zaman EN SON çalıştırdığı
// program sürümünün şemasını görmüş olabilir; program güncellendiğinde eski
// dosya açılınca migrateStep() içindeki adımlar sırayla uygulanır. Yeni bir
// migration eklerken:
//   1. kSchemaVersion'ı bir artır
//   2. migrateStep() içine yeni "case" ekle (fromVersion = eski sürüm)
//   3. eski case'leri SİLME — kullanıcıda hâlâ o sürümden dosya olabilir
class Db
{
public:
    // Uygulamanın bildiği en güncel şema sürümü.
    static constexpr int kSchemaVersion = 2;

    // Migration öncesi alınan .bak dosyalarından kaç tanesi saklanır.
    // Sınırsız biriktirmek diski yavaşça doldurur; tek bir yedek bırakmak da
    // arka arkaya iki hatalı göçte geri dönülecek noktayı yok eder.
    static constexpr int kSaklananYedek = 5;

    // Veritabanını açar; dosya yoksa oluşturur; şema güncel değilse göçürür.
    // Var olan bir dosya göçürülmeden önce aynı klasöre "<ad>.bak-YYYYMMDD-HHMMSS"
    // adıyla ham kopyası alınır. Göç bir adımda başarısız olursa o adımın
    // transaction'ı geri alınır (ROLLBACK) ve dosya migration öncesi haliyle
    // kalır — .bak dosyası da zaten duruyor olur, ekstra bir kurtarma adımı.
    //
    // connectionName: aynı süreçte birden fazla bağımsız bağlantı açılabilmesi
    // için (testlerde her test kendi bağlantı adını kullanır, birbirine
    // karışmaz).
    static bool openAndMigrate(const QString &path, QString *errorOut,
                                const QString &connectionName = QStringLiteral("teklif"));

    // Kullanıcının veri dizinindeki varsayılan veritabanı yolu. Programın
    // kurulduğu klasörde DEĞİL — Windows'ta %APPDATA%\<Uygulama>\teklif.db
    // altında, güncelleme sırasında exe değişse de veri yerinde kalır.
    static QString defaultPath();

    // Dosyanın "<yol>.bak-YYYYAAGG-SSDDss" kopyasını alır ve ardından eski
    // yedekleri budar. Normalde yalnızca göç öncesinde çağrılır; testlerin
    // budamayı doğrulayabilmesi için açık.
    static bool backupFile(const QString &path, QString *errorOut);

    // En yeni kSaklananYedek adet .bak dosyasını bırakır, eskilerini siler.
    static void pruneBackups(const QString &path);

private:
    static int currentVersion(QSqlDatabase &db);
    static bool setVersion(QSqlDatabase &db, int version);
    static bool migrateStep(QSqlDatabase &db, int fromVersion, QString *errorOut);
    static bool createV1Schema(QSqlDatabase &db, QString *errorOut);
    // v1 -> v2: müşteri bilgisi ayrı tablodan teklifin içine taşınır.
    static bool migrateV1ToV2(QSqlDatabase &db, QString *errorOut);
};
