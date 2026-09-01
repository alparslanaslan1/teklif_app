#include "teklif/ui/update_prompt.h"

#include "teklif/update/updater.h"

#include <QCheckBox>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QWidget>

UpdatePrompt::UpdatePrompt(QSqlDatabase db, const QUrl &manifestUrl,
                            const QString &currentVersion, QWidget *parent)
    : QObject(parent)
    , m_db(db)
    , m_settings(db)
    , m_updater(new Updater(manifestUrl, currentVersion, this))
    , m_parent(parent)
    , m_currentVersion(currentVersion)
{
    connect(m_updater, &Updater::updateAvailable, this, &UpdatePrompt::onUpdateAvailable);

    connect(m_updater, &Updater::upToDate, this, [this] {
        if (m_sessizDegil) {
            QMessageBox::information(m_parent, QStringLiteral("Güncelleme"),
                                      QStringLiteral("Programınız güncel (sürüm %1).")
                                          .arg(m_currentVersion));
        }
    });

    connect(m_updater, &Updater::checkFailed, this, [this](const QString &hata) {
        // Açılışta sessiz: internet yoksa kullanıcının yapabileceği bir şey
        // yok, her açılışta hata penceresi göstermek yalnızca rahatsız eder.
        if (m_sessizDegil) {
            QMessageBox::warning(m_parent, QStringLiteral("Güncelleme denetlenemedi"), hata);
        }
    });
}

void UpdatePrompt::checkOnStartup()
{
    if (!Updater::isAutoCheckEnabled(m_settings))
        return;
    m_sessizDegil = false;
    m_updater->checkForUpdate();
}

void UpdatePrompt::checkNow()
{
    m_sessizDegil = true;
    m_updater->checkForUpdate();
}

void UpdatePrompt::onUpdateAvailable(const UpdateInfo &info)
{
    const bool zorunlu = Updater::mustUpdate(info, m_currentVersion);

    // Kullanıcı bu sürümü atlamışsa ve güncelleme zorunlu DEĞİLSE sorma.
    // Elle denetimde ise her zaman gösterilir: kullanıcı bilerek sordu.
    if (!zorunlu && !m_sessizDegil && Updater::isVersionSkipped(m_settings, info.version))
        return;

    QMessageBox kutu(m_parent);
    kutu.setWindowTitle(QStringLiteral("Güncelleme var"));
    kutu.setIcon(QMessageBox::Information);
    kutu.setText(QStringLiteral("Yeni sürüm: %1 (şu an %2)").arg(info.version, m_currentVersion));
    if (!info.notes.isEmpty())
        kutu.setInformativeText(info.notes);

    QPushButton *kur = kutu.addButton(QStringLiteral("Şimdi güncelle"), QMessageBox::AcceptRole);
    QPushButton *sonra = nullptr;
    QPushButton *atla = nullptr;
    QCheckBox *denetimKapat = nullptr;

    if (zorunlu) {
        // Zorunlu güncellemede "sonra" ya da "atla" YOK: bu genelde veri
        // şeması değiştiği ve eski programın yeni .db dosyasını açamayacağı
        // anlamına gelir (bkz. Db::migrateStep).
        kutu.setInformativeText(
            kutu.informativeText()
            + QStringLiteral("\n\nBu güncelleme zorunludur; programı kullanmaya devam etmek "
                              "için kurulması gerekir."));
    } else {
        sonra = kutu.addButton(QStringLiteral("Daha sonra"), QMessageBox::RejectRole);
        atla = kutu.addButton(QStringLiteral("Bu sürümü atla"), QMessageBox::DestructiveRole);

        denetimKapat = new QCheckBox(QStringLiteral("Açılışta güncelleme denetleme"), &kutu);
        denetimKapat->setObjectName(QStringLiteral("guncellemeDenetimKapat"));
        kutu.setCheckBox(denetimKapat);
    }

    kutu.exec();

    if (denetimKapat && denetimKapat->isChecked())
        Updater::setAutoCheckEnabled(m_settings, false);

    if (kutu.clickedButton() == atla) {
        Updater::skipVersion(m_settings, info.version);
        return;
    }
    if (kutu.clickedButton() == sonra)
        return;
    if (kutu.clickedButton() != kur)
        return; // pencere kapatıldı

    startDownload(info);
}

void UpdatePrompt::startDownload(const UpdateInfo &info)
{
    auto *ilerleme = new QProgressDialog(QStringLiteral("Güncelleme indiriliyor..."),
                                          QStringLiteral("İptal"), 0, 100, m_parent);
    ilerleme->setObjectName(QStringLiteral("guncellemeIlerleme"));
    ilerleme->setWindowModality(Qt::WindowModal);
    ilerleme->setAutoClose(false);
    ilerleme->setAutoReset(false);
    ilerleme->show();

    connect(m_updater, &Updater::downloadProgress, ilerleme,
            [ilerleme](qint64 alinan, qint64 toplam) {
                // Sunucu içerik uzunluğu bildirmezse toplam -1 gelir; o
                // durumda belirsiz (dönen) çubuk gösterilir.
                if (toplam <= 0) {
                    ilerleme->setRange(0, 0);
                    return;
                }
                ilerleme->setRange(0, 100);
                ilerleme->setValue(static_cast<int>(alinan * 100 / toplam));
            });

    connect(m_updater, &Updater::downloadFailed, ilerleme, [this, ilerleme](const QString &hata) {
        ilerleme->close();
        ilerleme->deleteLater();
        // İndirme hatası kullanıcıya HER ZAMAN bildirilir: bunu kendisi
        // başlattı, sessizce vazgeçilmesi kafa karıştırırdı.
        QMessageBox::warning(m_parent, QStringLiteral("Güncelleme indirilemedi"), hata);
    });

    connect(m_updater, &Updater::downloadFinished, ilerleme,
            [this, ilerleme](const QString &paketYolu) {
                ilerleme->close();
                ilerleme->deleteLater();

                QString hata;
                if (!Updater::launchInstaller(paketYolu, &hata)) {
                    QMessageBox::warning(m_parent, QStringLiteral("Kurulum başlatılamadı"), hata);
                    return;
                }

                QMessageBox::information(
                    m_parent, QStringLiteral("Güncelleme"),
                    QStringLiteral("Kurulum başlatıldı. Program şimdi kapanacak; kurulum "
                                    "bitince yeniden açabilirsiniz."));
                emit quitRequested();
            });

    m_updater->downloadUpdate(info);
}
