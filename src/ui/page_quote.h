#pragma once

#include "core/models.h"

#include <QSqlDatabase>
#include <QWidget>

class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;
class ItemSearch;
class QuoteTableView;
class QuoteLineModel;

// Teklif oluşturma/düzenleme ekranı. Antet + müşteri seçimi + arama kutusu
// + kalem tablosu + toplamlar + Kaydet.
//
// db parametresi zaten AÇIK bir bağlantı olmalı (Db::openAndMigrate ile) —
// bu sınıf veritabanını açmaz/kapatmaz, sadece kullanır.
class PageQuote : public QWidget
{
    Q_OBJECT

public:
    explicit PageQuote(QSqlDatabase db, QWidget *parent = nullptr);

    // RepoItems::listAll ile arama kutusunu, RepoCustomers::listAll ile
    // müşteri açılır listesini besler. Ekran ilk açıldığında ve katalog/
    // müşteri listesi değiştiğinde çağrılır.
    void reloadCatalog();
    void reloadCustomers();

    // Formu sıfırlar, yeni (kaydedilmemiş) teklif moduna geçer.
    void newQuote();

    // Var olan bir teklifi id'sine göre açar. Başarısızsa false + errorOut.
    bool loadQuote(qint64 id, QString *errorOut);

    // Mevcut form durumunu kaydeder (yeni teklifse INSERT, açık bir teklifse
    // UPDATE). Müşteri seçilmemişse başarısız olur. Testler ve "Kaydet"
    // butonu bu fonksiyonu ortak kullanır.
    bool save(QString *errorOut);

    // Testler için: tablo modeline ve mevcut durum bilgisine doğrudan erişim.
    QuoteLineModel *lineModel() const { return m_model; }
    qint64 currentQuoteId() const { return m_quoteId; }
    QString currentQuoteNo() const { return m_teklifNo; }
    void selectCustomerById(qint64 customerId);

    // Antette kullanılan firma bilgisi. Part 7'de Ayarlar ekranından
    // beslenecek; o zamana kadar çağıran taraf (main.cpp) elle verir —
    // hiç verilmezse boş kalır, belge boş firma bilgisiyle basılabilir
    // (bkz. DocumentLayout: logo/firma bilgisi yoksa sabit bir boşluk
    // ayrılmaz, sayfalama bozulmaz).
    void setCompanyInfo(const CompanyInfo &company) { m_company = company; }

    // Geçerli formdan bir Quote nesnesi kurar (kaydedilmemiş haliyle bile) —
    // save()'in kullandığı aynı toplama mantığını Yazdır/PDF Kaydet de
    // kullanır, ekranda görülenle çıktı arasında fark olmasın diye.
    Quote currentQuoteSnapshot() const;

private slots:
    void onItemChosen(const Item &item);
    void recomputeTotals();
    void onSaveClicked();
    void onPrintClicked();
    void onExportPdfClicked();
    void deleteCurrentRow();

private:
    QSqlDatabase m_db;
    CompanyInfo m_company;

    QComboBox *m_customerCombo;
    ItemSearch *m_search;
    QuoteTableView *m_table;
    QuoteLineModel *m_model;
    QCheckBox *m_kdvCheck;
    QLabel *m_araLabel;
    QLabel *m_kdvLabel;
    QLabel *m_genelLabel;
    QLabel *m_teklifNoLabel;
    QPushButton *m_saveButton;
    QPushButton *m_printButton;
    QPushButton *m_pdfButton;

    qint64 m_quoteId = 0;
    QString m_teklifNo;
    QVector<Customer> m_customers; // reloadCustomers() doldurur; currentCustomer() burada arar

    void setupUi();
    Customer currentCustomer() const;
};
