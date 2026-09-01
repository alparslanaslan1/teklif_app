#include "page_quote.h"

#include "core/calculator.h"
#include "core/numtowords.h"
#include "dlg_line_entry.h"
#include "item_search.h"
#include "print/company_logo.h"
#include "print/print_service.h"
#include "quote_line_model.h"
#include "quote_table_view.h"

#include <QComboBox>
#include <QDateEdit>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QPushButton>
#include <QShortcut>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace {

// Yeni bir teklifin varsayılan geçerlilik süresi (gün). Ayarlar ekranı
// gelene kadar sabit; şema da aynı varsayılanı kullanıyor.
constexpr int kVarsayilanGecerlilik = 15;

} // namespace

PageQuote::PageQuote(QSqlDatabase db, QWidget *parent)
    : QWidget(parent)
    , m_repoItems(db)
    , m_repoCustomers(db)
    , m_repoQuotes(db)
    , m_settings(db)
{
    setupUi();

    m_sartlarMetni = m_settings.valueOr(Settings::keyTermsText(), Settings::varsayilanSartlar());

    newQuote();
}

void PageQuote::setupUi()
{
    // --- Antet: müşteri, proje, tarih, geçerlilik ---------------------------
    m_customerCombo = new QComboBox(this);
    m_customerCombo->setObjectName(QStringLiteral("customerCombo"));
    m_customerCombo->setMinimumWidth(260);

    m_projeEdit = new QLineEdit(this);
    m_projeEdit->setObjectName(QStringLiteral("projeEdit"));
    m_projeEdit->setPlaceholderText(QStringLiteral("örn. Ofis tadilatı"));

    m_tarihEdit = new QDateEdit(this);
    m_tarihEdit->setObjectName(QStringLiteral("tarihEdit"));
    m_tarihEdit->setCalendarPopup(true);
    m_tarihEdit->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));

    m_gecerlilikSpin = new QSpinBox(this);
    m_gecerlilikSpin->setObjectName(QStringLiteral("gecerlilikSpin"));
    m_gecerlilikSpin->setRange(1, 365);
    m_gecerlilikSpin->setSuffix(QStringLiteral(" gün"));

    m_teklifNoLabel = new QLabel(this);
    m_teklifNoLabel->setObjectName(QStringLiteral("teklifNoLabel"));

    auto *antet = new QFormLayout;
    antet->addRow(QStringLiteral("Müşteri"), m_customerCombo);
    antet->addRow(QStringLiteral("Proje"), m_projeEdit);
    antet->addRow(QStringLiteral("Tarih"), m_tarihEdit);
    antet->addRow(QStringLiteral("Geçerlilik"), m_gecerlilikSpin);
    antet->addRow(QStringLiteral("Teklif No"), m_teklifNoLabel);

    // --- Arama kutusu -------------------------------------------------------
    m_search = new ItemSearch(this);
    // testler ve ileride diğer ekranlar bu adla bulur.
    m_search->setObjectName(QStringLiteral("itemSearch"));
    connect(m_search, &ItemSearch::itemChosen, this, &PageQuote::onItemChosen);

    // --- Kalem tablosu ------------------------------------------------------
    m_model = new QuoteLineModel(this);
    m_table = new QuoteTableView(this);
    m_table->setObjectName(QStringLiteral("quoteTable"));
    m_table->setModel(m_model);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(QuoteLineModel::ColAciklama,
                                                       QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    // Hücreye tek tıkla düzenlemeye girilir: kullanıcı çift tıklamayı
    // beklemek zorunda kalmasın (plan kararı: düzeltmeler hücreden yapılır).
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked
                              | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);

    connect(m_model, &QuoteLineModel::totalsMayHaveChanged, this, &PageQuote::recomputeTotals);

    // Del tuşu seçili satırı siler.
    auto *silKisayol = new QShortcut(QKeySequence(Qt::Key_Delete), m_table);
    silKisayol->setContext(Qt::WidgetShortcut);
    connect(silKisayol, &QShortcut::activated, this, &PageQuote::deleteCurrentRow);

    // --- Toplamlar ----------------------------------------------------------
    m_genelLabel = new QLabel(this);
    m_genelLabel->setObjectName(QStringLiteral("genelLabel"));
    QFont kalin = m_genelLabel->font();
    kalin.setBold(true);
    m_genelLabel->setFont(kalin);

    // Tek satır: KDV ayrıştırması yok, fiyatlara KDV dahil. Belgenin altındaki
    // şartlar metni bunu yazar (bkz. Settings::varsayilanSartlar).
    auto *toplamlar = new QFormLayout;
    toplamlar->addRow(QStringLiteral("Toplam"), m_genelLabel);

    auto *toplamKutu = new QGroupBox(QStringLiteral("Toplam"), this);
    auto *toplamLay = new QVBoxLayout(toplamKutu);
    toplamLay->addLayout(toplamlar);

    // --- Butonlar -----------------------------------------------------------
    m_saveButton = new QPushButton(QStringLiteral("Kaydet"), this);
    m_saveButton->setObjectName(QStringLiteral("saveButton"));
    m_saveButton->setShortcut(QKeySequence::Save);
    connect(m_saveButton, &QPushButton::clicked, this, &PageQuote::onSaveClicked);

    m_printButton = new QPushButton(QStringLiteral("Yazdır"), this);
    m_printButton->setObjectName(QStringLiteral("printButton"));
    m_printButton->setShortcut(QKeySequence::Print);
    connect(m_printButton, &QPushButton::clicked, this, &PageQuote::onPrintClicked);

    m_pdfButton = new QPushButton(QStringLiteral("PDF Kaydet"), this);
    m_pdfButton->setObjectName(QStringLiteral("pdfButton"));
    connect(m_pdfButton, &QPushButton::clicked, this, &PageQuote::onExportPdfClicked);

    auto *butonlar = new QHBoxLayout;
    butonlar->addStretch();
    butonlar->addWidget(m_saveButton);
    butonlar->addWidget(m_printButton);
    butonlar->addWidget(m_pdfButton);

    // --- Yerleşim -----------------------------------------------------------
    auto *ana = new QVBoxLayout(this);
    ana->addLayout(antet);
    ana->addWidget(m_search);
    ana->addWidget(m_table, /*stretch=*/1);
    ana->addWidget(toplamKutu);
    ana->addLayout(butonlar);
}

// ---------------------------------------------------------------------------
// Veri yükleme
// ---------------------------------------------------------------------------

void PageQuote::reloadSettings()
{
    // Ayarlar ekranında bir şey değiştiğinde çağrılır: şartlar metni ve
    // belge yazı boyutu bir sonraki teklifte geçerli olsun.
    if (m_quoteId == 0)
        m_sartlarMetni = m_settings.valueOr(Settings::keyTermsText(),
                                             Settings::varsayilanSartlar());
    recomputeTotals();
}

void PageQuote::reloadCatalog()
{
    QString err;
    const QVector<Item> katalog = m_repoItems.listAll(/*includeInactive=*/false, &err);
    if (!err.isEmpty()) {
        // Katalog okunamadıysa kullanıcı bunu bilmeli: arama kutusu sessizce
        // boş kalırsa "kalem yok" sanılır.
        QMessageBox::warning(this, QStringLiteral("Katalog"),
                              QStringLiteral("Katalog okunamadı: %1").arg(err));
        return;
    }
    m_search->setCatalog(katalog);
}

void PageQuote::reloadCustomers()
{
    QString err;
    m_customers = m_repoCustomers.listAll(/*includeInactive=*/false, &err);
    if (!err.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Müşteriler"),
                              QStringLiteral("Müşteri listesi okunamadı: %1").arg(err));
        return;
    }

    const qint64 oncekiId = currentCustomer().id;

    m_customerCombo->clear();
    // İlk satır bilinçli olarak boş: yeni teklifte hiçbir müşteri seçili
    // gelmez, kullanıcı bilerek seçmek zorundadır (yanlış müşteriye teklif
    // kaydetmek pahalı bir hatadır).
    m_customerCombo->addItem(QStringLiteral("— müşteri seçin —"), QVariant(qint64(0)));
    for (const Customer &c : m_customers)
        m_customerCombo->addItem(c.unvan, QVariant(c.id));

    if (oncekiId != 0)
        selectCustomerById(oncekiId);
}

void PageQuote::selectCustomerById(qint64 customerId)
{
    const int idx = m_customerCombo->findData(QVariant(customerId));
    if (idx >= 0)
        m_customerCombo->setCurrentIndex(idx);
}

Customer PageQuote::currentCustomer() const
{
    const qint64 id = m_customerCombo->currentData().toLongLong();
    for (const Customer &c : m_customers) {
        if (c.id == id)
            return c;
    }
    return Customer{};
}

// ---------------------------------------------------------------------------
// Teklif durumu
// ---------------------------------------------------------------------------

void PageQuote::newQuote()
{
    m_quoteId = 0;
    m_teklifNo.clear();
    m_model->clear();
    m_projeEdit->clear();
    m_tarihEdit->setDate(QDate::currentDate());
    m_gecerlilikSpin->setValue(kVarsayilanGecerlilik);
    m_customerCombo->setCurrentIndex(0);
    // Numara ancak kaydedilince atanır (sayaç transaction içinde artar),
    // bu yüzden kaydedilmemiş teklifte numara gösterilmez.
    m_teklifNoLabel->setText(QStringLiteral("(kaydedilmedi)"));
    recomputeTotals();
    m_search->focusInput();
}

bool PageQuote::loadQuote(qint64 id, QString *errorOut)
{
    const auto teklif = m_repoQuotes.get(id, errorOut);
    if (!teklif.has_value())
        return false;

    m_quoteId = teklif->id;
    m_teklifNo = teklif->teklifNo;
    m_teklifNoLabel->setText(m_teklifNo);
    m_projeEdit->setText(teklif->projeBasligi);
    m_tarihEdit->setDate(teklif->tarih.isValid() ? teklif->tarih : QDate::currentDate());
    m_gecerlilikSpin->setValue(teklif->gecerlilikGun > 0 ? teklif->gecerlilikGun
                                                          : kVarsayilanGecerlilik);

    // Kayıtlı teklif kendi şartlar metnini taşır.
    m_sartlarMetni = teklif->sartlarMetni;

    // Müşteri listede yoksa (pasife alınmışsa) yine de seçilebilsin diye
    // listeye geçici olarak eklenir; aksi halde teklif müşterisiz görünürdü.
    if (m_customerCombo->findData(QVariant(teklif->customerId)) < 0 && teklif->customerId != 0) {
        QString custErr;
        const QVector<Customer> hepsi = m_repoCustomers.listAll(/*includeInactive=*/true, &custErr);
        for (const Customer &c : hepsi) {
            if (c.id == teklif->customerId) {
                m_customers.append(c);
                m_customerCombo->addItem(c.unvan + QStringLiteral(" (pasif)"), QVariant(c.id));
                break;
            }
        }
    }
    selectCustomerById(teklif->customerId);

    m_model->setLines(teklif->satirlar);
    recomputeTotals();
    return true;
}

Quote PageQuote::currentQuoteSnapshot() const
{
    Quote q;
    q.id = m_quoteId;
    q.teklifNo = m_teklifNo;
    q.customerId = currentCustomer().id;
    q.tarih = m_tarihEdit->date();
    q.gecerlilikGun = m_gecerlilikSpin->value();
    q.projeBasligi = m_projeEdit->text().trimmed();
    // Şartlar metni ayarlardan gelir; teklif kaydedildiğinde metnin O ANKİ
    // hâli belgeye yazılır ve orada donar (ayar sonradan değişse bile eski
    // teklif kendi şartlarıyla basılır).
    q.sartlarMetni = m_sartlarMetni;
    // KDV AYRIŞTIRMASI YOK: fiyatlar KDV dahil girilir, belge altındaki
    // şartlar metni bunu belirtir. Şema sütunları duruyor ki KDV'li olarak
    // kaydedilmiş ESKİ teklifler kendi dökümüyle basılmaya devam etsin
    // (bkz. DocumentLayout::paintTotals).
    q.kdvOraniYuzde = 0;
    q.satirlar = m_model->lines();

    // Toplamlar ekranda gösterilenle AYNI kaynaktan hesaplanır; kaydedilen
    // ve basılan değerlerin ekrandakinden ayrışması mümkün olmasın diye.
    QVector<CalcLine> hesap;
    hesap.reserve(q.satirlar.size());
    for (const QuoteLine &l : q.satirlar)
        hesap.append(CalcLine{l.miktar, l.birimFiyat, Money(0)});

    const QuoteTotals t = Calculator::totals(hesap, q.kdvOraniYuzde);
    q.araToplam = t.araToplam;
    q.kdvTutari = t.kdvTutari;
    q.genelToplam = t.genelToplam;
    return q;
}

void PageQuote::recomputeTotals()
{
    const Quote q = currentQuoteSnapshot();
    m_genelLabel->setText(q.genelToplam.toString());
}

// ---------------------------------------------------------------------------
// Kaydetme
// ---------------------------------------------------------------------------

bool PageQuote::save(QString *errorOut)
{
    Quote q = currentQuoteSnapshot();

    if (q.customerId == 0) {
        if (errorOut)
            *errorOut = QStringLiteral("Kaydetmeden önce bir müşteri seçin.");
        return false;
    }

    if (m_quoteId == 0) {
        // Yeni teklif: numara add() içinde, sayaçla aynı transaction'da atanır.
        if (!m_repoQuotes.add(q, errorOut))
            return false;
        m_quoteId = q.id;
        m_teklifNo = q.teklifNo;
        m_teklifNoLabel->setText(m_teklifNo);
        return true;
    }

    return m_repoQuotes.update(q, errorOut);
}

void PageQuote::onSaveClicked()
{
    QString err;
    if (!save(&err)) {
        QMessageBox::warning(this, QStringLiteral("Kaydedilemedi"), err);
        return;
    }
    QMessageBox::information(this, QStringLiteral("Kaydedildi"),
                              QStringLiteral("Teklif %1 kaydedildi.").arg(m_teklifNo));
}

// ---------------------------------------------------------------------------
// Kalem ekleme / silme
// ---------------------------------------------------------------------------

void PageQuote::onItemChosen(const Item &item)
{
    LineEntryDialog dlg(item, this);
    if (dlg.exec() != QDialog::Accepted) {
        // Vazgeçildi: satır eklenmez, odak arama kutusuna döner.
        m_search->focusInput();
        return;
    }

    m_model->addLine(item, dlg.miktar(), dlg.birimFiyat(), dlg.satirNotu());
    // Ekleme sonrası odak arama kutusunda kalır: fareye dokunmadan arka
    // arkaya kalem girilebilsin diye (plan: 10 kalemlik teklif klavyeyle).
    m_search->focusInput();
}

void PageQuote::deleteCurrentRow()
{
    const QModelIndex idx = m_table->currentIndex();
    if (!idx.isValid())
        return;
    m_model->removeLine(idx.row());
}

// ---------------------------------------------------------------------------
// Yazdırma / PDF
// ---------------------------------------------------------------------------

DocumentContext PageQuote::buildDocumentContext() const
{
    DocumentContext ctx;
    ctx.quote = currentQuoteSnapshot();
    ctx.customer = currentCustomer();
    ctx.company = m_company;
    // Logo ayarlardan okunur. Yoksa null kalır ve antet logosuz düzene
    // geçer (bkz. print/document_layout.h).
    ctx.logo = CompanyLogo::load(m_settings);
    // Belge yazı boyutu ayarlardan; arayüz ölçeğinden bağımsızdır.
    ctx.fontPt = static_cast<int>(m_settings.intValueOr(Settings::keyDocumentFontPt(), 10));
    return ctx;
}

void PageQuote::onPrintClicked()
{
    const DocumentContext ctx = buildDocumentContext();

    QPrinter printer(QPrinter::HighResolution);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

    // Önizleme ve gerçek yazıcı AYNI PrintService::paint çağrısını kullanır —
    // önizlemede görünen ile kâğıttan çıkan arasında fark olamaz.
    QPrintPreviewDialog onizleme(&printer, this);
    onizleme.setWindowTitle(QStringLiteral("Baskı Önizleme"));

    QString hata;
    connect(&onizleme, &QPrintPreviewDialog::paintRequested, this,
            [&ctx, &hata](QPrinter *p) { PrintService::paint(ctx, p, &hata); });

    onizleme.exec();

    if (!hata.isEmpty())
        QMessageBox::warning(this, QStringLiteral("Yazdırılamadı"), hata);
}

void PageQuote::onExportPdfClicked()
{
    const DocumentContext ctx = buildDocumentContext();

    // Ayarlarda bir klasör seçilmişse oradan başla; yoksa Belgeler.
    QString klasor = m_settings.valueOr(Settings::keyPdfFolder());
    if (klasor.isEmpty())
        klasor = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

    const QString onerilen =
        klasor + QLatin1Char('/') + PrintService::suggestedFileName(ctx.quote, ctx.customer);

    const QString yol = QFileDialog::getSaveFileName(this, QStringLiteral("PDF Kaydet"), onerilen,
                                                      QStringLiteral("PDF dosyası (*.pdf)"));
    if (yol.isEmpty())
        return; // kullanıcı vazgeçti

    QString hata;
    if (!PrintService::exportPdf(ctx, yol, &hata)) {
        QMessageBox::warning(this, QStringLiteral("PDF kaydedilemedi"), hata);
        return;
    }
    QMessageBox::information(this, QStringLiteral("PDF kaydedildi"), yol);
}
