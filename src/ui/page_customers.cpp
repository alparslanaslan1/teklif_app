#include "teklif/ui/page_customers.h"

#include "teklif/ui/quote_summary_model.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTableView>
#include <QVBoxLayout>

PageCustomers::PageCustomers(QSqlDatabase db, QWidget *parent)
    : QWidget(parent)
    , m_repoCustomers(db)
    , m_repoQuotes(db)
{
    setupUi();
    refresh();
}

void PageCustomers::setupUi()
{
    // --- Sol: arama + liste -------------------------------------------------
    m_aramaEdit = new QLineEdit(this);
    m_aramaEdit->setObjectName(QStringLiteral("musteriAramaEdit"));
    m_aramaEdit->setPlaceholderText(QStringLiteral("Unvan, yetkili, telefon ara..."));
    m_aramaEdit->setClearButtonEnabled(true);
    connect(m_aramaEdit, &QLineEdit::textChanged, this, &PageCustomers::onSearchChanged);

    m_pasifCheck = new QCheckBox(QStringLiteral("Pasifleri de göster"), this);
    m_pasifCheck->setObjectName(QStringLiteral("musteriPasifCheck"));
    connect(m_pasifCheck, &QCheckBox::toggled, this, &PageCustomers::onSearchChanged);

    m_liste = new QListWidget(this);
    m_liste->setObjectName(QStringLiteral("musteriListe"));
    connect(m_liste, &QListWidget::currentRowChanged, this, &PageCustomers::onSelectionChanged);

    auto *solLay = new QVBoxLayout;
    solLay->addWidget(m_aramaEdit);
    solLay->addWidget(m_pasifCheck);
    solLay->addWidget(m_liste, 1);

    auto *sol = new QWidget(this);
    sol->setLayout(solLay);
    sol->setMinimumWidth(240);

    // --- Sağ üst: detay kartı ----------------------------------------------
    m_unvanEdit = new QLineEdit(this);
    m_unvanEdit->setObjectName(QStringLiteral("musteriUnvanEdit"));
    m_yetkiliEdit = new QLineEdit(this);
    m_yetkiliEdit->setObjectName(QStringLiteral("musteriYetkiliEdit"));
    m_telefonEdit = new QLineEdit(this);
    m_emailEdit = new QLineEdit(this);
    m_adresEdit = new QLineEdit(this);
    m_vergiDairesiEdit = new QLineEdit(this);
    m_vergiNoEdit = new QLineEdit(this);
    m_notlarEdit = new QPlainTextEdit(this);
    m_notlarEdit->setMaximumHeight(70);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Unvan *"), m_unvanEdit);
    form->addRow(QStringLiteral("Yetkili"), m_yetkiliEdit);
    form->addRow(QStringLiteral("Telefon"), m_telefonEdit);
    form->addRow(QStringLiteral("E-posta"), m_emailEdit);
    form->addRow(QStringLiteral("Adres"), m_adresEdit);
    form->addRow(QStringLiteral("Vergi dairesi"), m_vergiDairesiEdit);
    form->addRow(QStringLiteral("Vergi no"), m_vergiNoEdit);
    form->addRow(QStringLiteral("Notlar"), m_notlarEdit);

    m_yeniButton = new QPushButton(QStringLiteral("Yeni"), this);
    m_yeniButton->setObjectName(QStringLiteral("musteriYeniButton"));
    connect(m_yeniButton, &QPushButton::clicked, this, &PageCustomers::onNewClicked);

    m_kaydetButton = new QPushButton(QStringLiteral("Kaydet"), this);
    m_kaydetButton->setObjectName(QStringLiteral("musteriKaydetButton"));
    connect(m_kaydetButton, &QPushButton::clicked, this, &PageCustomers::onSaveClicked);

    m_pasifButton = new QPushButton(QStringLiteral("Pasife al"), this);
    m_pasifButton->setObjectName(QStringLiteral("musteriPasifButton"));
    connect(m_pasifButton, &QPushButton::clicked, this, &PageCustomers::onToggleActiveClicked);

    auto *butonlar = new QHBoxLayout;
    butonlar->addWidget(m_yeniButton);
    butonlar->addStretch();
    butonlar->addWidget(m_pasifButton);
    butonlar->addWidget(m_kaydetButton);

    auto *detayKutu = new QGroupBox(QStringLiteral("Müşteri"), this);
    auto *detayLay = new QVBoxLayout(detayKutu);
    detayLay->addLayout(form);
    detayLay->addLayout(butonlar);

    // --- Sağ alt: bu müşterinin teklifleri ----------------------------------
    m_teklifModel = new QuoteSummaryModel(this);
    m_teklifTable = new QTableView(this);
    m_teklifTable->setObjectName(QStringLiteral("musteriTeklifTable"));
    m_teklifTable->setModel(m_teklifModel);
    m_teklifTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_teklifTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_teklifTable->verticalHeader()->setVisible(false);
    // Zaten tek bir müşterinin tekliflerine bakılıyor; unvanı her satırda
    // tekrar etmek yer kaplamaktan başka bir şey yapmaz.
    m_teklifTable->setColumnHidden(QuoteSummaryModel::ColMusteri, true);
    m_teklifTable->horizontalHeader()->setStretchLastSection(true);
    connect(m_teklifTable, &QTableView::doubleClicked, this, [this](const QModelIndex &idx) {
        const qint64 id = m_teklifModel->at(idx.row()).id;
        if (id != 0)
            emit quoteOpenRequested(id);
    });

    m_toplamLabel = new QLabel(this);
    m_toplamLabel->setObjectName(QStringLiteral("musteriToplamLabel"));

    auto *teklifKutu = new QGroupBox(QStringLiteral("Teklifleri"), this);
    auto *teklifLay = new QVBoxLayout(teklifKutu);
    teklifLay->addWidget(m_teklifTable, 1);
    teklifLay->addWidget(m_toplamLabel);

    auto *sagLay = new QVBoxLayout;
    sagLay->addWidget(detayKutu);
    sagLay->addWidget(teklifKutu, 1);
    auto *sag = new QWidget(this);
    sag->setLayout(sagLay);

    auto *bolucu = new QSplitter(Qt::Horizontal, this);
    bolucu->addWidget(sol);
    bolucu->addWidget(sag);
    bolucu->setStretchFactor(1, 1);

    auto *ana = new QVBoxLayout(this);
    ana->addWidget(bolucu);
}

void PageCustomers::refresh()
{
    loadListe();
    detayiTazele();
}

void PageCustomers::loadListe()
{
    const QSignalBlocker b(m_liste);
    m_liste->clear();

    QString err;
    const QVector<Customer> liste =
        m_repoCustomers.search(m_aramaEdit->text(), m_pasifCheck->isChecked(), &err);
    if (!err.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Müşteriler"),
                              QStringLiteral("Müşteri listesi okunamadı: %1").arg(err));
        return;
    }

    for (const Customer &c : liste) {
        // Pasif müşteri listede işaretlenir: kullanıcı neden tekliflerde
        // çıkmadığını görebilsin.
        const QString etiket = c.aktif ? c.unvan
                                        : c.unvan + QStringLiteral("  (pasif)");
        auto *item = new QListWidgetItem(etiket);
        item->setData(Qt::UserRole, QVariant(c.id));
        m_liste->addItem(item);
    }

    // Önceki seçim listede hâlâ varsa korunur (arama daraltıldığında seçimin
    // kaybolması can sıkıcıdır).
    if (m_seciliId != 0) {
        selectCustomerById(m_seciliId);
        if (m_liste->currentRow() >= 0)
            return;
    }

    // Seçim yoksa ilk müşteri seçilir: ekran boş bir formla açılmaz.
    // Yeni müşteri girmek isteyen kullanıcı "Yeni" düğmesini kullanır.
    if (m_liste->count() > 0) {
        // Sinyaller bloklu olduğu için onSelectionChanged elle çağrılır.
        m_liste->setCurrentRow(0);
        m_seciliId = m_liste->item(0)->data(Qt::UserRole).toLongLong();
        QString err;
        const auto c = m_repoCustomers.get(m_seciliId, &err);
        if (c.has_value())
            formuDoldur(*c);
    }
}

void PageCustomers::selectCustomerById(qint64 id)
{
    for (int i = 0; i < m_liste->count(); ++i) {
        if (m_liste->item(i)->data(Qt::UserRole).toLongLong() == id) {
            m_liste->setCurrentRow(i);
            return;
        }
    }
}

void PageCustomers::onSearchChanged()
{
    loadListe();
}

void PageCustomers::onSelectionChanged()
{
    auto *item = m_liste->currentItem();
    if (!item) {
        m_seciliId = 0;
        detayiTazele();
        return;
    }

    m_seciliId = item->data(Qt::UserRole).toLongLong();

    QString err;
    const auto c = m_repoCustomers.get(m_seciliId, &err);
    if (!c.has_value()) {
        QMessageBox::warning(this, QStringLiteral("Müşteri"), err);
        return;
    }
    formuDoldur(*c);
    detayiTazele();
}

void PageCustomers::formuDoldur(const Customer &c)
{
    m_unvanEdit->setText(c.unvan);
    m_yetkiliEdit->setText(c.yetkili);
    m_telefonEdit->setText(c.telefon);
    m_emailEdit->setText(c.email);
    m_adresEdit->setText(c.adres);
    m_vergiDairesiEdit->setText(c.vergiDairesi);
    m_vergiNoEdit->setText(c.vergiNo);
    m_notlarEdit->setPlainText(c.notlar);
    m_pasifButton->setText(c.aktif ? QStringLiteral("Pasife al")
                                    : QStringLiteral("Aktife al"));
}

Customer PageCustomers::formdanOku() const
{
    Customer c;
    c.id = m_seciliId;
    c.unvan = m_unvanEdit->text().trimmed();
    c.yetkili = m_yetkiliEdit->text().trimmed();
    c.telefon = m_telefonEdit->text().trimmed();
    c.email = m_emailEdit->text().trimmed();
    c.adres = m_adresEdit->text().trimmed();
    c.vergiDairesi = m_vergiDairesiEdit->text().trimmed();
    c.vergiNo = m_vergiNoEdit->text().trimmed();
    c.notlar = m_notlarEdit->toPlainText().trimmed();
    c.aktif = true;
    return c;
}

void PageCustomers::detayiTazele()
{
    const bool secili = m_seciliId != 0;
    m_pasifButton->setEnabled(secili);

    if (!secili) {
        m_teklifModel->setQuotes({});
        m_toplamLabel->clear();
        return;
    }

    QuoteFilter f;
    f.customerId = m_seciliId;
    QString err;
    const QVector<QuoteSummary> teklifler = m_repoQuotes.list(f, &err);
    m_teklifModel->setQuotes(teklifler);

    const Money toplam = m_repoQuotes.customerTotal(m_seciliId, &err);
    m_toplamLabel->setText(QStringLiteral("%1 teklif · toplam %2")
                                .arg(teklifler.size())
                                .arg(toplam.toString()));
}

void PageCustomers::onNewClicked()
{
    m_seciliId = 0;
    m_liste->setCurrentRow(-1);
    formuDoldur(Customer{});
    detayiTazele();
    m_unvanEdit->setFocus();
}

void PageCustomers::onSaveClicked()
{
    Customer c = formdanOku();

    if (c.unvan.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Eksik bilgi"),
                              QStringLiteral("Unvan boş olamaz."));
        m_unvanEdit->setFocus();
        return;
    }

    QString err;
    if (m_seciliId == 0) {
        if (!m_repoCustomers.add(c, &err)) {
            QMessageBox::warning(this, QStringLiteral("Kaydedilemedi"), err);
            return;
        }
        m_seciliId = c.id;
    } else {
        // Aktiflik bayrağı formda değil ayrı bir düğmede; kaydetme onu
        // sıfırlamamalı, bu yüzden mevcut değeri okunup korunur.
        const auto mevcut = m_repoCustomers.get(m_seciliId);
        c.aktif = mevcut.has_value() ? mevcut->aktif : true;

        if (!m_repoCustomers.update(c, &err)) {
            QMessageBox::warning(this, QStringLiteral("Kaydedilemedi"), err);
            return;
        }
    }

    loadListe();
    selectCustomerById(m_seciliId);
    emit customersChanged();
}

void PageCustomers::onToggleActiveClicked()
{
    if (m_seciliId == 0)
        return;

    const auto mevcut = m_repoCustomers.get(m_seciliId);
    if (!mevcut.has_value())
        return;

    const bool yeniDurum = !mevcut->aktif;

    if (!yeniDurum) {
        // Pasife alırken kaç teklifi olduğu söylenir: kullanıcı geçmişi
        // kaybetmediğini bilsin, ama etkiyi de görsün.
        const int adet = m_repoCustomers.quoteCount(m_seciliId);
        const QString mesaj =
            adet > 0 ? QStringLiteral("%1 müşterisinin %2 teklifi var.\n\nPasife alınırsa yeni "
                                       "tekliflerde listeye gelmez; mevcut teklifler korunur.\n\n"
                                       "Devam edilsin mi?")
                            .arg(mevcut->unvan)
                            .arg(adet)
                     : QStringLiteral("%1 pasife alınsın mı?").arg(mevcut->unvan);

        if (QMessageBox::question(this, QStringLiteral("Pasife al"), mesaj)
            != QMessageBox::Yes) {
            return;
        }
    }

    QString err;
    if (!m_repoCustomers.setActive(m_seciliId, yeniDurum, &err)) {
        QMessageBox::warning(this, QStringLiteral("Değiştirilemedi"), err);
        return;
    }

    loadListe();
    selectCustomerById(m_seciliId);
    onSelectionChanged();
    emit customersChanged();
}
