#include "ui/mainwindow.h"

#include "core/db.h"
#include "core/settings.h"
#include "core/version.h"
#include "ui/theme.h"

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
    QCoreApplication::setOrganizationName(QStringLiteral("OzYapi"));
    QCoreApplication::setApplicationName(QStringLiteral("Teklif"));

    QApplication app(argc, argv);

    QString err;
    if (!Db::openAndMigrate(Db::defaultPath(), &err)) {
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

    return app.exec();
}
