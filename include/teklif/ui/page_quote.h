#pragma once

#include "teklif/core/models.h"
#include "teklif/core/repo_items.h"
#include "teklif/core/repo_quotes.h"
#include "teklif/core/settings.h"
#include "teklif/print/document_layout.h"

#include <QDate>
#include <QSqlDatabase>
#include <QWidget>

class QDateEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class ItemSearch;
class QuoteTableView;
class QuoteLineModel;

// Teklif oluşturma/düzenleme ekranı: müşteri bilgileri + proje/tarih +
// kalem tablosu + toplam + Kaydet/Yazdır/PDF.
//
// MÜŞTERİ AYRI BİR KAYIT DEĞİLDİR. Unvan, adres, vergi bilgisi doğrudan bu
// formda yazılır ve teklif kaydedilirken teklifin İÇİNE yazılır (bkz.
// core/models.h, Customer). Bu yüzden ne bir müşteri seçici ne de bir
// müşteri yönetim ekranı vardır — müşteriye göre arama arşiv ekranındaki
// metin aramasıyla yapılır.
//
// db parametresi zaten AÇIK bir bağlantı olmalı (Db::openAndMigrate ile) —
// bu sınıf veritabanını açmaz/kapatmaz, sadece kullanır.
class PageQuote : public QWidget
{
    Q_OBJECT

public:
    explicit PageQuote(QSqlDatabase db, QWidget *parent = nullptr);

    // RepoItems::listAll ile arama kutusunu besler. Ekran ilk açıldığında ve
    // katalog değiştiğinde çağrılır.
    void reloadCatalog();
    // Ayarlar değiştiğinde şartlar metnini ve belge yazı boyutunu yeniden okur.
    void reloadSettings();

    // Formu sıfırlar, yeni (kaydedilmemiş) teklif moduna geçer.
    void newQuote();

    // Var olan bir teklifi id'sine göre açar. Başarısızsa false + errorOut.
    bool loadQuote(qint64 id, QString *errorOut);

    // Mevcut form durumunu kaydeder (yeni teklifse INSERT, açık bir teklifse
    // UPDATE). Müşteri unvanı boşsa başarısız olur. Testler ve "Kaydet"
    // butonu bu fonksiyonu ortak kullanır.
    bool save(QString *errorOut);

    // Testler için: tablo modeline ve mevcut durum bilgisine doğrudan erişim.
    QuoteLineModel *lineModel() const { return m_model; }
    qint64 currentQuoteId() const { return m_quoteId; }
    QString currentQuoteNo() const { return m_teklifNo; }

    // Form alanlarındaki müşteri bilgisi. Testler ve kaydetme yolu aynı
    // kaynağı kullanır.
    Customer currentCustomer() const;
    void setCustomer(const Customer &musteri);

    // Antette kullanılan firma bilgisi (Ayarlar ekranından beslenir).
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
    // "+" düğmesi: boş satır açar ve imleci açıklama hücresine götürür.
    void addEmptyRow();
    // "−" düğmesi ve Del tuşu: seçili satır(lar)ı siler.
    void deleteSelectedRows();
    // Seçim değiştikçe "Satır sil" düğmesini etkin/pasif yapar; düğmenin
    // sönük durması "seçili satır yok" demektir, kullanıcı tıklayıp
    // hiçbir şey olmamasıyla karşılaşmaz.
    void updateRowButtons();

private:
    // Depolar bağlantıyı kendi içlerinde tutar; ekran ömrü boyunca bir kez
    // kurulur, her çağrıda QSqlDatabase taşınmaz (bkz. core/repo_items.h).
    RepoItems m_repoItems;
    RepoQuotes m_repoQuotes;
    Settings m_settings;

    CompanyInfo m_company;

    // Müşteri alanları. Ayrı bir tabloya değil, teklifin kendisine yazılır.
    QLineEdit *m_musteriUnvan;
    QLineEdit *m_musteriYetkili;
    QLineEdit *m_musteriTelefon;
    QLineEdit *m_musteriEmail;
    QLineEdit *m_musteriAdres;
    QLineEdit *m_musteriVergiDairesi;
    QLineEdit *m_musteriVergiNo;

    ItemSearch *m_search;
    QuoteTableView *m_table;
    QuoteLineModel *m_model;
    QLineEdit *m_projeEdit;
    QDateEdit *m_tarihEdit;
    QSpinBox *m_gecerlilikSpin;
    QLabel *m_genelLabel;
    QLabel *m_teklifNoLabel;
    QPushButton *m_satirEkleButton;
    QPushButton *m_satirSilButton;
    QPushButton *m_saveButton;
    QPushButton *m_printButton;
    QPushButton *m_pdfButton;

    // Kaydedilmiş teklifin kimliği. 0 = henüz kaydedilmemiş yeni teklif;
    // save() bu değere bakarak INSERT mi UPDATE mi yapacağına karar verir.
    qint64 m_quoteId = 0;
    QString m_teklifNo;
    // Kaydedilirken teklife kopyalanan şartlar metni. Yeni teklifte
    // ayarlardan gelir, açılan teklifte kendi metnidir.
    QString m_sartlarMetni;

    void setupUi();
    // Yazdırma ve PDF aynı belge bağlamını kullanır; ikisi de ekrandakiyle
    // birebir aynı çıktıyı üretsin diye tek yerde kurulur.
    DocumentContext buildDocumentContext() const;
};
