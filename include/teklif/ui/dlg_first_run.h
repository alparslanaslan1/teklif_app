#pragma once

#include "teklif/core/settings.h"

#include <QDialog>
#include <QSqlDatabase>

class QCheckBox;
class QLineEdit;

// İlk çalıştırma sihirbazı.
//
// Temiz bir makineye kurulduktan sonra program boş bir veritabanıyla açılır:
// firma bilgisi yok (belge antetsiz basılır), katalog boş (arama hiçbir şey
// bulmaz). Kullanıcı bu ikisini Ayarlar ve Katalog ekranlarından kendisi
// bulmak zorunda kalmasın diye açılışta bir kez sorulur.
//
// Sihirbaz "gerekli" değildir: hepsi atlanabilir, atlanırsa program yine
// çalışır ve aynı bilgiler ekranlardan girilebilir.
class FirstRunDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FirstRunDialog(QSqlDatabase db, QWidget *parent = nullptr);

    // Sihirbazın gösterilmesi gerekiyor mu: firma unvanı boşsa gösterilir.
    // Kullanıcı unvanı boş bırakıp geçtiyse bir daha sorulmaz — bunun için
    // ayrı bir "gösterildi" bayrağı tutulur.
    static bool shouldShow(const Settings &settings);

    // Sihirbazın bir daha açılmayacağını işaretler.
    static bool markShown(Settings &settings, QString *errorOut = nullptr);

    // Örnek katalogu ekler (doğalgaz tesisat kalemleri).
    // Zaten kalem varsa hiçbir şey yapmaz ve true döner: sihirbaz ikinci kez
    // çalıştırılsa bile katalog ikizlenmez.
    static bool loadSampleCatalog(QSqlDatabase db, QString *errorOut = nullptr);

private slots:
    void onAccept();

private:
    QSqlDatabase m_db;
    Settings m_settings;

    QLineEdit *m_unvanEdit;
    QLineEdit *m_adresEdit;
    QLineEdit *m_telefonEdit;
    QLineEdit *m_emailEdit;
    QLineEdit *m_vergiDairesiEdit;
    QLineEdit *m_vergiNoEdit;
    QCheckBox *m_ornekKatalogCheck;

    void setupUi();
};
