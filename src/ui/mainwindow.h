#pragma once

#include "core/models.h"

#include <QMainWindow>
#include <QSqlDatabase>

class QListWidget;
class QStackedWidget;
class PageQuote;
class PageArchive;
class PageCustomers;

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

    // Sayfa sırası; sol listedeki satırlarla birebir aynı.
    enum Page { PageQuoteIndex = 0, PageArchiveIndex, PageCustomersIndex };
    void showPage(Page page);

private slots:
    // Arşivden ya da müşteri kartından gelen "bu teklifi aç" isteği.
    void openQuote(qint64 quoteId);

private:
    QListWidget *m_nav;
    QStackedWidget *m_stack;

    PageQuote *m_pageQuote;
    PageArchive *m_pageArchive;
    PageCustomers *m_pageCustomers;

    void setupUi(QSqlDatabase db);
};
