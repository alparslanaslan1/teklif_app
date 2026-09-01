#include "teklif/ui/page_catalog.h"

#include "teklif/core/money.h"
#include "teklif/ui/item_table_model.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStandardPaths>
#include <QTableView>
#include <QTextStream>
#include <QVBoxLayout>

PageCatalog::PageCatalog(QSqlDatabase db, QWidget *parent) : QWidget(parent), m_repo(db)
{
    setupUi();
    refresh();
}

void PageCatalog::setupUi()
{
    // --- Filtreler ----------------------------------------------------------
    m_aramaEdit = new QLineEdit(this);
    m_aramaEdit->setObjectName(QStringLiteral("katalogAramaEdit"));
    m_aramaEdit->setPlaceholderText(QStringLiteral("Kod veya ad ara..."));
    m_aramaEdit->setClearButtonEnabled(true);
    connect(m_aramaEdit, &QLineEdit::textChanged, this, &PageCatalog::applyFilter);

    m_kategoriFiltre = new QComboBox(this);
    m_kategoriFiltre->setObjectName(QStringLiteral("katalogKategoriFiltre"));
    connect(m_kategoriFiltre, &QComboBox::currentIndexChanged, this, &PageCatalog::applyFilter);

    m_pasifCheck = new QCheckBox(QStringLiteral("Pasifleri de göster"), this);
    m_pasifCheck->setObjectName(QStringLiteral("katalogPasifCheck"));
    connect(m_pasifCheck, &QCheckBox::toggled, this, &PageCatalog::applyFilter);

    auto *filtreLay = new QHBoxLayout;
    filtreLay->addWidget(m_aramaEdit, 1);
    filtreLay->addWidget(new QLabel(QStringLiteral("Kategori"), this));
    filtreLay->addWidget(m_kategoriFiltre);
    filtreLay->addWidget(m_pasifCheck);

    // --- Tablo --------------------------------------------------------------
    m_model = new ItemTableModel(this);
    m_table = new QTableView(this);
    m_table->setObjectName(QStringLiteral("katalogTable"));
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(ItemTableModel::ColAd, QHeaderView::Stretch);
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &PageCatalog::onSelectionChanged);

    m_ozetLabel = new QLabel(this);
    m_ozetLabel->setObjectName(QStringLiteral("katalogOzetLabel"));

    // --- CSV ----------------------------------------------------------------
    auto *iceAktar = new QPushButton(QStringLiteral("CSV içe aktar"), this);
    iceAktar->setObjectName(QStringLiteral("katalogIceAktarButton"));
    connect(iceAktar, &QPushButton::clicked, this, &PageCatalog::onImportCsvClicked);

    auto *disaAktar = new QPushButton(QStringLiteral("CSV dışa aktar"), this);
    disaAktar->setObjectName(QStringLiteral("katalogDisaAktarButton"));
    connect(disaAktar, &QPushButton::clicked, this, &PageCatalog::onExportCsvClicked);

    auto *csvLay = new QHBoxLayout;
    csvLay->addWidget(m_ozetLabel);
    csvLay->addStretch();
    csvLay->addWidget(iceAktar);
    csvLay->addWidget(disaAktar);

    auto *solLay = new QVBoxLayout;
    solLay->addLayout(filtreLay);
    solLay->addWidget(m_table, 1);
    solLay->addLayout(csvLay);
    auto *sol = new QWidget(this);
    sol->setLayout(solLay);

    // --- Sağ: ekle/düzenle formu -------------------------------------------
    m_kodEdit = new QLineEdit(this);
    m_kodEdit->setObjectName(QStringLiteral("katalogKodEdit"));
    m_adEdit = new QLineEdit(this);
    m_adEdit->setObjectName(QStringLiteral("katalogAdEdit"));
    m_birimEdit = new QLineEdit(this);
    m_birimEdit->setObjectName(QStringLiteral("katalogBirimEdit"));
    m_birimEdit->setPlaceholderText(QStringLiteral("adet, m2, saat..."));
    m_fiyatEdit = new QLineEdit(this);
    m_fiyatEdit->setObjectName(QStringLiteral("katalogFiyatEdit"));
    m_fiyatEdit->setPlaceholderText(QStringLiteral("1.234,56"));

    m_kategoriCombo = new QComboBox(this);
    m_kategoriCombo->setObjectName(QStringLiteral("katalogKategoriCombo"));
    // Düzenlenebilir: listede olmayan bir kategori adı yazılabilsin, kaydederken
    // otomatik oluşturulsun. Ayrı bir kategori yönetim ekranı gerekmiyor.
    m_kategoriCombo->setEditable(true);
    m_kategoriCombo->setInsertPolicy(QComboBox::NoInsert);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Kod *"), m_kodEdit);
    form->addRow(QStringLiteral("Ad *"), m_adEdit);
    form->addRow(QStringLiteral("Birim *"), m_birimEdit);
    form->addRow(QStringLiteral("Varsayılan fiyat"), m_fiyatEdit);
    form->addRow(QStringLiteral("Kategori"), m_kategoriCombo);

    m_yeniButton = new QPushButton(QStringLiteral("Yeni"), this);
    m_yeniButton->setObjectName(QStringLiteral("katalogYeniButton"));
    connect(m_yeniButton, &QPushButton::clicked, this, &PageCatalog::onNewClicked);

    m_kaydetButton = new QPushButton(QStringLiteral("Kaydet"), this);
    m_kaydetButton->setObjectName(QStringLiteral("katalogKaydetButton"));
    connect(m_kaydetButton, &QPushButton::clicked, this, &PageCatalog::onSaveClicked);

    m_pasifButton = new QPushButton(QStringLiteral("Pasife al"), this);
    m_pasifButton->setObjectName(QStringLiteral("katalogPasifButton"));
    connect(m_pasifButton, &QPushButton::clicked, this, &PageCatalog::onToggleActiveClicked);

    auto *butonlar = new QHBoxLayout;
    butonlar->addWidget(m_yeniButton);
    butonlar->addStretch();
    butonlar->addWidget(m_pasifButton);
    butonlar->addWidget(m_kaydetButton);

    auto *formKutu = new QGroupBox(QStringLiteral("Kalem"), this);
    auto *formLay = new QVBoxLayout(formKutu);
    formLay->addLayout(form);
    formLay->addLayout(butonlar);
    formLay->addStretch();

    auto *bolucu = new QSplitter(Qt::Horizontal, this);
    bolucu->addWidget(sol);
    bolucu->addWidget(formKutu);
    bolucu->setStretchFactor(0, 1);

    auto *ana = new QVBoxLayout(this);
    ana->addWidget(bolucu);

    // Başlangıçta seçim yok; onSelectionChanged henüz çalışmadığı için
    // düğmenin ilk hâli burada belirlenir.
    m_pasifButton->setEnabled(false);
}

void PageCatalog::refresh()
{
    loadKategoriler();
    applyFilter();
}

void PageCatalog::loadKategoriler()
{
    const QSignalBlocker b1(m_kategoriFiltre);
    const QSignalBlocker b2(m_kategoriCombo);

    const qint64 oncekiFiltre = m_kategoriFiltre->currentData().toLongLong();
    const QString oncekiForm = m_kategoriCombo->currentText();

    QString err;
    const QVector<Category> kategoriler = m_repo.listCategories(&err);

    m_kategoriFiltre->clear();
    m_kategoriFiltre->addItem(QStringLiteral("Tümü"), QVariant(qint64(0)));

    m_kategoriCombo->clear();
    m_kategoriCombo->addItem(QString(), QVariant(qint64(0))); // kategorisiz

    for (const Category &c : kategoriler) {
        m_kategoriFiltre->addItem(c.ad, QVariant(c.id));
        m_kategoriCombo->addItem(c.ad, QVariant(c.id));
    }

    const int idx = m_kategoriFiltre->findData(QVariant(oncekiFiltre));
    if (idx >= 0)
        m_kategoriFiltre->setCurrentIndex(idx);
    m_kategoriCombo->setCurrentText(oncekiForm);
}

void PageCatalog::applyFilter()
{
    ItemFilter f;
    f.includeInactive = m_pasifCheck->isChecked();
    f.categoryId = m_kategoriFiltre->currentData().toLongLong();
    f.aranan = m_aramaEdit->text();

    QString err;
    const QVector<Item> kalemler = m_repo.list(f, &err);
    if (!err.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Katalog"),
                              QStringLiteral("Katalog okunamadı: %1").arg(err));
        return;
    }

    QHash<qint64, QString> adlar;
    for (const Category &c : m_repo.listCategories())
        adlar.insert(c.id, c.ad);

    m_model->setItems(kalemler, adlar);
    m_ozetLabel->setText(QStringLiteral("%1 kalem").arg(kalemler.size()));

    if (m_seciliId != 0)
        selectItemById(m_seciliId);
}

void PageCatalog::selectItemById(qint64 id)
{
    for (int i = 0; i < m_model->rowCount(); ++i) {
        if (m_model->at(i).id == id) {
            m_table->selectRow(i);
            return;
        }
    }
}

void PageCatalog::onSelectionChanged()
{
    const QModelIndex idx = m_table->currentIndex();
    if (!idx.isValid()) {
        m_pasifButton->setEnabled(false);
        return;
    }

    const Item it = m_model->at(idx.row());
    m_seciliId = it.id;
    formuDoldur(it);
    m_pasifButton->setEnabled(it.id != 0);
}

void PageCatalog::formuDoldur(const Item &it)
{
    m_kodEdit->setText(it.kod);
    m_adEdit->setText(it.ad);
    m_birimEdit->setText(it.birim);
    m_fiyatEdit->setText(it.varsayilanFiyat.toString());

    if (it.categoryId != 0) {
        const int idx = m_kategoriCombo->findData(QVariant(it.categoryId));
        m_kategoriCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    } else {
        m_kategoriCombo->setCurrentIndex(0);
    }

    m_pasifButton->setText(it.aktif ? QStringLiteral("Pasife al")
                                     : QStringLiteral("Aktife al"));
}

void PageCatalog::onNewClicked()
{
    m_seciliId = 0;
    m_table->clearSelection();
    formuDoldur(Item{});
    m_fiyatEdit->setText(QStringLiteral("0,00"));
    m_pasifButton->setEnabled(false);
    m_kodEdit->setFocus();
}

void PageCatalog::onSaveClicked()
{
    Item it;
    it.id = m_seciliId;
    it.kod = m_kodEdit->text().trimmed();
    it.ad = m_adEdit->text().trimmed();
    it.birim = m_birimEdit->text().trimmed();

    if (it.kod.isEmpty() || it.ad.isEmpty() || it.birim.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Eksik bilgi"),
                              QStringLiteral("Kod, ad ve birim boş olamaz."));
        return;
    }

    const auto fiyat = Money::fromString(m_fiyatEdit->text());
    if (!fiyat.has_value() || fiyat->isNegative()) {
        QMessageBox::warning(this, QStringLiteral("Geçersiz fiyat"),
                              QStringLiteral("Fiyat negatif olmayan bir sayı olmalı "
                                              "(örn. 1.234,56)."));
        m_fiyatEdit->setFocus();
        return;
    }
    it.varsayilanFiyat = fiyat.value();

    // Kullanıcı listede olmayan bir kategori adı yazmış olabilir; aynı
    // eşleştirme kuralları (trim, büyük/küçük harf) CSV içe aktarımıyla
    // paylaşılsın diye ensureCategory kullanılır.
    QString err;
    const qint64 catId = m_repo.ensureCategory(m_kategoriCombo->currentText(), &err);
    if (catId < 0) {
        QMessageBox::warning(this, QStringLiteral("Kategori"), err);
        return;
    }
    it.categoryId = catId;

    if (m_seciliId == 0) {
        it.aktif = true;
        if (!m_repo.add(it, &err)) {
            QMessageBox::warning(this, QStringLiteral("Kaydedilemedi"), err);
            return;
        }
        m_seciliId = it.id;
    } else {
        // Aktiflik bayrağı formda değil ayrı düğmede; kaydetme onu
        // sıfırlamamalı.
        const auto mevcut = m_repo.get(m_seciliId);
        it.aktif = mevcut.has_value() ? mevcut->aktif : true;

        if (!m_repo.update(it, &err)) {
            QMessageBox::warning(this, QStringLiteral("Kaydedilemedi"), err);
            return;
        }
    }

    refresh();
    selectItemById(m_seciliId);
    emit catalogChanged();
}

void PageCatalog::onToggleActiveClicked()
{
    if (m_seciliId == 0)
        return;

    const auto mevcut = m_repo.get(m_seciliId);
    if (!mevcut.has_value())
        return;

    const bool yeniDurum = !mevcut->aktif;
    QString err;
    if (!m_repo.setActive(m_seciliId, yeniDurum, &err)) {
        QMessageBox::warning(this, QStringLiteral("Değiştirilemedi"), err);
        return;
    }

    // Pasife alınan kalem varsayılan filtrede kaybolur; kullanıcı ne olduğunu
    // görebilsin diye pasifler otomatik gösterilir.
    if (!yeniDurum && !m_pasifCheck->isChecked())
        m_pasifCheck->setChecked(true); // applyFilter'i tetikler
    else
        refresh();

    selectItemById(m_seciliId);
    emit catalogChanged();
}

void PageCatalog::onImportCsvClicked()
{
    const QString yol = QFileDialog::getOpenFileName(
        this, QStringLiteral("CSV içe aktar"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        QStringLiteral("CSV dosyası (*.csv);;Tüm dosyalar (*)"));
    if (yol.isEmpty())
        return;

    QFile f(yol);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Açılamadı"),
                              QStringLiteral("Dosya okunamadı: %1").arg(yol));
        return;
    }
    QTextStream in(&f);
    // Excel'in yazdığı dosyalar UTF-8'dir (BOM'lu); ayrıştırıcı BOM'u ve
    // ayracı kendisi tanır (bkz. core/csv.cpp).
    in.setEncoding(QStringConverter::Utf8);
    const QString icerik = in.readAll();
    f.close();

    QString err;
    if (!m_repo.importCsv(icerik, &err)) {
        // İçe aktarma hepsi-ya-da-hiçbiri: hata varsa tek satır bile eklenmedi.
        QMessageBox::warning(this, QStringLiteral("İçe aktarılamadı"),
                              QStringLiteral("%1\n\nHiçbir kalem eklenmedi.").arg(err));
        return;
    }

    refresh();
    emit catalogChanged();
    QMessageBox::information(this, QStringLiteral("İçe aktarıldı"),
                              QStringLiteral("Katalog güncellendi."));
}

void PageCatalog::onExportCsvClicked()
{
    QString err;
    const QString icerik = m_repo.exportCsv(&err);
    if (icerik.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Dışa aktarılamadı"), err);
        return;
    }

    const QString onerilen =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + QStringLiteral("/katalog.csv");
    const QString yol = QFileDialog::getSaveFileName(this, QStringLiteral("CSV dışa aktar"),
                                                      onerilen, QStringLiteral("CSV dosyası (*.csv)"));
    if (yol.isEmpty())
        return;

    QFile f(yol);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Yazılamadı"),
                              QStringLiteral("Dosya yazılamadı: %1").arg(yol));
        return;
    }
    QTextStream out(&f);
    // UTF-8: içerik zaten BOM ile başlıyor (csvOlustur), böylece Türkçe
    // Windows'ta Excel dosyayı doğru kodlamayla açar.
    out.setEncoding(QStringConverter::Utf8);
    out << icerik;
    f.close();

    QMessageBox::information(this, QStringLiteral("Dışa aktarıldı"), yol);
}
