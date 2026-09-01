#pragma once

#include "teklif/core/settings.h"

#include <QSqlDatabase>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

// Ayarlar ekranı: firma bilgileri ve logo, teklif varsayılanları, çıktı
// tercihleri, arayüz ölçeği ve yedekleme.
//
// Her şey settings tablosunda tutulur, yani yedekle birlikte taşınır ve
// ayrı bir yapılandırma dosyası yoktur.
class PageSettings : public QWidget
{
    Q_OBJECT

public:
    explicit PageSettings(QSqlDatabase db, QWidget *parent = nullptr);

    // Ayarları veritabanından okuyup forma yazar.
    void refresh();

    // Testler için: formu kaydetme (diyalog açmaz).
    bool save(QString *errorOut = nullptr);

signals:
    // Firma bilgisi ya da logo değişti — belge anteti tazelenmeli.
    void companyInfoChanged();
    // Arayüz ölçeği değişti.
    void uiScaleChanged(int yuzde);

private slots:
    void onSaveClicked();
    void onLogoSelectClicked();
    void onLogoClearClicked();
    void onPdfFolderClicked();
    void onBackupClicked();
    void onRestoreClicked();

private:
    QSqlDatabase m_db;
    Settings m_settings;

    QLineEdit *m_unvanEdit;
    QLineEdit *m_yetkiEdit;
    QLineEdit *m_adresEdit;
    QLineEdit *m_telefonEdit;
    QLineEdit *m_emailEdit;
    QLineEdit *m_vergiDairesiEdit;
    QLineEdit *m_vergiNoEdit;

    QLabel *m_logoOnizleme;
    QPushButton *m_logoSecButton;
    QPushButton *m_logoSilButton;

    QSpinBox *m_haneSpin;
    QSpinBox *m_belgeYaziSpin;
    QSpinBox *m_olcekSpin;
    QLineEdit *m_pdfKlasorEdit;
    QPlainTextEdit *m_sartlarEdit;

    void setupUi();
    void logoOnizlemeyiTazele();
};
