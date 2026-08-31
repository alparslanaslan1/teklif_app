#include "mainwindow.h"

#include "page_archive.h"
#include "page_customers.h"
#include "page_quote.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QMessageBox>
#include <QStackedWidget>
#include <QWidget>

MainWindow::MainWindow(QSqlDatabase db, QWidget *parent) : QMainWindow(parent)
{
    setupUi(db);
}

void MainWindow::setupUi(QSqlDatabase db)
{
    m_nav = new QListWidget(this);
    m_nav->setObjectName(QStringLiteral("navList"));
    m_nav->setMaximumWidth(180);
    m_nav->addItem(QStringLiteral("Teklif"));
    m_nav->addItem(QStringLiteral("Arşiv"));
    m_nav->addItem(QStringLiteral("Müşteriler"));

    m_stack = new QStackedWidget(this);
    m_stack->setObjectName(QStringLiteral("pageStack"));

    m_pageQuote = new PageQuote(db, this);
    m_pageQuote->setObjectName(QStringLiteral("pageQuote"));
    m_pageArchive = new PageArchive(db, this);
    m_pageArchive->setObjectName(QStringLiteral("pageArchive"));
    m_pageCustomers = new PageCustomers(db, this);
    m_pageCustomers->setObjectName(QStringLiteral("pageCustomers"));

    // Sıra mainwindow.h'deki Page numaralandırmasıyla AYNI olmalı.
    m_stack->addWidget(m_pageQuote);
    m_stack->addWidget(m_pageArchive);
    m_stack->addWidget(m_pageCustomers);

    connect(m_nav, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0)
            return;
        m_stack->setCurrentIndex(row);
        // Sayfaya her girişte tazelenir: başka bir sayfada kaydedilen teklif
        // ya da eklenen müşteri burada görünmelidir.
        if (row == PageArchiveIndex)
            m_pageArchive->refresh();
        else if (row == PageCustomersIndex)
            m_pageCustomers->refresh();
    });

    // Arşivden ve müşteri kartından gelen "bu teklifi aç" istekleri.
    connect(m_pageArchive, &PageArchive::quoteOpenRequested, this, &MainWindow::openQuote);
    connect(m_pageArchive, &PageArchive::quoteDuplicated, this, &MainWindow::openQuote);
    connect(m_pageCustomers, &PageCustomers::quoteOpenRequested, this, &MainWindow::openQuote);

    // Müşteri eklendiğinde teklif ekranındaki açılır liste bayatlamasın.
    connect(m_pageCustomers, &PageCustomers::customersChanged, this,
            [this] { m_pageQuote->reloadCustomers(); });

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
