#include "ui/mainwindow.h"

#include "core/db.h"
#include "core/settings.h"
#include "core/version.h"

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

    Settings settings(db);
    CompanyInfo firma;
    firma.unvan = settings.valueOr(Settings::keyCompanyName());
    firma.adres = settings.valueOr(Settings::keyCompanyAddress());
    firma.telefon = settings.valueOr(Settings::keyCompanyPhone());
    firma.email = settings.valueOr(Settings::keyCompanyEmail());
    firma.vergiDairesi = settings.valueOr(Settings::keyCompanyTaxOffice());
    firma.vergiNo = settings.valueOr(Settings::keyCompanyTaxNo());

    MainWindow window(db);
    window.setWindowTitle(QStringLiteral("Teklif %1").arg(QStringLiteral(APP_VERSION)));
    window.setCompanyInfo(firma);
    window.resize(1200, 780);
    window.show();

    return app.exec();
}
