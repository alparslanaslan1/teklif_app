#include "teklif/ui/dlg_first_run.h"

#include "teklif/core/repo_items.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QVBoxLayout>

namespace {

// Sihirbazın gösterildiğini işaretleyen ayar. Firma unvanının dolu olup
// olmamasına BAKMAK yetmez: kullanıcı unvanı boş bırakıp geçmiş olabilir ve
// her açılışta aynı pencereyle karşılaşmamalı.
QString keyFirstRunDone()
{
    return QStringLiteral("ilk_calistirma_tamam");
}

// Örnek katalog: doğalgaz iç tesisat işlerinden tipik kalemler. Amaç eksiksiz
// bir fiyat listesi vermek değil, kullanıcının arama kutusunun ve teklif
// akışının nasıl çalıştığını boş ekranla uğraşmadan görmesi. Fiyatlar
// yuvarlak ve BAŞLANGIÇ değeridir; kullanıcı kendi fiyatlarını girecektir.
struct OrnekKalem
{
    const char *kod;
    const char *ad;
    const char *birim;
    qint64 fiyatKurus;
    const char *kategori;
};

const OrnekKalem kOrnekler[] = {
    {"KMB-01", "Kombi Montajı (24 kW)",        "adet", 450000, "Cihaz"},
    {"KMB-02", "Kombi Montajı (30 kW)",        "adet", 520000, "Cihaz"},
    {"BRU-01", "Bakır Boru Tesisatı 15 mm",    "m",     18000, "Tesisat"},
    {"BRU-02", "Bakır Boru Tesisatı 22 mm",    "m",     26000, "Tesisat"},
    {"VNA-01", "Küresel Vana",                 "adet",  12000, "Tesisat"},
    {"BCA-01", "Baca Bağlantı Seti",           "adet",  85000, "Tesisat"},
    {"DDK-01", "Dedektör (Gaz Kaçak)",         "adet", 110000, "Cihaz"},
    {"TST-01", "Gaz Kaçak ve Sızdırmazlık Testi", "adet", 90000, "Hizmet"},
    {"PRJ-01", "Proje ve Çizim",               "adet", 150000, "Hizmet"},
    {"ISC-01", "İşçilik",                      "saat",  35000, "Hizmet"},
};

} // namespace

FirstRunDialog::FirstRunDialog(QSqlDatabase db, QWidget *parent)
    : QDialog(parent), m_db(db), m_settings(db)
{
    setupUi();
}

bool FirstRunDialog::shouldShow(const Settings &settings)
{
    return !settings.boolValueOr(keyFirstRunDone(), false);
}

bool FirstRunDialog::markShown(Settings &settings, QString *errorOut)
{
    return settings.setBool(keyFirstRunDone(), true, errorOut);
}

bool FirstRunDialog::loadSampleCatalog(QSqlDatabase db, QString *errorOut)
{
    RepoItems repo(db);

    // Katalogda kalem varsa dokunma: sihirbaz yeniden çalıştırılsa da
    // (ya da kullanıcı kendi kalemlerini önce girdiyse) ikizleme olmasın.
    QString listErr;
    if (!repo.listAll(/*includeInactive=*/true, &listErr).isEmpty())
        return true;
    if (!listErr.isEmpty()) {
        if (errorOut)
            *errorOut = listErr;
        return false;
    }

    for (const OrnekKalem &o : kOrnekler) {
        Item it;
        it.kod = QString::fromUtf8(o.kod);
        it.ad = QString::fromUtf8(o.ad);
        it.birim = QString::fromUtf8(o.birim);
        it.varsayilanFiyat = Money(o.fiyatKurus);
        it.categoryId = repo.ensureCategory(QString::fromUtf8(o.kategori), errorOut);
        if (it.categoryId < 0)
            return false;

        if (!repo.add(it, errorOut))
            return false;
    }
    return true;
}

void FirstRunDialog::setupUi()
{
    setWindowTitle(QStringLiteral("Hoş geldiniz"));
    setModal(true);

    auto *aciklama = new QLabel(
        QStringLiteral("Programı ilk kez açıyorsunuz. Firma bilgileriniz tekliflerin "
                        "üstündeki antette görünür.\n\nHepsini şimdi doldurmak zorunda "
                        "değilsiniz; sonradan Ayarlar ekranından değiştirebilirsiniz."),
        this);
    aciklama->setWordWrap(true);

    m_unvanEdit = new QLineEdit(this);
    m_unvanEdit->setObjectName(QStringLiteral("ilkUnvanEdit"));
    m_adresEdit = new QLineEdit(this);
    m_adresEdit->setObjectName(QStringLiteral("ilkAdresEdit"));
    m_telefonEdit = new QLineEdit(this);
    m_telefonEdit->setObjectName(QStringLiteral("ilkTelefonEdit"));
    m_emailEdit = new QLineEdit(this);
    m_vergiDairesiEdit = new QLineEdit(this);
    m_vergiNoEdit = new QLineEdit(this);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Unvan"), m_unvanEdit);
    form->addRow(QStringLiteral("Adres"), m_adresEdit);
    form->addRow(QStringLiteral("Telefon"), m_telefonEdit);
    form->addRow(QStringLiteral("E-posta"), m_emailEdit);
    form->addRow(QStringLiteral("Vergi dairesi"), m_vergiDairesiEdit);
    form->addRow(QStringLiteral("Vergi no"), m_vergiNoEdit);

    m_ornekKatalogCheck = new QCheckBox(
        QStringLiteral("Örnek katalog kalemlerini ekle (sonradan düzenleyebilirsiniz)"), this);
    m_ornekKatalogCheck->setObjectName(QStringLiteral("ilkOrnekKatalogCheck"));
    m_ornekKatalogCheck->setChecked(true);

    auto *butonlar = new QDialogButtonBox(this);
    butonlar->setObjectName(QStringLiteral("ilkButtonBox"));
    butonlar->addButton(QStringLiteral("Başla"), QDialogButtonBox::AcceptRole);
    butonlar->addButton(QStringLiteral("Şimdilik atla"), QDialogButtonBox::RejectRole);
    connect(butonlar, &QDialogButtonBox::accepted, this, &FirstRunDialog::onAccept);
    // Atlansa bile sihirbaz bir daha açılmaz; kullanıcı her açılışta aynı
    // pencereyle karşılaşmamalı.
    connect(butonlar, &QDialogButtonBox::rejected, this, [this] {
        markShown(m_settings);
        reject();
    });

    auto *lay = new QVBoxLayout(this);
    lay->addWidget(aciklama);
    lay->addLayout(form);
    lay->addWidget(m_ornekKatalogCheck);
    lay->addWidget(butonlar);

    m_unvanEdit->setFocus();
}

void FirstRunDialog::onAccept()
{
    QString err;

    struct Alan { QString anahtar; QString deger; };
    const QVector<Alan> alanlar = {
        {Settings::keyCompanyName(), m_unvanEdit->text().trimmed()},
        {Settings::keyCompanyAddress(), m_adresEdit->text().trimmed()},
        {Settings::keyCompanyPhone(), m_telefonEdit->text().trimmed()},
        {Settings::keyCompanyEmail(), m_emailEdit->text().trimmed()},
        {Settings::keyCompanyTaxOffice(), m_vergiDairesiEdit->text().trimmed()},
        {Settings::keyCompanyTaxNo(), m_vergiNoEdit->text().trimmed()},
    };
    for (const Alan &a : alanlar) {
        if (!m_settings.setValue(a.anahtar, a.deger, &err)) {
            QMessageBox::warning(this, QStringLiteral("Kaydedilemedi"), err);
            return;
        }
    }

    // Şartlar metni ilk kurulumda hazır gelsin: KDV dahil olduğu bilgisi
    // belgede mutlaka bulunmalı ve kullanıcının bunu bilip yazması beklenemez.
    if (m_settings.valueOr(Settings::keyTermsText()).isEmpty())
        m_settings.setValue(Settings::keyTermsText(), Settings::varsayilanSartlar(), &err);

    if (m_ornekKatalogCheck->isChecked() && !loadSampleCatalog(m_db, &err)) {
        // Örnek katalog eklenemese bile firma bilgisi kaydedildi; kullanıcıyı
        // baştan başlatmaya gerek yok, sadece bilgilendirilir.
        QMessageBox::warning(this, QStringLiteral("Örnek katalog eklenemedi"), err);
    }

    markShown(m_settings, &err);
    accept();
}
