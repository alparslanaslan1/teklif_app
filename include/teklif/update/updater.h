#pragma once

#include "teklif/update/update_info.h"

#include "teklif/core/settings.h"

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

// Otomatik güncelleme: açılışta sunucudaki latest.json'a bakar, yeni sürüm
// varsa paketi indirir, SHA-256 ile doğrular ve yardımcı programı çağırır.
//
// TASARIM KURALI: güncelleme HİÇBİR ZAMAN programın açılmasını engellemez.
// İnternet yoksa, sunucu kapalıysa, manifest bozuksa sadece checkFailed
// yayınlanır ve program normal çalışmaya devam eder. Tek istisna zorunlu
// güncellemedir (bkz. mustUpdate).
//
// Arayüz içermez (Widgets'e bağlı değil) — kullanıcıya "güncelleme var,
// kurulsun mu?" diyaloğunu gösteren kısım ui katmanındadır.
class Updater : public QObject
{
    Q_OBJECT

public:
    // manifestUrl : latest.json adresi (https olmalı)
    // currentVersion : çalışan programın sürümü, normalde APP_VERSION
    explicit Updater(const QUrl &manifestUrl, const QString &currentVersion,
                      QObject *parent = nullptr);
    ~Updater() override;

    // latest.json'u indirip sürümü karşılaştırır. Sonuç sinyallerle bildirilir.
    // Ağ beklemesi programı kilitlemesin diye tamamen asenkrondur.
    void checkForUpdate();

    // Güncelleme paketini indirir ve SHA-256'sını doğrular. Yalnızca
    // updateAvailable sinyalinden sonra, kullanıcı onayıyla çağrılmalıdır.
    void downloadUpdate(const UpdateInfo &info);

    // Çalışan sürüm, sunucunun dayattığı minVersion'dan eski mi. True ise
    // kullanıcıya "güncellemeden devam edemezsiniz" denmelidir — genelde veri
    // şeması değiştiği ve eski programın yeni .db dosyasını açamayacağı
    // durumlarda kullanılır (bkz. Db::migrateStep).
    static bool mustUpdate(const UpdateInfo &info, const QString &currentVersion);

    // İndirilen dosyanın SHA-256'sını hesaplayıp beklenenle karşılaştırır.
    // beklenenSha256 boşsa doğrulama yapılmaz ve true döner (manifest'te
    // özet vermek isteğe bağlıdır, ama şiddetle önerilir).
    static bool verifyChecksum(const QString &dosyaYolu, const QString &beklenenSha256,
                                QString *errorOut);

    // İndirilen kurulum dosyasını çalıştırır ve true dönerse ÇAĞIRAN TARAF
    // programı hemen kapatmalıdır: installer, çalışan exe'nin üzerine
    // yazamaz.
    //
    // NEDEN INSTALLER, ELLE DOSYA DEĞİŞTİRME DEĞİL: Windows çalışan bir
    // exe'yi kilitler, bu yüzden program kendi dosyalarını değiştiremez.
    // Klasik çözüm, ana program kapandıktan sonra dosyaları taşıyan ikinci
    // bir yardımcı programdır — ama Inno Setup bu işi zaten doğru yapıyor
    // (sürüm kaydı, kısayollar, kaldırma girdisi, kısmi kurulumdan dönüş).
    // İkinci bir kopya yazmak, aynı işi daha kötü yapmak olurdu.
    //
    // Kurulum SESSİZ kipte (/SILENT) çalışır: kullanıcı zaten onay verdi,
    // bir daha sihirbaz göstermenin anlamı yok.
    static bool launchInstaller(const QString &kurulumDosyasi, QString *errorOut = nullptr);

    // Kullanıcı "bu sürümü atla" dedi: bu sürüm bir daha teklif edilmez,
    // ama daha yenisi çıkarsa yine haber verilir.
    static bool skipVersion(Settings &settings, const QString &version, QString *errorOut = nullptr);

    // Verilen sürüm kullanıcı tarafından atlandı mı.
    static bool isVersionSkipped(const Settings &settings, const QString &version);

    // Açılışta otomatik denetim açık mı (varsayılan: açık).
    static bool isAutoCheckEnabled(const Settings &settings);
    static bool setAutoCheckEnabled(Settings &settings, bool acik, QString *errorOut = nullptr);

signals:
    // Sunucudaki sürüm çalışandan yeni.
    void updateAvailable(const UpdateInfo &info);
    // Sunucudaki sürüm çalışanla aynı ya da daha eski.
    void upToDate();
    // Ağ/ayrıştırma hatası. Program çalışmaya devam etmelidir.
    void checkFailed(const QString &hata);

    void downloadProgress(qint64 alinan, qint64 toplam);
    // Paket indirildi ve özeti doğrulandı; parametre yerel zip dosyasının yolu.
    void downloadFinished(const QString &paketYolu);
    void downloadFailed(const QString &hata);

private:
    QNetworkAccessManager *m_net;
    QUrl m_manifestUrl;
    QString m_currentVersion;
    QNetworkReply *m_reply = nullptr; // aynı anda tek istek; yenisi öncekini iptal eder

    void abortPending();
};
