#pragma once

#include "teklif/core/settings.h"
#include "teklif/update/update_info.h"

#include <QObject>
#include <QSqlDatabase>

class QWidget;
class Updater;

// Güncelleme akışının arayüz tarafı: açılışta sessizce denetler, sürüm
// varsa kullanıcıya sorar, onay verilirse indirir ve kurulumu başlatır.
//
// TASARIM KURALI: güncelleme HİÇBİR ZAMAN programın açılmasını ya da
// kullanılmasını engellemez. İnternet yoksa, sunucu kapalıysa ya da
// manifest bozuksa hiçbir şey gösterilmez — kullanıcı bir hata penceresiyle
// karşılaşmaz, çünkü yapabileceği bir şey yoktur. Tek istisna zorunlu
// güncellemedir (manifest'teki minVersion).
class UpdatePrompt : public QObject
{
    Q_OBJECT

public:
    // manifestUrl boşsa hiçbir şey yapılmaz (güncelleme yapılandırılmamış).
    UpdatePrompt(QSqlDatabase db, const QUrl &manifestUrl, const QString &currentVersion,
                  QWidget *parent);

    // Açılışta çağrılır. Otomatik denetim kapalıysa sessizce çıkar.
    void checkOnStartup();

    // "Güncellemeleri denetle" menüsünden çağrılır: sonuç ne olursa olsun
    // kullanıcıya bildirilir (güncel olduğunu da görmek ister).
    void checkNow();

signals:
    // Kurulum başlatıldı; ana pencere kapanmalıdır — installer çalışan
    // exe'nin üzerine yazamaz.
    void quitRequested();

private:
    QSqlDatabase m_db;
    Settings m_settings;
    Updater *m_updater;
    QWidget *m_parent;
    QString m_currentVersion;
    // true ise sonuç sessiz geçilmez (kullanıcı elle denetledi).
    bool m_sessizDegil = false;

    void onUpdateAvailable(const UpdateInfo &info);
    void startDownload(const UpdateInfo &info);
};
