#include "ui/page_quote.h"

#include "core/db.h"

#include <QApplication>
#include <QMainWindow>
#include <QMessageBox>
#include <QSqlDatabase>


// Programın giriş noktası: veritabanını açar, ana pencereyi kurar ve olay
// döngüsünü başlatır.
// setOrganizationName/setApplicationName QApplication'dan bile önce ayarlanır
// ve bu zorunludur: Db::defaultPath() içindeki QStandardPaths::AppDataLocation
// veritabanı yolunu bu iki değerden türetir.
//   db     : openAndMigrate'in AÇTIĞI bağlantı, adıyla ("teklif") geri alınır —
//            burada yeni bir bağlantı açılmaz
//   window : ana pencere
//   page   : teklif ekranı. Heap'te oluşturulup setCentralWidget ile pencereye
//            verilir; sahipliğini QMainWindow alır, elle delete gerekmez.
// app.exec() olay döngüsüdür ve pencere kapanana kadar geri dönmez.
int main(int argc, char *argv[])
{
    // QStandardPaths::AppDataLocation bu ikisine bağlıdır (Db::defaultPath
    // yorumuna bkz.) — veritabanı yolu hesaplanmadan önce ayarlanmalı.
    QCoreApplication::setOrganizationName(QStringLiteral("OzYapi"));
    QCoreApplication::setApplicationName(QStringLiteral("Teklif"));

    QApplication app(argc, argv);

    QString err;
    if (!Db::openAndMigrate(Db::defaultPath(), &err)) {
        QMessageBox::critical(nullptr, QStringLiteral("Veritabanı açılamadı"), err);
        return 1;
    }

    // Db::openAndMigrate varsayılan bağlantı adı "teklif"dir.
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("teklif"));

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("Teklif"));
    window.resize(1100, 720);

    auto *page = new PageQuote(db, &window);
    window.setCentralWidget(page);
    window.show();

    return app.exec();
}
