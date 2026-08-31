#include "mainwindow.h"

#include "page_archive.h"
#include "page_catalog.h"
#include "page_customers.h"
#include "page_settings.h"
#include "theme.h"
#include "page_quote.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QMessageBox>
#include <QStackedWidget>
#include <QWidget>

MainWindow::MainWindow(QSqlDatabase db, QWidget *parent) : QMainWindow(parent), m_db(db)
{
    setupUi(db);
    reloadCompanyInfo();
}

void MainWindow::setupUi(QSqlDatabase db)
{
    m_nav = new QListWidget(this);
    m_nav->setObjectName(QStringLiteral("navList"));
    m_nav->setMaximumWidth(180);
    m_nav->addItem(QStringLiteral("Teklif"));
    m_nav->addItem(QStringLiteral("Arşiv"));
    m_nav->addItem(QStringLiteral("Katalog"));
    m_nav->addItem(QStringLiteral("Müşteriler"));
    m_nav->addItem(QStringLiteral("Ayarlar"));

    m_stack = new QStackedWidget(this);
    m_stack->setObjectName(QStringLiteral("pageStack"));

    m_pageQuote = new PageQuote(db, this);
    m_pageQuote->setObjectName(QStringLiteral("pageQuote"));
    m_pageArchive = new PageArchive(db, this);
    m_pageArchive->setObjectName(QStringLiteral("pageArchive"));
    m_pageCatalog = new PageCatalog(db, this);
    m_pageCatalog->setObjectName(QStringLiteral("pageCatalog"));
    m_pageCustomers = new PageCustomers(db, this);
    m_pageCustomers->setObjectName(QStringLiteral("pageCustomers"));
    m_pageSettings = new PageSettings(db, this);
    m_pageSettings->setObjectName(QStringLiteral("pageSettings"));

    // Sıra mainwindow.h'deki Page numaralandırmasıyla AYNI olmalı.
    m_stack->addWidget(m_pageQuote);
    m_stack->addWidget(m_pageArchive);
    m_stack->addWidget(m_pageCatalog);
    m_stack->addWidget(m_pageCustomers);
    m_stack->addWidget(m_pageSettings);

    connect(m_nav, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0)
            return;
        m_stack->setCurrentIndex(row);
        // Sayfaya her girişte tazelenir: başka bir sayfada kaydedilen teklif
        // ya da eklenen müşteri burada görünmelidir.
        if (row == PageArchiveIndex)
            m_pageArchive->refresh();
        else if (row == PageCatalogIndex)
            m_pageCatalog->refresh();
        else if (row == PageCustomersIndex)
            m_pageCustomers->refresh();
        else if (row == PageSettingsIndex)
            m_pageSettings->refresh();
    });

    // Arşivden ve müşteri kartından gelen "bu teklifi aç" istekleri.
    connect(m_pageArchive, &PageArchive::quoteOpenRequested, this, &MainWindow::openQuote);
    connect(m_pageArchive, &PageArchive::quoteDuplicated, this, &MainWindow::openQuote);
    connect(m_pageCustomers, &PageCustomers::quoteOpenRequested, this, &MainWindow::openQuote);

    // Müşteri eklendiğinde teklif ekranındaki açılır liste bayatlamasın.
    connect(m_pageCustomers, &PageCustomers::customersChanged, this,
            [this] { m_pageQuote->reloadCustomers(); });

    // Katalog değiştiğinde teklif ekranındaki arama indeksi bayatlamasın:
    // yeni eklenen kalem hemen aranabilir olmalı.
    connect(m_pageCatalog, &PageCatalog::catalogChanged, this,
            [this] { m_pageQuote->reloadCatalog(); });

    // Ayarlar kaydedilince antet, KDV oranı ve şartlar metni tazelenir;
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
    firma.adres = settings.valueOr(Settings::keyCompanyAddress());
    firma.telefon = settings.valueOr(Settings::keyCompanyPhone());
    firma.email = settings.valueOr(Settings::keyCompanyEmail());
    firma.vergiDairesi = settings.valueOr(Settings::keyCompanyTaxOffice());
    firma.vergiNo = settings.valueOr(Settings::keyCompanyTaxNo());
    m_pageQuote->setCompanyInfo(firma);
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
