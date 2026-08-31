#pragma once

#include "core/repo_customers.h"
#include "core/repo_quotes.h"

#include <QSqlDatabase>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableView;
class QuoteSummaryModel;

// Arşiv ekranı: kayıtlı tekliflerin listesi, filtreler ve teklif üzerinde
// yapılan komutlar (aç, kopyala, durum değiştir).
//
// Teklifi KENDİSİ açmaz — quoteOpenRequested sinyalini yayınlar ve
// MainWindow teklif ekranına geçirir. Böylece arşiv, teklif ekranının nasıl
// çalıştığını bilmek zorunda kalmaz.
class PageArchive : public QWidget
{
    Q_OBJECT

public:
    explicit PageArchive(QSqlDatabase db, QWidget *parent = nullptr);

    // Filtre kutularını (müşteri listesi) ve tabloyu tazeler. Ekrana her
    // geçildiğinde çağrılır: başka bir ekranda kaydedilen teklif burada
    // görünmelidir.
    void refresh();

    // Testler için: tabloya ve o an seçili teklife doğrudan erişim.
    QuoteSummaryModel *model() const { return m_model; }
    qint64 selectedQuoteId() const;

signals:
    // Kullanıcı bir teklifi açmak istedi (çift tıkladı ya da "Aç" dedi).
    void quoteOpenRequested(qint64 quoteId);
    // Kopyalama yeni bir teklif oluşturdu; onu da açmak mantıklı.
    void quoteDuplicated(qint64 newQuoteId);

private slots:
    void applyFilter();
    void clearFilter();
    void openSelected();
    void duplicateSelected();
    void changeStatusOfSelected();

private:
    RepoQuotes m_repoQuotes;
    RepoCustomers m_repoCustomers;

    QComboBox *m_customerCombo;
    QComboBox *m_durumCombo;
    QCheckBox *m_tarihCheck;
    QDateEdit *m_tarihBas;
    QDateEdit *m_tarihBit;
    QLineEdit *m_aramaEdit;
    QTableView *m_table;
    QuoteSummaryModel *m_model;
    QLabel *m_ozetLabel;
    QPushButton *m_acButton;
    QPushButton *m_kopyalaButton;
    QPushButton *m_durumButton;

    void setupUi();
    QuoteFilter currentFilter() const;
    void updateButtons();
};
