#include "page_settings.h"

#include "core/db.h"
#include "core/repo_quotes.h"
#include "print/company_logo.h"
#include "theme.h"

#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace {
constexpr int kLogoOnizlemeYuksekligi = 60;
}

PageSettings::PageSettings(QSqlDatabase db, QWidget *parent)
    : QWidget(parent), m_db(db), m_settings(db)
{
    setupUi();
    refresh();
}

void PageSettings::setupUi()
{
    // --- Firma bilgileri ----------------------------------------------------
    m_unvanEdit = new QLineEdit(this);
    m_unvanEdit->setObjectName(QStringLiteral("ayarUnvanEdit"));
    m_yetkiEdit = new QLineEdit(this);
    m_yetkiEdit->setObjectName(QStringLiteral("ayarYetkiEdit"));
    m_yetkiEdit->setPlaceholderText(QStringLiteral("örn. Aksa Doğalgaz Yetkili Firma (No: 328)"));
    m_adresEdit = new QLineEdit(this);
    m_adresEdit->setObjectName(QStringLiteral("ayarAdresEdit"));
    m_telefonEdit = new QLineEdit(this);
    m_telefonEdit->setObjectName(QStringLiteral("ayarTelefonEdit"));
    m_emailEdit = new QLineEdit(this);
    m_emailEdit->setObjectName(QStringLiteral("ayarEmailEdit"));
    m_vergiDairesiEdit = new QLineEdit(this);
    m_vergiDairesiEdit->setObjectName(QStringLiteral("ayarVergiDairesiEdit"));
    m_vergiNoEdit = new QLineEdit(this);
    m_vergiNoEdit->setObjectName(QStringLiteral("ayarVergiNoEdit"));

    m_logoOnizleme = new QLabel(this);
    m_logoOnizleme->setObjectName(QStringLiteral("ayarLogoOnizleme"));
    m_logoOnizleme->setFixedHeight(kLogoOnizlemeYuksekligi);
    m_logoOnizleme->setMinimumWidth(160);
    m_logoOnizleme->setFrameShape(QFrame::StyledPanel);
    m_logoOnizleme->setAlignment(Qt::AlignCenter);

    m_logoSecButton = new QPushButton(QStringLiteral("Logo seç..."), this);
    m_logoSecButton->setObjectName(QStringLiteral("ayarLogoSecButton"));
    connect(m_logoSecButton, &QPushButton::clicked, this, &PageSettings::onLogoSelectClicked);

    m_logoSilButton = new QPushButton(QStringLiteral("Kaldır"), this);
    m_logoSilButton->setObjectName(QStringLiteral("ayarLogoSilButton"));
    connect(m_logoSilButton, &QPushButton::clicked, this, &PageSettings::onLogoClearClicked);

    auto *logoLay = new QHBoxLayout;
    logoLay->addWidget(m_logoOnizleme);
    logoLay->addWidget(m_logoSecButton);
    logoLay->addWidget(m_logoSilButton);
    logoLay->addStretch();

    auto *firmaForm = new QFormLayout;
    firmaForm->addRow(QStringLiteral("Unvan"), m_unvanEdit);
    firmaForm->addRow(QStringLiteral("Yetki belgesi"), m_yetkiEdit);
    firmaForm->addRow(QStringLiteral("Adres"), m_adresEdit);
    firmaForm->addRow(QStringLiteral("Telefon"), m_telefonEdit);
    firmaForm->addRow(QStringLiteral("E-posta"), m_emailEdit);
    firmaForm->addRow(QStringLiteral("Vergi dairesi"), m_vergiDairesiEdit);
    firmaForm->addRow(QStringLiteral("Vergi no"), m_vergiNoEdit);
    firmaForm->addRow(QStringLiteral("Logo"), logoLay);

    auto *firmaKutu = new QGroupBox(QStringLiteral("Firma bilgileri"), this);
    firmaKutu->setLayout(firmaForm);

    // --- Teklif varsayılanları ---------------------------------------------
    m_haneSpin = new QSpinBox(this);
    m_haneSpin->setObjectName(QStringLiteral("ayarHaneSpin"));
    m_haneSpin->setRange(RepoQuotes::kMinQuoteNoDigits, RepoQuotes::kMaxQuoteNoDigits);
    m_haneSpin->setSuffix(QStringLiteral(" hane"));

    m_sartlarEdit = new QPlainTextEdit(this);
    m_sartlarEdit->setObjectName(QStringLiteral("ayarSartlarEdit"));
    m_sartlarEdit->setMaximumHeight(90);
    // Boş bırakılırsa varsayılan metin kullanılır; kullanıcı ne basılacağını
    // görebilsin diye ipucu olarak gösterilir.
    m_sartlarEdit->setPlaceholderText(Settings::varsayilanSartlar());

    auto *teklifForm = new QFormLayout;
    teklifForm->addRow(QStringLiteral("Teklif no hane sayısı"), m_haneSpin);
    teklifForm->addRow(QStringLiteral("Şartlar metni"), m_sartlarEdit);

    auto *teklifKutu = new QGroupBox(QStringLiteral("Teklif varsayılanları"), this);
    teklifKutu->setLayout(teklifForm);

    // --- Görünüm ve çıktı ---------------------------------------------------
    m_belgeYaziSpin = new QSpinBox(this);
    m_belgeYaziSpin->setObjectName(QStringLiteral("ayarBelgeYaziSpin"));
    m_belgeYaziSpin->setRange(8, 12);
    m_belgeYaziSpin->setSuffix(QStringLiteral(" pt"));

    m_olcekSpin = new QSpinBox(this);
    m_olcekSpin->setObjectName(QStringLiteral("ayarOlcekSpin"));
    m_olcekSpin->setRange(Theme::kMinScale, Theme::kMaxScale);
    m_olcekSpin->setSingleStep(Theme::kScaleStep);
    m_olcekSpin->setSuffix(QStringLiteral(" %"));

    m_pdfKlasorEdit = new QLineEdit(this);
    m_pdfKlasorEdit->setObjectName(QStringLiteral("ayarPdfKlasorEdit"));
    auto *pdfSec = new QPushButton(QStringLiteral("Seç..."), this);
    connect(pdfSec, &QPushButton::clicked, this, &PageSettings::onPdfFolderClicked);
    auto *pdfLay = new QHBoxLayout;
    pdfLay->addWidget(m_pdfKlasorEdit, 1);
    pdfLay->addWidget(pdfSec);

    auto *gorunumForm = new QFormLayout;
    // İkisi bilinçli olarak ayrı: belge boyutu yalnızca çıktıyı, arayüz
    // ölçeği yalnızca ekranı etkiler.
    gorunumForm->addRow(QStringLiteral("Belge yazı boyutu"), m_belgeYaziSpin);
    gorunumForm->addRow(QStringLiteral("Arayüz ölçeği"), m_olcekSpin);
    gorunumForm->addRow(QStringLiteral("PDF klasörü"), pdfLay);

    auto *gorunumKutu = new QGroupBox(QStringLiteral("Görünüm ve çıktı"), this);
    gorunumKutu->setLayout(gorunumForm);

    // --- Yedekleme ----------------------------------------------------------
    auto *yedekAl = new QPushButton(QStringLiteral("Yedek al..."), this);
    yedekAl->setObjectName(QStringLiteral("ayarYedekAlButton"));
    connect(yedekAl, &QPushButton::clicked, this, &PageSettings::onBackupClicked);

    auto *geriYukle = new QPushButton(QStringLiteral("Yedekten geri yükle..."), this);
    geriYukle->setObjectName(QStringLiteral("ayarGeriYukleButton"));
    connect(geriYukle, &QPushButton::clicked, this, &PageSettings::onRestoreClicked);

    auto *yedekLay = new QHBoxLayout;
    yedekLay->addWidget(yedekAl);
    yedekLay->addWidget(geriYukle);
    yedekLay->addStretch();

    auto *yedekKutu = new QGroupBox(QStringLiteral("Yedekleme"), this);
    yedekKutu->setLayout(yedekLay);

    // --- Kaydet -------------------------------------------------------------
    auto *kaydet = new QPushButton(QStringLiteral("Kaydet"), this);
    kaydet->setObjectName(QStringLiteral("ayarKaydetButton"));
    connect(kaydet, &QPushButton::clicked, this, &PageSettings::onSaveClicked);

    auto *kaydetLay = new QHBoxLayout;
    kaydetLay->addStretch();
    kaydetLay->addWidget(kaydet);

    auto *icerik = new QWidget;
    auto *icLay = new QVBoxLayout(icerik);
    icLay->addWidget(firmaKutu);
    icLay->addWidget(teklifKutu);
    icLay->addWidget(gorunumKutu);
    icLay->addWidget(yedekKutu);
    icLay->addLayout(kaydetLay);
    icLay->addStretch();

    // Kaydırılabilir: arayüz ölçeği %150'ye çıkarıldığında içerik pencereye
    // sığmayabilir.
    auto *kaydirma = new QScrollArea(this);
    kaydirma->setWidgetResizable(true);
    kaydirma->setWidget(icerik);

    auto *ana = new QVBoxLayout(this);
    ana->addWidget(kaydirma);
}

void PageSettings::refresh()
{
    m_unvanEdit->setText(m_settings.valueOr(Settings::keyCompanyName()));
    m_yetkiEdit->setText(m_settings.valueOr(Settings::keyCompanyLicence()));
    m_adresEdit->setText(m_settings.valueOr(Settings::keyCompanyAddress()));
    m_telefonEdit->setText(m_settings.valueOr(Settings::keyCompanyPhone()));
    m_emailEdit->setText(m_settings.valueOr(Settings::keyCompanyEmail()));
    m_vergiDairesiEdit->setText(m_settings.valueOr(Settings::keyCompanyTaxOffice()));
    m_vergiNoEdit->setText(m_settings.valueOr(Settings::keyCompanyTaxNo()));

    m_haneSpin->setValue(static_cast<int>(m_settings.intValueOr(
        Settings::keyQuoteNoDigits(), RepoQuotes::kDefaultQuoteNoDigits)));
    m_belgeYaziSpin->setValue(
        static_cast<int>(m_settings.intValueOr(Settings::keyDocumentFontPt(), 10)));
    m_olcekSpin->setValue(
        static_cast<int>(m_settings.intValueOr(Settings::keyUiScale(), Theme::kDefaultScale)));
    m_pdfKlasorEdit->setText(m_settings.valueOr(Settings::keyPdfFolder()));
    m_sartlarEdit->setPlainText(m_settings.valueOr(Settings::keyTermsText()));


    logoOnizlemeyiTazele();
}

void PageSettings::logoOnizlemeyiTazele()
{
    const QImage logo = CompanyLogo::load(m_settings);
    if (logo.isNull()) {
        m_logoOnizleme->setPixmap(QPixmap());
        m_logoOnizleme->setText(QStringLiteral("(logo yok)"));
        m_logoSilButton->setEnabled(false);
        return;
    }
    m_logoOnizleme->setText(QString());
    m_logoOnizleme->setPixmap(QPixmap::fromImage(
        logo.scaledToHeight(kLogoOnizlemeYuksekligi - 4, Qt::SmoothTransformation)));
    m_logoSilButton->setEnabled(true);
}

bool PageSettings::save(QString *errorOut)
{
    struct Alan { QString anahtar; QString deger; };
    const QVector<Alan> alanlar = {
        {Settings::keyCompanyName(), m_unvanEdit->text().trimmed()},
        {Settings::keyCompanyLicence(), m_yetkiEdit->text().trimmed()},
        {Settings::keyCompanyAddress(), m_adresEdit->text().trimmed()},
        {Settings::keyCompanyPhone(), m_telefonEdit->text().trimmed()},
        {Settings::keyCompanyEmail(), m_emailEdit->text().trimmed()},
        {Settings::keyCompanyTaxOffice(), m_vergiDairesiEdit->text().trimmed()},
        {Settings::keyCompanyTaxNo(), m_vergiNoEdit->text().trimmed()},
        {Settings::keyPdfFolder(), m_pdfKlasorEdit->text().trimmed()},
        {Settings::keyTermsText(), m_sartlarEdit->toPlainText()},
    };
    for (const Alan &a : alanlar) {
        if (!m_settings.setValue(a.anahtar, a.deger, errorOut))
            return false;
    }

    if (!m_settings.setInt(Settings::keyQuoteNoDigits(), m_haneSpin->value(), errorOut))
        return false;
    if (!m_settings.setInt(Settings::keyDocumentFontPt(), m_belgeYaziSpin->value(), errorOut))
        return false;

    // Ölçek hem uygulanır hem yazılır; kullanıcı sonucu anında görür.
    if (!Theme::setAndStore(m_settings, m_olcekSpin->value(), errorOut))
        return false;

    emit companyInfoChanged();
    emit uiScaleChanged(m_olcekSpin->value());
    return true;
}

void PageSettings::onSaveClicked()
{
    QString err;
    if (!save(&err)) {
        QMessageBox::warning(this, QStringLiteral("Kaydedilemedi"), err);
        return;
    }
    QMessageBox::information(this, QStringLiteral("Kaydedildi"),
                              QStringLiteral("Ayarlar kaydedildi."));
}

void PageSettings::onLogoSelectClicked()
{
    const QString yol = QFileDialog::getOpenFileName(
        this, QStringLiteral("Logo seç"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        QStringLiteral("Görsel dosyaları (*.png *.jpg *.jpeg *.bmp);;Tüm dosyalar (*)"));
    if (yol.isEmpty())
        return;

    QImageReader okuyucu(yol);
    okuyucu.setAutoTransform(true); // fotoğraf EXIF dönüşü varsa uygula
    const QImage img = okuyucu.read();
    if (img.isNull()) {
        QMessageBox::warning(this, QStringLiteral("Okunamadı"),
                              QStringLiteral("Görsel açılamadı: %1").arg(okuyucu.errorString()));
        return;
    }

    QString err;
    // Ölçekleme ve PNG'ye kodlama CompanyLogo::save içinde; büyük bir
    // fotoğraf seçilse bile veritabanı şişmez.
    if (!CompanyLogo::save(m_settings, img, &err)) {
        QMessageBox::warning(this, QStringLiteral("Kaydedilemedi"), err);
        return;
    }

    logoOnizlemeyiTazele();
    emit companyInfoChanged();
}

void PageSettings::onLogoClearClicked()
{
    QString err;
    if (!CompanyLogo::clear(m_settings, &err)) {
        QMessageBox::warning(this, QStringLiteral("Kaldırılamadı"), err);
        return;
    }
    logoOnizlemeyiTazele();
    emit companyInfoChanged();
}

void PageSettings::onPdfFolderClicked()
{
    const QString mevcut = m_pdfKlasorEdit->text().isEmpty()
                                ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                                : m_pdfKlasorEdit->text();
    const QString klasor =
        QFileDialog::getExistingDirectory(this, QStringLiteral("PDF klasörü"), mevcut);
    if (!klasor.isEmpty())
        m_pdfKlasorEdit->setText(klasor);
}

void PageSettings::onBackupClicked()
{
    const QString onerilen =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + QStringLiteral("/teklif-yedek-")
        + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))
        + QStringLiteral(".db");

    const QString yol = QFileDialog::getSaveFileName(this, QStringLiteral("Yedek al"), onerilen,
                                                      QStringLiteral("Veritabanı (*.db)"));
    if (yol.isEmpty())
        return;

    // VACUUM INTO: bağlantıyı kapatmadan TUTARLI bir kopya üretir. Düz dosya
    // kopyası, o an açık bir işlem varsa yarım bir dosya verebilirdi.
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("VACUUM INTO :yol"));
    q.bindValue(QStringLiteral(":yol"), yol);

    if (!q.exec()) {
        QMessageBox::warning(this, QStringLiteral("Yedeklenemedi"),
                              QStringLiteral("%1\n\nHedef dosya zaten varsa silin; VACUUM INTO "
                                              "var olan bir dosyanın üzerine yazmaz.")
                                  .arg(q.lastError().text()));
        return;
    }

    QMessageBox::information(this, QStringLiteral("Yedek alındı"), yol);
}

void PageSettings::onRestoreClicked()
{
    const QString yol = QFileDialog::getOpenFileName(
        this, QStringLiteral("Yedekten geri yükle"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        QStringLiteral("Veritabanı (*.db)"));
    if (yol.isEmpty())
        return;

    const QString hedef = Db::defaultPath();
    if (QFileInfo(yol).canonicalFilePath() == QFileInfo(hedef).canonicalFilePath()) {
        QMessageBox::warning(this, QStringLiteral("Geri yüklenemedi"),
                              QStringLiteral("Seçilen dosya zaten kullanılan veritabanı."));
        return;
    }

    if (QMessageBox::question(
            this, QStringLiteral("Geri yükle"),
            QStringLiteral("Mevcut veritabanının YERİNE bu yedek konacak ve program "
                            "kapanacak.\n\nDevam edilsin mi?"))
        != QMessageBox::Yes) {
        return;
    }

    // Geri yükleme, çalışan programın altından veritabanını değiştirmek
    // demektir; açık bağlantılar bozulur. Bu yüzden dosya değiştirilir ve
    // program kapatılır — kullanıcı yeniden açtığında openAndMigrate yedeği
    // açar, gerekiyorsa göçürür ve öncesinde .bak kopyasını alır.
    const QString eskisi = hedef + QStringLiteral(".geri-yukleme-oncesi");
    QFile::remove(eskisi);
    if (QFile::exists(hedef) && !QFile::rename(hedef, eskisi)) {
        QMessageBox::warning(this, QStringLiteral("Geri yüklenemedi"),
                              QStringLiteral("Mevcut veritabanı yeniden adlandırılamadı."));
        return;
    }

    if (!QFile::copy(yol, hedef)) {
        QFile::rename(eskisi, hedef); // eskisini geri koy
        QMessageBox::warning(this, QStringLiteral("Geri yüklenemedi"),
                              QStringLiteral("Yedek kopyalanamadı."));
        return;
    }

    QMessageBox::information(
        this, QStringLiteral("Geri yüklendi"),
        QStringLiteral("Yedek geri yüklendi. Program şimdi kapanacak; değişikliklerin "
                        "geçerli olması için yeniden açın."));
    QApplication::quit();
}
