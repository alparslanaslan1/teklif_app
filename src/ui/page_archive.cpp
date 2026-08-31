#include "page_archive.h"

#include "core/quote_status.h"
#include "quote_summary_model.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

PageArchive::PageArchive(QSqlDatabase db, QWidget *parent)
    : QWidget(parent)
    , m_repoQuotes(db)
    , m_repoCustomers(db)
{
    setupUi();
    refresh();
}

void PageArchive::setupUi()
{
    // --- Filtreler ----------------------------------------------------------
    m_customerCombo = new QComboBox(this);
    m_customerCombo->setObjectName(QStringLiteral("arsivMusteriCombo"));
    m_customerCombo->setMinimumWidth(200);

    m_durumCombo = new QComboBox(this);
    m_durumCombo->setObjectName(QStringLiteral("arsivDurumCombo"));

    // Tarih aralığı varsayılan olarak KAPALI: arşiv ilk açıldığında tüm
    // teklifler görünmeli, kullanıcı bir aralık seçmeye zorlanmamalı.
    m_tarihCheck = new QCheckBox(QStringLiteral("Tarih aralığı"), this);
    m_tarihCheck->setObjectName(QStringLiteral("arsivTarihCheck"));

    m_tarihBas = new QDateEdit(this);
    m_tarihBas->setObjectName(QStringLiteral("arsivTarihBas"));
    m_tarihBas->setCalendarPopup(true);
    m_tarihBas->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    m_tarihBas->setDate(QDate::currentDate().addMonths(-3));
    m_tarihBas->setEnabled(false);

    m_tarihBit = new QDateEdit(this);
    m_tarihBit->setObjectName(QStringLiteral("arsivTarihBit"));
    m_tarihBit->setCalendarPopup(true);
    m_tarihBit->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    m_tarihBit->setDate(QDate::currentDate());
    m_tarihBit->setEnabled(false);

    connect(m_tarihCheck, &QCheckBox::toggled, this, [this](bool acik) {
        m_tarihBas->setEnabled(acik);
        m_tarihBit->setEnabled(acik);
        applyFilter();
    });

    m_aramaEdit = new QLineEdit(this);
    m_aramaEdit->setObjectName(QStringLiteral("arsivAramaEdit"));
    m_aramaEdit->setPlaceholderText(QStringLiteral("Teklif no, müşteri veya proje ara..."));
    m_aramaEdit->setClearButtonEnabled(true);

    auto *temizle = new QPushButton(QStringLiteral("Filtreyi temizle"), this);
    connect(temizle, &QPushButton::clicked, this, &PageArchive::clearFilter);

    // Filtre değişince liste anında tazelenir; ayrı bir "Uygula" düğmesi
    // fazladan bir tıklama olurdu.
    connect(m_customerCombo, &QComboBox::currentIndexChanged, this, &PageArchive::applyFilter);
    connect(m_durumCombo, &QComboBox::currentIndexChanged, this, &PageArchive::applyFilter);
    connect(m_tarihBas, &QDateEdit::dateChanged, this, &PageArchive::applyFilter);
    connect(m_tarihBit, &QDateEdit::dateChanged, this, &PageArchive::applyFilter);
    connect(m_aramaEdit, &QLineEdit::textChanged, this, &PageArchive::applyFilter);

    auto *filtreSatir1 = new QHBoxLayout;
    filtreSatir1->addWidget(new QLabel(QStringLiteral("Müşteri"), this));
    filtreSatir1->addWidget(m_customerCombo);
    filtreSatir1->addWidget(new QLabel(QStringLiteral("Durum"), this));
    filtreSatir1->addWidget(m_durumCombo);
    filtreSatir1->addStretch();

    auto *filtreSatir2 = new QHBoxLayout;
    filtreSatir2->addWidget(m_tarihCheck);
    filtreSatir2->addWidget(m_tarihBas);
    filtreSatir2->addWidget(new QLabel(QStringLiteral("—"), this));
    filtreSatir2->addWidget(m_tarihBit);
    filtreSatir2->addWidget(m_aramaEdit, /*stretch=*/1);
    filtreSatir2->addWidget(temizle);

    auto *filtreKutu = new QGroupBox(QStringLiteral("Filtre"), this);
    auto *filtreLay = new QVBoxLayout(filtreKutu);
    filtreLay->addLayout(filtreSatir1);
    filtreLay->addLayout(filtreSatir2);

    // --- Tablo --------------------------------------------------------------
    m_model = new QuoteSummaryModel(this);
    m_table = new QTableView(this);
    m_table->setObjectName(QStringLiteral("arsivTable"));
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(QuoteSummaryModel::ColMusteri,
                                                       QHeaderView::Stretch);
    m_table->setSortingEnabled(false); // sıralama SQL'de (en yeni önce)

    // Çift tıklama en doğal "aç" hareketi.
    connect(m_table, &QTableView::doubleClicked, this, &PageArchive::openSelected);
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this] { updateButtons(); });

    // --- Komutlar -----------------------------------------------------------
    m_acButton = new QPushButton(QStringLiteral("Aç"), this);
    m_acButton->setObjectName(QStringLiteral("arsivAcButton"));
    connect(m_acButton, &QPushButton::clicked, this, &PageArchive::openSelected);

    m_kopyalaButton = new QPushButton(QStringLiteral("Kopyala"), this);
    m_kopyalaButton->setObjectName(QStringLiteral("arsivKopyalaButton"));
    connect(m_kopyalaButton, &QPushButton::clicked, this, &PageArchive::duplicateSelected);

    m_durumButton = new QPushButton(QStringLiteral("Durum değiştir"), this);
    m_durumButton->setObjectName(QStringLiteral("arsivDurumButton"));
    connect(m_durumButton, &QPushButton::clicked, this, &PageArchive::changeStatusOfSelected);

    m_ozetLabel = new QLabel(this);
    m_ozetLabel->setObjectName(QStringLiteral("arsivOzetLabel"));

    auto *komutlar = new QHBoxLayout;
    komutlar->addWidget(m_ozetLabel);
    komutlar->addStretch();
    komutlar->addWidget(m_acButton);
    komutlar->addWidget(m_kopyalaButton);
    komutlar->addWidget(m_durumButton);

    auto *ana = new QVBoxLayout(this);
    ana->addWidget(filtreKutu);
    ana->addWidget(m_table, /*stretch=*/1);
    ana->addLayout(komutlar);

    updateButtons();
}

void PageArchive::refresh()
{
    // Müşteri ve durum kutuları yeniden kurulurken currentIndexChanged
    // tetiklenip applyFilter'ı erkenden çağırmasın diye sinyaller kapatılır;
    // sonunda zaten bir kez uygulanıyor.
    const QSignalBlocker b1(m_customerCombo);
    const QSignalBlocker b2(m_durumCombo);

    const qint64 oncekiMusteri = m_customerCombo->currentData().toLongLong();
    const QString oncekiDurum = m_durumCombo->currentData().toString();

    m_customerCombo->clear();
    m_customerCombo->addItem(QStringLiteral("Tüm müşteriler"), QVariant(qint64(0)));
    QString err;
    // Pasif müşteriler DE listelenir: eski teklifleri arşivde aranabilmeli.
    for (const Customer &c : m_repoCustomers.listAll(/*includeInactive=*/true, &err))
        m_customerCombo->addItem(c.unvan, QVariant(c.id));

    m_durumCombo->clear();
    m_durumCombo->addItem(QStringLiteral("Tüm durumlar"), QString());
    for (const QString &d : QuoteStatus::all())
        m_durumCombo->addItem(d, d);

    const int mIdx = m_customerCombo->findData(QVariant(oncekiMusteri));
    if (mIdx >= 0)
        m_customerCombo->setCurrentIndex(mIdx);
    const int dIdx = m_durumCombo->findData(oncekiDurum);
    if (dIdx >= 0)
        m_durumCombo->setCurrentIndex(dIdx);

    applyFilter();
}

QuoteFilter PageArchive::currentFilter() const
{
    QuoteFilter f;
    f.customerId = m_customerCombo->currentData().toLongLong();
    f.durum = m_durumCombo->currentData().toString();
    f.aranan = m_aramaEdit->text();
    if (m_tarihCheck->isChecked()) {
        f.tarihBaslangic = m_tarihBas->date();
        f.tarihBitis = m_tarihBit->date();
    }
    return f;
}

void PageArchive::applyFilter()
{
    QString err;
    const QVector<QuoteSummary> liste = m_repoQuotes.list(currentFilter(), &err);
    if (!err.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Arşiv"),
                              QStringLiteral("Teklif listesi okunamadı: %1").arg(err));
        return;
    }
    m_model->setQuotes(liste);

    // Listelenen tekliflerin toplamı: "bu müşteriye bu yıl ne kadarlık teklif
    // verdim" sorusu filtreyle birlikte anında cevaplanır.
    Money toplam;
    for (const QuoteSummary &s : liste)
        toplam += s.genelToplam;
    m_ozetLabel->setText(QStringLiteral("%1 teklif · toplam %2")
                              .arg(liste.size())
                              .arg(toplam.toString()));

    updateButtons();
}

void PageArchive::clearFilter()
{
    const QSignalBlocker b1(m_customerCombo);
    const QSignalBlocker b2(m_durumCombo);
    const QSignalBlocker b3(m_aramaEdit);
    const QSignalBlocker b4(m_tarihCheck);

    m_customerCombo->setCurrentIndex(0);
    m_durumCombo->setCurrentIndex(0);
    m_aramaEdit->clear();
    m_tarihCheck->setChecked(false);
    m_tarihBas->setEnabled(false);
    m_tarihBit->setEnabled(false);

    applyFilter();
}

qint64 PageArchive::selectedQuoteId() const
{
    const QModelIndex idx = m_table->currentIndex();
    if (!idx.isValid())
        return 0;
    return m_model->at(idx.row()).id;
}

void PageArchive::updateButtons()
{
    const bool secili = selectedQuoteId() != 0;
    m_acButton->setEnabled(secili);
    m_kopyalaButton->setEnabled(secili);
    m_durumButton->setEnabled(secili);
}

void PageArchive::openSelected()
{
    const qint64 id = selectedQuoteId();
    if (id == 0)
        return;
    emit quoteOpenRequested(id);
}

void PageArchive::duplicateSelected()
{
    const qint64 id = selectedQuoteId();
    if (id == 0)
        return;

    QString err;
    const auto kopya = m_repoQuotes.duplicate(id, &err);
    if (!kopya.has_value()) {
        QMessageBox::warning(this, QStringLiteral("Kopyalanamadı"), err);
        return;
    }

    applyFilter(); // yeni teklif listede görünsün
    // Kopya çıkarmanın amacı neredeyse her zaman onu düzenlemektir; doğrudan
    // açılır.
    emit quoteDuplicated(kopya->id);
}

void PageArchive::changeStatusOfSelected()
{
    const qint64 id = selectedQuoteId();
    if (id == 0)
        return;

    const QuoteSummary mevcut = m_model->at(m_table->currentIndex().row());
    const QStringList durumlar = QuoteStatus::all();

    bool tamam = false;
    const QString yeni = QInputDialog::getItem(
        this, QStringLiteral("Durum değiştir"),
        QStringLiteral("%1 numaralı teklifin durumu:").arg(mevcut.teklifNo), durumlar,
        qMax(0, durumlar.indexOf(mevcut.durum)), /*editable=*/false, &tamam);
    if (!tamam)
        return;

    QString err;
    if (!m_repoQuotes.setStatus(id, yeni, &err)) {
        QMessageBox::warning(this, QStringLiteral("Durum değiştirilemedi"), err);
        return;
    }
    applyFilter();
}
