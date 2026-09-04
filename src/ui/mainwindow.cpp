#include "teklif/ui/mainwindow.h"

#include "teklif/ui/page_archive.h"
#include "teklif/ui/page_catalog.h"
#include "teklif/ui/page_settings.h"
#include "teklif/ui/theme.h"
#include "teklif/ui/update_prompt.h"
#include "teklif/core/version.h"
#include "teklif/ui/page_quote.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QApplication>
#include <QMenuBar>
#include <QMessageBox>
#include <QStackedWidget>
#include <QWidget>

namespace {
// Sol menünün genişliği ve satır yüksekliği.
constexpr int kNavGenislik = 190;
constexpr int kNavSatirYuksekligi = 40;
} // namespace

MainWindow::MainWindow(QSqlDatabase db, QWidget *parent) : QMainWindow(parent), m_db(db)
{
    setupUi(db);
    reloadCompanyInfo();
}

void MainWindow::setupUi(QSqlDatabase db)
{
    m_nav = new QListWidget(this);
    m_nav->setObjectName(QStringLiteral("navList"));
    m_nav->setFixedWidth(kNavGenislik);
    m_nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    for (const QString &ad : {QStringLiteral("Teklif"), QStringLiteral("Arşiv"),
                              QStringLiteral("Katalog"), QStringLiteral("Ayarlar")}) {
        auto *satir = new QListWidgetItem(ad, m_nav);
        // Yükseklik ELLE veriliyor: stil sayfasındaki ::item dolgusu satır
        // yüksekliğini BÜYÜTMEZ, yalnızca içeriği kaydırır. Verilmezse
        // satırlar yazı yüksekliği kadar kalır, seçili satırın vurgusu
        // komşularının üzerine taşar ve menü üst üste binmiş görünür.
        satir->setSizeHint(QSize(0, kNavSatirYuksekligi));
    }

    m_stack = new QStackedWidget(this);
    m_stack->setObjectName(QStringLiteral("pageStack"));

    m_pageQuote = new PageQuote(db, this);
    m_pageQuote->setObjectName(QStringLiteral("pageQuote"));
    m_pageArchive = new PageArchive(db, this);
    m_pageArchive->setObjectName(QStringLiteral("pageArchive"));
    m_pageCatalog = new PageCatalog(db, this);
    m_pageCatalog->setObjectName(QStringLiteral("pageCatalog"));
    m_pageSettings = new PageSettings(db, this);
    m_pageSettings->setObjectName(QStringLiteral("pageSettings"));

    // Sıra mainwindow.h'deki Page numaralandırmasıyla AYNI olmalı.
    m_stack->addWidget(m_pageQuote);
    m_stack->addWidget(m_pageArchive);
    m_stack->addWidget(m_pageCatalog);
    m_stack->addWidget(m_pageSettings);

    connect(m_nav, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0)
            return;
        m_stack->setCurrentIndex(row);
        // Sayfaya her girişte tazelenir: başka bir sayfada kaydedilen teklif
        // ya da eklenen katalog kalemi burada görünmelidir.
        if (row == PageArchiveIndex)
            m_pageArchive->refresh();
        else if (row == PageCatalogIndex)
            m_pageCatalog->refresh();
        else if (row == PageSettingsIndex)
            m_pageSettings->refresh();
    });

    // Arşivden gelen "bu teklifi aç" istekleri.
    connect(m_pageArchive, &PageArchive::quoteOpenRequested, this, &MainWindow::openQuote);
    connect(m_pageArchive, &PageArchive::quoteDuplicated, this, &MainWindow::openQuote);

    // Silinen teklif ekranda açıksa form boşaltılır; yoksa kullanıcı artık
    // var olmayan bir kaydı güncellemeye çalışır ve "teklif bulunamadı"
    // hatasıyla karşılaşır.
    connect(m_pageArchive, &PageArchive::quoteDeleted, this, [this](qint64 silinenId) {
        if (m_pageQuote->currentQuoteId() == silinenId)
            m_pageQuote->newQuote();
    });

    // Katalog değiştiğinde teklif ekranındaki arama indeksi bayatlamasın:
    // yeni eklenen kalem hemen aranabilir olmalı.
    connect(m_pageCatalog, &PageCatalog::catalogChanged, this,
            [this] { m_pageQuote->reloadCatalog(); });

    // Ayarlar kaydedilince antet ve şartlar metni tazelenir;
    // kullanıcı ayarları girip hemen yazdırabilmeli.
    connect(m_pageSettings, &PageSettings::companyInfoChanged, this, [this] {
        reloadCompanyInfo();
        m_pageQuote->reloadSettings();
    });

    auto *govde = new QWidget(this);
    auto *lay = new QHBoxLayout(govde);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_nav);
    lay->addWidget(m_stack, /*stretch=*/1);
    setCentralWidget(govde);

    // Katalogu BURADA bir kez yükle.
    //
    // Eskiden yalnızca iki yerden çağrılıyordu: ilk çalıştırma sihirbazından
    // sonra ve katalog ekranında bir değişiklik olduğunda. Yani programı
    // ikinci kez açan kullanıcının teklif ekranındaki arama kutusu BOŞTU —
    // katalogda kayıtlı malzemeler hiç çıkmıyordu, ta ki Katalog ekranına
    // girip bir şey değiştirene kadar.
    m_pageQuote->reloadCatalog();

    m_nav->setCurrentRow(PageQuoteIndex);
}

void MainWindow::setCompanyInfo(const CompanyInfo &company)
{
    m_pageQuote->setCompanyInfo(company);
}

void MainWindow::reloadCompanyInfo()
{
    Settings settings(m_db);
    CompanyInfo firma;
    firma.unvan = settings.valueOr(Settings::keyCompanyName());
    firma.yetkiBelgesi = settings.valueOr(Settings::keyCompanyLicence());
    firma.adres = settings.valueOr(Settings::keyCompanyAddress());
    firma.telefon = settings.valueOr(Settings::keyCompanyPhone());
    firma.email = settings.valueOr(Settings::keyCompanyEmail());
    firma.vergiDairesi = settings.valueOr(Settings::keyCompanyTaxOffice());
    firma.vergiNo = settings.valueOr(Settings::keyCompanyTaxNo());
    m_pageQuote->setCompanyInfo(firma);
}

void MainWindow::setupUpdates(const QUrl &manifestUrl, const QString &currentVersion)
{
    if (manifestUrl.isEmpty())
        return; // güncelleme yapılandırılmamış

    m_updatePrompt = new UpdatePrompt(m_db, manifestUrl, currentVersion, this);

    // Kurulum başlatıldığında program kapanmalı: installer çalışan exe'nin
    // üzerine yazamaz.
    connect(m_updatePrompt, &UpdatePrompt::quitRequested, qApp, &QApplication::quit);

    auto *yardim = menuBar()->addMenu(QStringLiteral("&Yardım"));
    yardim->addAction(QStringLiteral("Güncellemeleri denetle"), m_updatePrompt,
                       &UpdatePrompt::checkNow);
    yardim->addAction(QStringLiteral("Hakkında"), this, [this, currentVersion] {
        QMessageBox::about(this, QStringLiteral("Hakkında"),
                            QStringLiteral("<b>Teklif</b><br>Sürüm %1<br><br>"
                                            "Derleme: %2<br>Kaynak: %3")
                                .arg(currentVersion, QStringLiteral(APP_BUILD_DATE),
                                     QStringLiteral(APP_GIT_SHA)));
    });

    // Açılış denetimi pencere gösterildikten sonra, olay döngüsü
    // çalışmaya başlayınca yapılır; ağ beklemesi açılışı geciktirmesin.
    QMetaObject::invokeMethod(m_updatePrompt, &UpdatePrompt::checkOnStartup,
                               Qt::QueuedConnection);
}

void MainWindow::reloadAfterFirstRun()
{
    reloadCompanyInfo();
    m_pageQuote->reloadSettings();
    m_pageQuote->reloadCatalog();
    m_pageCatalog->refresh();
    m_pageSettings->refresh();
}

void MainWindow::showPage(Page page)
{
    m_nav->setCurrentRow(static_cast<int>(page));
}

void MainWindow::openQuote(qint64 quoteId)
{
    QString err;
    if (!m_pageQuote->loadQuote(quoteId, &err)) {
        QMessageBox::warning(this, QStringLiteral("Teklif açılamadı"), err);
        return;
    }
    showPage(PageQuoteIndex);
}
