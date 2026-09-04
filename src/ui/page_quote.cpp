#include "teklif/ui/page_quote.h"

#include "teklif/core/calculator.h"
#include "teklif/core/numtowords.h"
#include "teklif/ui/item_search.h"
#include "teklif/print/company_logo.h"
#include "teklif/print/print_service.h"
#include "teklif/ui/quote_line_model.h"
#include "teklif/ui/quote_table_view.h"

#include <QDateEdit>
#include <QItemSelectionModel>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
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

#include <algorithm>

namespace {

// Yeni bir teklifin varsayılan geçerlilik süresi (gün).
constexpr int kVarsayilanGecerlilik = 15;

// Katalogdan seçilen kalemin başlangıç miktarı. Doğrudan eklenir, araya
// bir diyalog girmez: miktar ve fiyat zaten tablodan düzeltilebiliyor,
// her kalem için pencere açıp kapatmak akışı yavaşlatıyordu.
constexpr double kKatalogVarsayilanMiktar = 1.0;

// Kalem tablosunun satır yüksekliği (piksel).
constexpr int kSatirYuksekligi = 30;

// Tek satırlık müşteri alanı kurar. Yedi alanın hepsi aynı kalıpta
// olduğu için tek yerde toplanmıştır.
QLineEdit *musteriAlani(QWidget *parent, const QString &nesneAdi, const QString &ipucu)
{
    auto *e = new QLineEdit(parent);
    e->setObjectName(nesneAdi);
    e->setPlaceholderText(ipucu);
    e->setClearButtonEnabled(true);
    return e;
}

} // namespace

PageQuote::PageQuote(QSqlDatabase db, QWidget *parent)
    : QWidget(parent)
    , m_repoItems(db)
    , m_repoQuotes(db)
    , m_settings(db)
{
    setupUi();

    m_sartlarMetni = m_settings.valueOr(Settings::keyTermsText(), Settings::varsayilanSartlar());

    newQuote();
}

void PageQuote::setupUi()
{
    // --- Müşteri bilgileri --------------------------------------------------
    // Ayrı bir müşteri kaydı yok; bu alanlar doğrudan teklifin içine yazılır.
    m_musteriUnvan = musteriAlani(this, QStringLiteral("musteriUnvan"),
                                   QStringLiteral("Firma ya da kişi adı (zorunlu)"));
    m_musteriYetkili = musteriAlani(this, QStringLiteral("musteriYetkili"),
                                     QStringLiteral("Yetkili kişi"));
    m_musteriTelefon = musteriAlani(this, QStringLiteral("musteriTelefon"),
                                     QStringLiteral("Telefon"));
    m_musteriEmail = musteriAlani(this, QStringLiteral("musteriEmail"),
                                   QStringLiteral("E-posta"));
    m_musteriAdres = musteriAlani(this, QStringLiteral("musteriAdres"),
                                   QStringLiteral("Adres"));
    m_musteriVergiDairesi = musteriAlani(this, QStringLiteral("musteriVergiDairesi"),
                                          QStringLiteral("Vergi dairesi"));
    m_musteriVergiNo = musteriAlani(this, QStringLiteral("musteriVergiNo"),
                                     QStringLiteral("Vergi / TC no"));

    // İki sütunlu ızgara: form dikeyde uzayıp tabloyu ezmesin.
    auto *musteriIzgara = new QGridLayout;
    musteriIzgara->setHorizontalSpacing(16);
    musteriIzgara->addWidget(new QLabel(QStringLiteral("Ünvan"), this), 0, 0);
    musteriIzgara->addWidget(m_musteriUnvan, 0, 1);
    musteriIzgara->addWidget(new QLabel(QStringLiteral("Yetkili"), this), 0, 2);
    musteriIzgara->addWidget(m_musteriYetkili, 0, 3);
    musteriIzgara->addWidget(new QLabel(QStringLiteral("Telefon"), this), 1, 0);
    musteriIzgara->addWidget(m_musteriTelefon, 1, 1);
    musteriIzgara->addWidget(new QLabel(QStringLiteral("E-posta"), this), 1, 2);
    musteriIzgara->addWidget(m_musteriEmail, 1, 3);
    musteriIzgara->addWidget(new QLabel(QStringLiteral("Adres"), this), 2, 0);
    musteriIzgara->addWidget(m_musteriAdres, 2, 1, 1, 3);
    musteriIzgara->addWidget(new QLabel(QStringLiteral("Vergi D."), this), 3, 0);
    musteriIzgara->addWidget(m_musteriVergiDairesi, 3, 1);
    musteriIzgara->addWidget(new QLabel(QStringLiteral("Vergi No"), this), 3, 2);
    musteriIzgara->addWidget(m_musteriVergiNo, 3, 3);
    musteriIzgara->setColumnStretch(1, 1);
    musteriIzgara->setColumnStretch(3, 1);

    auto *musteriKutu = new QGroupBox(QStringLiteral("Müşteri"), this);
    musteriKutu->setObjectName(QStringLiteral("musteriKutu"));
    auto *musteriLay = new QVBoxLayout(musteriKutu);
    musteriLay->addLayout(musteriIzgara);

    // --- Teklif bilgileri ---------------------------------------------------
    m_projeEdit = new QLineEdit(this);
    m_projeEdit->setObjectName(QStringLiteral("projeEdit"));
    m_projeEdit->setPlaceholderText(QStringLiteral("örn. Doğalgaz iç tesisat"));

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

    auto *teklifIzgara = new QGridLayout;
    teklifIzgara->setHorizontalSpacing(16);
    teklifIzgara->addWidget(new QLabel(QStringLiteral("Proje"), this), 0, 0);
    teklifIzgara->addWidget(m_projeEdit, 0, 1, 1, 3);
    teklifIzgara->addWidget(new QLabel(QStringLiteral("Tarih"), this), 1, 0);
    teklifIzgara->addWidget(m_tarihEdit, 1, 1);
    teklifIzgara->addWidget(new QLabel(QStringLiteral("Geçerlilik"), this), 1, 2);
    teklifIzgara->addWidget(m_gecerlilikSpin, 1, 3);
    teklifIzgara->addWidget(new QLabel(QStringLiteral("Teklif No"), this), 2, 0);
    teklifIzgara->addWidget(m_teklifNoLabel, 2, 1, 1, 3);
    teklifIzgara->setColumnStretch(1, 1);
    teklifIzgara->setColumnStretch(3, 1);

    auto *teklifKutu = new QGroupBox(QStringLiteral("Teklif"), this);
    teklifKutu->setObjectName(QStringLiteral("teklifKutu"));
    auto *teklifLay = new QVBoxLayout(teklifKutu);
    teklifLay->addLayout(teklifIzgara);

    auto *ustSatir = new QHBoxLayout;
    ustSatir->addWidget(musteriKutu, /*stretch=*/3);
    ustSatir->addWidget(teklifKutu, /*stretch=*/2);

    // --- Kalem araç çubuğu: arama + satır ekle/sil --------------------------
    m_search = new ItemSearch(this);
    m_search->setObjectName(QStringLiteral("itemSearch"));
    connect(m_search, &ItemSearch::itemChosen, this, &PageQuote::onItemChosen);

    // Katalogda olmayan bir kalem için katalog kaydı açmak zorunda kalmamalı:
    // "+" boş bir satır açar, kullanıcı hücreleri kendisi doldurur.
    m_satirEkleButton = new QPushButton(QStringLiteral("+  Satır ekle"), this);
    m_satirEkleButton->setObjectName(QStringLiteral("satirEkleButton"));
    m_satirEkleButton->setToolTip(QStringLiteral("Boş satır ekle (Ctrl+N)"));
    m_satirEkleButton->setShortcut(QKeySequence(QStringLiteral("Ctrl+N")));
    connect(m_satirEkleButton, &QPushButton::clicked, this, &PageQuote::addEmptyRow);

    m_satirSilButton = new QPushButton(QStringLiteral("−  Satır sil"), this);
    m_satirSilButton->setObjectName(QStringLiteral("satirSilButton"));
    m_satirSilButton->setToolTip(QStringLiteral("Seçili satırı sil (Del)"));
    // Silme geri alınamaz; tema bu özelliğe bakıp uyarı rengiyle çizer.
    m_satirSilButton->setProperty("tehlike", true);
    connect(m_satirSilButton, &QPushButton::clicked, this, &PageQuote::deleteSelectedRows);

    auto *aracCubugu = new QHBoxLayout;
    aracCubugu->addWidget(m_search, /*stretch=*/1);
    aracCubugu->addWidget(m_satirEkleButton);
    aracCubugu->addWidget(m_satirSilButton);

    // --- Kalem tablosu ------------------------------------------------------
    m_model = new QuoteLineModel(this);
    m_table = new QuoteTableView(this);
    m_table->setObjectName(QStringLiteral("quoteTable"));
    m_table->setModel(m_model);
    m_table->horizontalHeader()->setStretchLastSection(false);
    // Sayı sütunları içeriğine göre daralır; boşluğu açıklama ve not paylaşır.
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(QuoteLineModel::ColAciklama,
                                                       QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(QuoteLineModel::ColNot,
                                                       QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    // Satır yüksekliği yazıya göre değil sabit: düzenleyici (QLineEdit)
    // hücrenin dikdörtgenine sığdırıldığı için dar satırda yazı kırpılıyordu.
    m_table->verticalHeader()->setDefaultSectionSize(kSatirYuksekligi);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    // Birden fazla satır seçilip tek seferde silinebilir (Ctrl/Shift ile).
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setAlternatingRowColors(true);
    // Hücreye tek tıkla ya da yazmaya başlayınca düzenlemeye girilir:
    // satırların çoğu artık elle dolduruluyor, çift tıklama beklemek
    // gereksiz bir adım olurdu.
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked
                              | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);

    connect(m_model, &QuoteLineModel::totalsMayHaveChanged, this, &PageQuote::recomputeTotals);

    // Düğmenin etkinliği seçimi izler.
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &PageQuote::updateRowButtons);
    connect(m_model, &QAbstractItemModel::modelReset, this, &PageQuote::updateRowButtons);
    connect(m_model, &QAbstractItemModel::rowsInserted, this, &PageQuote::updateRowButtons);
    connect(m_model, &QAbstractItemModel::rowsRemoved, this, &PageQuote::updateRowButtons);

    // Del tuşu seçili satırları siler.
    auto *silKisayol = new QShortcut(QKeySequence(Qt::Key_Delete), m_table);
    silKisayol->setContext(Qt::WidgetShortcut);
    connect(silKisayol, &QShortcut::activated, this, &PageQuote::deleteSelectedRows);

    // --- Toplam -------------------------------------------------------------
    m_genelLabel = new QLabel(this);
    m_genelLabel->setObjectName(QStringLiteral("genelLabel"));

    auto *toplamSatir = new QHBoxLayout;
    toplamSatir->addStretch();
    auto *toplamBaslik = new QLabel(QStringLiteral("GENEL TOPLAM"), this);
    toplamBaslik->setObjectName(QStringLiteral("toplamBaslik"));
    toplamSatir->addWidget(toplamBaslik);
    toplamSatir->addWidget(m_genelLabel);

    // --- Butonlar -----------------------------------------------------------
    m_saveButton = new QPushButton(QStringLiteral("Kaydet"), this);
    m_saveButton->setObjectName(QStringLiteral("saveButton"));
    // Ana eylem; tema bu özelliğe bakıp vurgulu çizer (bkz. ui/theme.cpp).
    m_saveButton->setProperty("birincil", true);
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
    butonlar->addWidget(m_pdfButton);
    butonlar->addWidget(m_printButton);
    butonlar->addWidget(m_saveButton);

    // --- Yerleşim -----------------------------------------------------------
    // Sayfa kenar boşlukları. Varsayılan (9 px) kart görünümü için dardı:
    // QGroupBox başlığı kutunun üst kenarının üzerine oturduğu için üstte
    // yer kalmıyor ve başlık kırpılıyordu.
    auto *ana = new QVBoxLayout(this);
    ana->setContentsMargins(18, 20, 18, 16);
    ana->setSpacing(12);
    ana->addLayout(ustSatir);
    ana->addLayout(aracCubugu);
    ana->addWidget(m_table, /*stretch=*/1);
    ana->addLayout(toplamSatir);
    ana->addLayout(butonlar);

    updateRowButtons();
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

Customer PageQuote::currentCustomer() const
{
    Customer m;
    m.unvan = m_musteriUnvan->text().trimmed();
    m.yetkili = m_musteriYetkili->text().trimmed();
    m.telefon = m_musteriTelefon->text().trimmed();
    m.email = m_musteriEmail->text().trimmed();
    m.adres = m_musteriAdres->text().trimmed();
    m.vergiDairesi = m_musteriVergiDairesi->text().trimmed();
    m.vergiNo = m_musteriVergiNo->text().trimmed();
    return m;
}

void PageQuote::setCustomer(const Customer &musteri)
{
    m_musteriUnvan->setText(musteri.unvan);
    m_musteriYetkili->setText(musteri.yetkili);
    m_musteriTelefon->setText(musteri.telefon);
    m_musteriEmail->setText(musteri.email);
    m_musteriAdres->setText(musteri.adres);
    m_musteriVergiDairesi->setText(musteri.vergiDairesi);
    m_musteriVergiNo->setText(musteri.vergiNo);
}

// ---------------------------------------------------------------------------
// Teklif durumu
// ---------------------------------------------------------------------------

void PageQuote::newQuote()
{
    m_quoteId = 0;
    m_teklifNo.clear();
    m_model->clear();
    setCustomer(Customer{});
    m_projeEdit->clear();
    m_tarihEdit->setDate(QDate::currentDate());
    m_gecerlilikSpin->setValue(kVarsayilanGecerlilik);
    // Numara ancak kaydedilince atanır (sayaç transaction içinde artar),
    // bu yüzden kaydedilmemiş teklifte numara gösterilmez.
    m_teklifNoLabel->setText(QStringLiteral("(kaydedilmedi)"));
    recomputeTotals();
    // Yeni teklifte ilk iş müşteriyi yazmaktır; imleç oraya gider.
    m_musteriUnvan->setFocus();
}

bool PageQuote::loadQuote(qint64 id, QString *errorOut)
{
    const auto teklif = m_repoQuotes.get(id, errorOut);
    if (!teklif.has_value())
        return false;

    m_quoteId = teklif->id;
    m_teklifNo = teklif->teklifNo;
    m_teklifNoLabel->setText(m_teklifNo);
    setCustomer(teklif->musteri);
    m_projeEdit->setText(teklif->projeBasligi);
    m_tarihEdit->setDate(teklif->tarih.isValid() ? teklif->tarih : QDate::currentDate());
    m_gecerlilikSpin->setValue(teklif->gecerlilikGun > 0 ? teklif->gecerlilikGun
                                                          : kVarsayilanGecerlilik);

    // Kayıtlı teklif kendi şartlar metnini taşır.
    m_sartlarMetni = teklif->sartlarMetni;

    m_model->setLines(teklif->satirlar);
    recomputeTotals();
    return true;
}

Quote PageQuote::currentQuoteSnapshot() const
{
    Quote q;
    q.id = m_quoteId;
    q.teklifNo = m_teklifNo;
    q.musteri = currentCustomer();
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
    // Açıklaması boş satırlar dışarıda kalır: kullanıcı "+" ile açtığı ama
    // doldurmadığı bir satırı ekranda bırakmış olabilir, belgeye boş satır
    // girmemeli.
    q.satirlar = m_model->filledLines();

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

    // Unvan tek zorunlu alan: belgenin muhatabı belirsiz olamaz. Diğer
    // müşteri alanları boş bırakılabilir, antette o satırlar hiç basılmaz.
    if (q.musteri.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Kaydetmeden önce müşteri ünvanını yazın.");
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
        // Eksik olan müşteri unvanıysa imleci oraya götür: kullanıcı hatayı
        // okuyup nereye yazacağını aramasın.
        if (currentCustomer().isEmpty())
            m_musteriUnvan->setFocus();
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
    // Diyalog YOK: kalem miktar 1 ve katalog fiyatıyla doğrudan eklenir,
    // düzeltme gerekiyorsa tablodan yapılır.
    const int row = m_model->addLine(item, kKatalogVarsayilanMiktar, item.varsayilanFiyat,
                                      QString());
    // İmleç miktar hücresine gider: katalogdan gelen satırda değiştirilecek
    // ilk şey neredeyse her zaman miktardır.
    m_table->setCurrentIndex(m_model->index(row, QuoteLineModel::ColMiktar));
    // Odak arama kutusunda kalır: fareye dokunmadan arka arkaya kalem
    // girilebilsin diye.
    m_search->focusInput();
}

void PageQuote::addEmptyRow()
{
    const int row = m_model->addEmptyLine();
    // Yeni satırda yazılacak ilk şey açıklamadır; imleç oraya konur ve
    // düzenleme doğrudan açılır — kullanıcı ayrıca tıklamak zorunda kalmasın.
    const QModelIndex idx = m_model->index(row, QuoteLineModel::ColAciklama);
    m_table->setCurrentIndex(idx);
    m_table->edit(idx);
}

void PageQuote::deleteSelectedRows()
{
    // Açık bir hücre düzenleyicisi varsa ÖNCE kapatılır. Aksi halde satır
    // silindikten sonra düzenleyici kapanır ve yazdığı değeri artık BAŞKA
    // bir satıra denk gelen indekse yazar — kullanıcı sildiği satırın
    // metninin bir alttaki satıra atladığını görürdü.
    if (QWidget *odak = focusWidget()) {
        if (m_table->isAncestorOf(odak))
            m_table->setFocus();
    }

    // Seçili satırlar; yoksa imlecin bulunduğu satır. İkisi de yoksa
    // yapacak bir şey yok.
    QList<int> satirlar;
    if (QItemSelectionModel *sec = m_table->selectionModel()) {
        const QModelIndexList secili = sec->selectedRows();
        for (const QModelIndex &i : secili)
            satirlar.append(i.row());
    }
    if (satirlar.isEmpty() && m_table->currentIndex().isValid())
        satirlar.append(m_table->currentIndex().row());
    if (satirlar.isEmpty())
        return;

    // SONDAN BAŞA: baştan silinseydi her silme sonrasında kalan satırların
    // numaraları kayar ve sonraki silme yanlış satıra denk gelirdi.
    std::sort(satirlar.begin(), satirlar.end(), std::greater<int>());
    for (int satir : satirlar)
        m_model->removeLine(satir);

    updateRowButtons();
}

void PageQuote::updateRowButtons()
{
    const QItemSelectionModel *sec = m_table->selectionModel();
    const bool secimVar = sec && (!sec->selectedRows().isEmpty() || sec->currentIndex().isValid());
    m_satirSilButton->setEnabled(secimVar && m_model->rowCount() > 0);
}

// ---------------------------------------------------------------------------
// Yazdırma / PDF
// ---------------------------------------------------------------------------

DocumentContext PageQuote::buildDocumentContext() const
{
    DocumentContext ctx;
    ctx.quote = currentQuoteSnapshot();
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
        klasor + QLatin1Char('/') + PrintService::suggestedFileName(ctx.quote);

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
