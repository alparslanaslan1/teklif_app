#pragma once

#include "teklif/core/repo_items.h"

#include <QSqlDatabase>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableView;
class ItemTableModel;

// Katalog ekranı: solda aranabilir/filtrelenebilir kalem tablosu, sağda
// ekle/düzenle formu, altta CSV içe-dışa aktarma.
//
// Silme YOKTUR, yalnızca pasife alma: geçmiş tekliflerdeki satırlar fiyatı
// kopyaladığı için silme onları teknik olarak bozmaz, ama kalemin katalogdan
// kaybolması kullanıcıyı yanıltır (bkz. RepoItems::setActive).
class PageCatalog : public QWidget
{
    Q_OBJECT

public:
    explicit PageCatalog(QSqlDatabase db, QWidget *parent = nullptr);

    // Kategori kutusunu ve tabloyu tazeler.
    void refresh();

    // Testler için.
    ItemTableModel *model() const { return m_model; }
    qint64 selectedItemId() const { return m_seciliId; }
    void selectItemById(qint64 id);

signals:
    // Katalog değişti — teklif ekranındaki arama indeksi bayatlamasın.
    void catalogChanged();

private slots:
    void applyFilter();
    void onSelectionChanged();
    void onNewClicked();
    void onSaveClicked();
    void onToggleActiveClicked();
    void onImportCsvClicked();
    void onExportCsvClicked();

private:
    RepoItems m_repo;

    QLineEdit *m_aramaEdit;
    QComboBox *m_kategoriFiltre;
    QCheckBox *m_pasifCheck;
    QTableView *m_table;
    ItemTableModel *m_model;
    QLabel *m_ozetLabel;

    QLineEdit *m_kodEdit;
    QLineEdit *m_adEdit;
    QLineEdit *m_birimEdit;
    QLineEdit *m_fiyatEdit;
    // Düzenlenebilir: kullanıcı listede olmayan bir kategori adı yazarsa
    // kaydederken otomatik oluşturulur (RepoItems::ensureCategory).
    QComboBox *m_kategoriCombo;

    QPushButton *m_yeniButton;
    QPushButton *m_kaydetButton;
    QPushButton *m_pasifButton;

    // 0 = yeni (kaydedilmemiş) kalem. Kaydetme buna bakarak INSERT mi UPDATE
    // mi yapacağına karar verir — teklif ve müşteri ekranlarıyla aynı kalıp.
    qint64 m_seciliId = 0;

    void setupUi();
    void loadKategoriler();
    void formuDoldur(const Item &it);
};
