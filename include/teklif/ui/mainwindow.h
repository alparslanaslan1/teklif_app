#pragma once

#include "teklif/core/models.h"

#include <QMainWindow>
#include <QUrl>
#include <QSqlDatabase>

class QListWidget;
class QStackedWidget;
class PageQuote;
class PageArchive;
class PageCustomers;
class PageCatalog;
class PageSettings;
class UpdatePrompt;

// Ana pencere: solda sayfa listesi, sağda seçili sayfa.
//
// SAYFALAR BİRBİRİNİ TANIMAZ. Arşiv "şu teklifi aç" diye sinyal yayınlar,
// bu sınıf teklif ekranına yükletip oraya geçer. Sayfalar arasında doğrudan
// bağlantı kurulsaydı her yeni sayfa diğerlerini de değiştirmeyi
// gerektirirdi; burada yeni bir sayfa eklemek addPage() çağrısı ve varsa
// birkaç sinyal bağlantısından ibarettir.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QSqlDatabase db, QWidget *parent = nullptr);

    // Belge antetinde kullanılacak firma bilgisi (Part 7'de Ayarlar
    // ekranından gelecek).
    void setCompanyInfo(const CompanyInfo &company);

    // Testler için sayfalara doğrudan erişim.
    PageQuote *quotePage() const { return m_pageQuote; }
    PageArchive *archivePage() const { return m_pageArchive; }
    PageCustomers *customersPage() const { return m_pageCustomers; }
    PageCatalog *catalogPage() const { return m_pageCatalog; }
    PageSettings *settingsPage() const { return m_pageSettings; }

    // Sayfa sırası; sol listedeki satırlarla birebir aynı.
    enum Page { PageQuoteIndex = 0, PageArchiveIndex, PageCatalogIndex, PageCustomersIndex,
                PageSettingsIndex };
    void showPage(Page page);

    // İlk çalıştırma sihirbazı ayarları ve katalogu değiştirmiş olabilir;
    // açık ekranlar bayat kalmasın diye hepsi yeniden okunur.
    void reloadAfterFirstRun();

    // Güncelleme akışını bağlar. manifestUrl boşsa güncelleme özelliği
    // tamamen kapalı kalır (menü girdisi de eklenmez) — bu, sunucu adresi
    // henüz belli değilken programın çalışmasını engellememesi için.
    void setupUpdates(const QUrl &manifestUrl, const QString &currentVersion);

private slots:
    // Arşivden ya da müşteri kartından gelen "bu teklifi aç" isteği.
    void openQuote(qint64 quoteId);

private:
    QListWidget *m_nav;
    QStackedWidget *m_stack;

    PageQuote *m_pageQuote;
    PageArchive *m_pageArchive;
    PageCustomers *m_pageCustomers;
    PageCatalog *m_pageCatalog;
    PageSettings *m_pageSettings;
    UpdatePrompt *m_updatePrompt = nullptr;

    void setupUi(QSqlDatabase db);
    // Firma bilgisini ayarlardan okuyup teklif ekranına verir. Hem açılışta
    // hem ayarlar kaydedildiğinde çağrılır, böylece antet tek yerden beslenir.
    void reloadCompanyInfo();

    QSqlDatabase m_db;
};
