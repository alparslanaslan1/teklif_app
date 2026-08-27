#include "ui/page_quote.h"

#include "core/db.h"

#include <QApplication>
#include <QMainWindow>
#include <QMessageBox>
#include <QSqlDatabase>


// ═══ main() ═══════════════════════════════════════════════════════════════
// NE YAPAR : Programın giriş noktası. Veritabanını açar, ana pencereyi kurar
//            ve olay döngüsünü başlatır.
//
// ADIM ADIM (açılışta bir sorun varsa bu sırayla ilerleyin):
//
//   [1] setOrganizationName / setApplicationName
//       QApplication'DAN BİLE ÖNCE ayarlanır ve bu ZORUNLUDUR:
//       Db::defaultPath() içindeki QStandardPaths::AppDataLocation bu iki
//       değerden yolu türetir. Ayarlanmazsa yol boş döner ve veritabanı
//       ~/.teklif gibi bir yedek konuma düşer.
//
//   [2] QApplication app(argc, argv)
//       Widget altyapısı burada kurulur. Bundan ÖNCE hiçbir QWidget
//       oluşturulamaz. (Testlerde QT_QPA_PLATFORM=offscreen ile ekransız çalışır.)
//
//   [3] Db::openAndMigrate(Db::defaultPath(), &err)
//       Klasörü oluşturur, dosyayı açar, gerekiyorsa yedekleyip göçürür.
//       Başarısızsa QMessageBox ile hata gösterilir ve program 1 ile ÇIKAR.
//       Buraya düşüyorsanız hata metnini okuyun; sık sebepler:
//         • klasör yazma izni yok
//         • dosya başka bir kopya tarafından kilitli
//         • "Bilinmeyen şema sürümü" -> .db dosyası programdan daha YENİ
//       Teşhis:  qDebug() << Db::defaultPath() << err;
//
//   [4] QSqlDatabase::database("teklif")
//       openAndMigrate'in AÇTIĞI bağlantı ADIYLA geri alınır — burada yeni bir
//       bağlantı açılmaz. Bağlantı adı openAndMigrate'in varsayılanıyla
//       ("teklif") birebir AYNI olmalıdır; farklı yazılırsa Qt boş/geçersiz bir
//       QSqlDatabase döndürür ve tüm sorgular sessizce başarısız olur.
//       Kontrol:  qDebug() << db.isValid() << db.isOpen();
//
//   [5] Pencere + PageQuote kurulur.
//       `page` HEAP'te oluşturulup setCentralWidget ile pencereye verilir;
//       sahipliği QMainWindow alır, elle delete GEREKMEZ.
//
//   [6] app.exec() -> olay döngüsü. Bu satır pencere kapanana kadar DÖNMEZ;
//       buraya koyduğunuz bir breakpoint programın sonunda tetiklenir.
//
// EKSİK    : PageQuote::setCompanyInfo() HİÇ ÇAĞRILMIYOR. Bu yüzden yazdırma /
//            PDF çıktısındaki firma anteti BOŞ kalır. Ayarlar ekranı gelene
//            kadar buradan elle doldurulması gerekir.
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
