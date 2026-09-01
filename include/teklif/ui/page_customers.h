#pragma once

#include "teklif/core/repo_customers.h"
#include "teklif/core/repo_quotes.h"

#include <QSqlDatabase>
#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QTableView;
class QuoteSummaryModel;

// Müşteriler ekranı: solda aranabilir liste, sağda seçili müşterinin detay
// kartı ve o müşteriye verilmiş teklifler.
//
// Silme YOKTUR, yalnızca pasife alma: quotes.customer_id üzerinde
// ON DELETE RESTRICT var, yani teklifi olan bir müşteri zaten silinemez.
// Silinebilenler için de aynı kural uygulanır ki geçmiş bozulmasın.
class PageCustomers : public QWidget
{
    Q_OBJECT

public:
    explicit PageCustomers(QSqlDatabase db, QWidget *parent = nullptr);

    // Listeyi ve seçili kartı tazeler.
    void refresh();

    // Testler için.
    qint64 selectedCustomerId() const { return m_seciliId; }
    void selectCustomerById(qint64 id);

signals:
    // Müşteri eklendi/değişti — müşteri listesi kullanan diğer ekranlar
    // (teklif ekranı) kendini tazelemeli.
    void customersChanged();
    // Kullanıcı müşterinin tekliflerinden birini açmak istedi.
    void quoteOpenRequested(qint64 quoteId);

private slots:
    void onSearchChanged();
    void onSelectionChanged();
    void onNewClicked();
    void onSaveClicked();
    void onToggleActiveClicked();

private:
    RepoCustomers m_repoCustomers;
    RepoQuotes m_repoQuotes;

    QLineEdit *m_aramaEdit;
    QCheckBox *m_pasifCheck;
    QListWidget *m_liste;

    QLineEdit *m_unvanEdit;
    QLineEdit *m_yetkiliEdit;
    QLineEdit *m_telefonEdit;
    QLineEdit *m_emailEdit;
    QLineEdit *m_adresEdit;
    QLineEdit *m_vergiDairesiEdit;
    QLineEdit *m_vergiNoEdit;
    QPlainTextEdit *m_notlarEdit;

    QPushButton *m_yeniButton;
    QPushButton *m_kaydetButton;
    QPushButton *m_pasifButton;

    QTableView *m_teklifTable;
    QuoteSummaryModel *m_teklifModel;
    QLabel *m_toplamLabel;

    // 0 = yeni (kaydedilmemiş) müşteri. Kaydetme bu değere bakarak INSERT mi
    // UPDATE mi yapacağına karar verir — teklif ekranındaki m_quoteId ile
    // aynı kalıp.
    qint64 m_seciliId = 0;

    void setupUi();
    void loadListe();
    void formuDoldur(const Customer &c);
    Customer formdanOku() const;
    void detayiTazele();
};
