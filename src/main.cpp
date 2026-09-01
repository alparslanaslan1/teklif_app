#include "teklif/ui/mainwindow.h"

#include "teklif/core/db.h"
#include "teklif/core/log.h"
#include "teklif/core/settings.h"
#include "teklif/core/version.h"
#include "teklif/ui/dlg_first_run.h"
#include "teklif/ui/theme.h"

#include <QApplication>
#include <QMessageBox>
#include <QSqlDatabase>

// Programın giriş noktası: veritabanını açar, ana pencereyi kurar ve olay
// döngüsünü başlatır.
// setOrganizationName/setApplicationName QApplication'dan bile önce ayarlanır
// ve bu zorunludur: Db::defaultPath() içindeki QStandardPaths::AppDataLocation
// veritabanı yolunu bu iki değerden türetir.
//   db     : openAndMigrate'in AÇTIĞI bağlantı, adıyla ("teklif") geri alınır —
//            burada yeni bir bağlantı açılmaz
//   firma  : belge antetindeki bilgiler; ayarlardan okunur, girilmemişse boş
//            kalır ve DocumentLayout antetı kendiliğinden kısaltır
//   window : ana pencere. Sayfaları kendisi kurar ve birbirine bağlar.
// app.exec() olay döngüsüdür ve pencere kapanana kadar geri dönmez.
int main(int argc, char *argv[])
{
    // DEĞİŞTİRMEYİN: veritabanının yeri bu iki addan türetilir
    // (%APPDATA%\KarasuVizyon\Teklif\teklif.db). Sonradan değiştirilirse
    // kullanıcıların mevcut veritabanı "kayıp" olur — program boş açılır ve
    // eski dosyayı elle taşımak gerekir.
    QCoreApplication::setOrganizationName(QStringLiteral("KarasuVizyon"));
    QCoreApplication::setApplicationName(QStringLiteral("Teklif"));

    QApplication app(argc, argv);

    // Günlük, veritabanı açılmadan ÖNCE kurulur: açılış hataları da kayda
    // geçsin. Kullanıcının veri dizinine yazar (teklif.db'nin yanına).
    Log::install();
    qCInfo(logApp) << "Teklif" << APP_VERSION << "başlıyor;" << APP_GIT_SHA;

    QString err;
    if (!Db::openAndMigrate(Db::defaultPath(), &err)) {
        qCCritical(logDb) << "veritabanı açılamadı:" << Db::defaultPath() << err;
        QMessageBox::critical(nullptr, QStringLiteral("Veritabanı açılamadı"), err);
        return 1;
    }

    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("teklif"));

    // Arayüz ölçeği pencereler kurulmadan ÖNCE uygulanır; sonradan
    // uygulansaydı zaten oluşmuş widget'lar eski boyutta kalırdı.
    Settings settings(db);
    Theme::applyFromSettings(settings);

    // Firma bilgisi MainWindow tarafından ayarlardan okunur; ayarlar
    // ekranında değiştirilince de aynı yoldan tazelenir.
    MainWindow window(db);
    window.setWindowTitle(QStringLiteral("Teklif %1").arg(QStringLiteral(APP_VERSION)));
    window.resize(1200, 780);
    window.show();

    // Güncelleme sunucusu. Boş bırakılırsa özellik kapalı kalır; sürüm
    // yayınlanmaya başlayınca burası doldurulur (bkz. README).
    window.setupUpdates(QUrl(QStringLiteral(TEKLIF_UPDATE_URL)),
                         QStringLiteral(APP_VERSION));

    // İlk çalıştırma sihirbazı ana pencere GÖSTERİLDİKTEN sonra açılır:
    // arkasında boş bir ekran değil, gerçek program görünsün. Kullanıcı
    // atlarsa da bir daha sorulmaz.
    if (FirstRunDialog::shouldShow(settings)) {
        FirstRunDialog sihirbaz(db, &window);
        sihirbaz.exec();
        // Sihirbaz firma bilgisi ve katalog yazmış olabilir; ekranlar tazelenir.
        window.reloadAfterFirstRun();
    }

    return app.exec();
}
