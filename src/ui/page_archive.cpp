#include "teklif/ui/page_archive.h"

#include "teklif/core/quote_status.h"
#include "teklif/ui/quote_summary_model.h"

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
{
    setupUi();
    refresh();
}

void PageArchive::setupUi()
{
    // --- Filtreler ----------------------------------------------------------
    // Müşteri seçici YOK: müşteri artık ayrı bir kayıt değil, teklifin kendi
    // alanı. Müşteriye göre listeleme aşağıdaki arama kutusuyla yapılır —
    // unvanın bir parçasını yazmak yeter.
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
    connect(m_durumCombo, &QComboBox::currentIndexChanged, this, &PageArchive::applyFilter);
    connect(m_tarihBas, &QDateEdit::dateChanged, this, &PageArchive::applyFilter);
    connect(m_tarihBit, &QDateEdit::dateChanged, this, &PageArchive::applyFilter);
    connect(m_aramaEdit, &QLineEdit::textChanged, this, &PageArchive::applyFilter);

    auto *filtreSatir1 = new QHBoxLayout;
    filtreSatir1->addWidget(m_aramaEdit, /*stretch=*/1);
    filtreSatir1->addWidget(new QLabel(QStringLiteral("Durum"), this));
    filtreSatir1->addWidget(m_durumCombo);

    auto *filtreSatir2 = new QHBoxLayout;
    filtreSatir2->addWidget(m_tarihCheck);
    filtreSatir2->addWidget(m_tarihBas);
    filtreSatir2->addWidget(new QLabel(QStringLiteral("—"), this));
    filtreSatir2->addWidget(m_tarihBit);
    filtreSatir2->addStretch();
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
    // Müşteri sütunu boşluğu doldurur, diğerleri içeriğine göre daralır —
    // sabit genişlikte kalsalardı "Genel Toplam" gibi geniş başlıklar
    // tablonun sağ kenarından taşıp kırpılıyordu.
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
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
    m_acButton->setProperty("birincil", true);
    connect(m_acButton, &QPushButton::clicked, this, &PageArchive::openSelected);

    m_kopyalaButton = new QPushButton(QStringLiteral("Kopyala"), this);
    m_kopyalaButton->setObjectName(QStringLiteral("arsivKopyalaButton"));
    connect(m_kopyalaButton, &QPushButton::clicked, this, &PageArchive::duplicateSelected);

    m_durumButton = new QPushButton(QStringLiteral("Durum değiştir"), this);
    m_durumButton->setObjectName(QStringLiteral("arsivDurumButton"));
    connect(m_durumButton, &QPushButton::clicked, this, &PageArchive::changeStatusOfSelected);

    m_silButton = new QPushButton(QStringLiteral("Sil"), this);
    m_silButton->setObjectName(QStringLiteral("arsivSilButton"));
    // Geri alınamaz eylem; tema bu özelliğe bakıp uyarı rengiyle çizer.
    m_silButton->setProperty("tehlike", true);
    connect(m_silButton, &QPushButton::clicked, this, &PageArchive::deleteSelected);

    m_ozetLabel = new QLabel(this);
    m_ozetLabel->setObjectName(QStringLiteral("arsivOzetLabel"));

    auto *komutlar = new QHBoxLayout;
    komutlar->addWidget(m_ozetLabel);
    komutlar->addStretch();
    komutlar->addWidget(m_acButton);
    komutlar->addWidget(m_kopyalaButton);
    komutlar->addWidget(m_durumButton);
    komutlar->addWidget(m_silButton);

    // Sayfa kenar boşlukları. Varsayılan (9 px) kart görünümü için dardı:
    // QGroupBox başlığı kutunun üst kenarının üzerine oturduğu için üstte
    // yer kalmıyor ve başlık kırpılıyordu.
    auto *ana = new QVBoxLayout(this);
    ana->setContentsMargins(18, 20, 18, 16);
    ana->setSpacing(12);
    ana->addWidget(filtreKutu);
    ana->addWidget(m_table, /*stretch=*/1);
    ana->addLayout(komutlar);

    updateButtons();
}

void PageArchive::refresh()
{
    // Durum kutusu yeniden kurulurken currentIndexChanged tetiklenip
    // applyFilter'ı erkenden çağırmasın diye sinyal kapatılır; sonunda
    // zaten bir kez uygulanıyor.
    const QSignalBlocker b2(m_durumCombo);

    const QString oncekiDurum = m_durumCombo->currentData().toString();

    m_durumCombo->clear();
    m_durumCombo->addItem(QStringLiteral("Tüm durumlar"), QString());
    for (const QString &d : QuoteStatus::all())
        m_durumCombo->addItem(d, d);

    const int dIdx = m_durumCombo->findData(oncekiDurum);
    if (dIdx >= 0)
        m_durumCombo->setCurrentIndex(dIdx);

    applyFilter();
}

QuoteFilter PageArchive::currentFilter() const
{
    QuoteFilter f;
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
    const QSignalBlocker b2(m_durumCombo);
    const QSignalBlocker b3(m_aramaEdit);
    const QSignalBlocker b4(m_tarihCheck);

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
    m_silButton->setEnabled(secili);
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

void PageArchive::deleteSelected()
{
    const qint64 id = selectedQuoteId();
    if (id == 0)
        return;

    const QuoteSummary mevcut = m_model->at(m_table->currentIndex().row());

    // Geri alınamaz bir işlem: onay metni hangi teklifin gideceğini AÇIKÇA
    // söyler. Yalnızca "emin misiniz?" demek, yanlış satır seçilmişse
    // kullanıcıyı korumaz.
    const QString mesaj =
        QStringLiteral("%1 numaralı teklif kalıcı olarak silinecek.\n\n"
                        "Müşteri: %2\nTarih: %3\nTutar: %4\n\n"
                        "Bu işlem geri alınamaz. Devam edilsin mi?")
            .arg(mevcut.teklifNo, mevcut.musteriUnvan,
                  mevcut.tarih.toString(QStringLiteral("dd.MM.yyyy")),
                  mevcut.genelToplam.toString());

    QMessageBox kutu(QMessageBox::Warning, QStringLiteral("Teklifi sil"), mesaj,
                      QMessageBox::NoButton, this);
    QPushButton *sil = kutu.addButton(QStringLiteral("Sil"), QMessageBox::DestructiveRole);
    kutu.addButton(QStringLiteral("Vazgeç"), QMessageBox::RejectRole);
    // Varsayılan düğme "Vazgeç": Enter'a refleksle basan kullanıcı yanlışlıkla
    // silmesin.
    kutu.setDefaultButton(qobject_cast<QPushButton *>(kutu.buttons().last()));
    kutu.exec();

    if (kutu.clickedButton() != sil)
        return;

    QString err;
    if (!m_repoQuotes.remove(id, &err)) {
        QMessageBox::warning(this, QStringLiteral("Silinemedi"), err);
        return;
    }

    applyFilter();
    emit quoteDeleted(id);
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
