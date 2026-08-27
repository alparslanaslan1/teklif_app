#include "updater.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTimer>

namespace {

// Ağ isteklerinin üst sınırı. Sunucu yanıt vermezse program açılışta
// takılı kalmasın diye kısa tutulur; güncelleme kaçarsa bir dahaki açılışta
// yeniden denenir.
constexpr int kManifestTimeoutMs = 5000;
constexpr int kDownloadTimeoutMs = 300000; // 5 dk: paket büyük olabilir

// Bir QNetworkReply'a zaman aşımı bağlar. Qt'nin kendi
// transferTimeout'u yalnızca "hiç veri akmıyor" halini yakalar; burada
// isteğin TOPLAM süresini sınırlıyoruz.
void attachTimeout(QNetworkReply *reply, int ms)
{
    QTimer *timer = new QTimer(reply);
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, reply, [reply] {
        if (reply->isRunning())
            reply->abort();
    });
    timer->start(ms);
}

} // namespace

Updater::Updater(const QUrl &manifestUrl, const QString &currentVersion, QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
    , m_manifestUrl(manifestUrl)
    , m_currentVersion(currentVersion)
{
}

Updater::~Updater()
{
    abortPending();
}

// Devam eden isteği iptal eder. Yeni bir kontrol/indirme başlatılmadan önce
// ve nesne yok edilirken çağrılır — yarım kalmış bir yanıtın sinyal yayması
// engellenir.
void Updater::abortPending()
{
    if (m_reply) {
        m_reply->disconnect(this);
        if (m_reply->isRunning())
            m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
}

void Updater::checkForUpdate()
{
    abortPending();

    QNetworkRequest req(m_manifestUrl);
    // Sunucudaki latest.json bir CDN/proxy tarafından önbelleğe alınırsa
    // kullanıcı günlerce eski sürümü görür; her seferinde taze istenir.
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                      QNetworkRequest::AlwaysNetwork);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                   QStringLiteral("TeklifApp/%1").arg(m_currentVersion));

    m_reply = m_net->get(req);
    attachTimeout(m_reply, kManifestTimeoutMs);

    connect(m_reply, &QNetworkReply::finished, this, [this] {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            // İnternet yok, sunucu kapalı, zaman aşımı... Hepsi aynı sonuç:
            // güncelleme atlanır, program normal açılır.
            emit checkFailed(reply->errorString());
            return;
        }

        QString parseErr;
        const auto info = parseUpdateManifest(reply->readAll(), &parseErr);
        if (!info.has_value()) {
            emit checkFailed(parseErr);
            return;
        }

        if (compareVersions(info->version, m_currentVersion) > 0)
            emit updateAvailable(info.value());
        else
            emit upToDate();
    });
}

void Updater::downloadUpdate(const UpdateInfo &info)
{
    abortPending();

    // Paket, kullanıcının önbellek dizinine iner: program klasörüne yazma
    // izni olmayabilir ve indirme yarım kalırsa orada çöp bırakmamak gerekir.
    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheDir.isEmpty() || !QDir().mkpath(cacheDir)) {
        emit downloadFailed(QStringLiteral("Geçici indirme klasörü oluşturulamadı."));
        return;
    }
    const QString hedef = cacheDir + QStringLiteral("/teklif-%1.zip").arg(info.version);

    QNetworkRequest req(info.url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                   QStringLiteral("TeklifApp/%1").arg(m_currentVersion));
    // GitHub Releases indirmeleri farklı bir konuma yönlendirir; izlenmezse
    // dosya yerine yönlendirme gövdesi inerdi.
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                      QNetworkRequest::NoLessSafeRedirectPolicy);

    m_reply = m_net->get(req);
    attachTimeout(m_reply, kDownloadTimeoutMs);

    connect(m_reply, &QNetworkReply::downloadProgress, this, &Updater::downloadProgress);

    connect(m_reply, &QNetworkReply::finished, this, [this, hedef, info] {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit downloadFailed(reply->errorString());
            return;
        }

        QFile f(hedef);
        if (!f.open(QIODevice::WriteOnly)) {
            emit downloadFailed(QStringLiteral("Paket yazılamadı: %1").arg(hedef));
            return;
        }
        f.write(reply->readAll());
        f.close();

        // Doğrulama indirmeden AYRI bir adım: bozuk ya da araya girilmiş bir
        // paketi kurmaktansa güncellemeyi iptal etmek gerekir.
        QString sumErr;
        if (!verifyChecksum(hedef, info.sha256, &sumErr)) {
            QFile::remove(hedef); // bozuk paketi bırakma
            emit downloadFailed(sumErr);
            return;
        }

        emit downloadFinished(hedef);
    });
}

bool Updater::mustUpdate(const UpdateInfo &info, const QString &currentVersion)
{
    if (info.minVersion.isEmpty())
        return false;
    return compareVersions(currentVersion, info.minVersion) < 0;
}

bool Updater::verifyChecksum(const QString &dosyaYolu, const QString &beklenenSha256,
                              QString *errorOut)
{
    // Manifest'te özet verilmemişse doğrulama atlanır. Bu bilinçli bir esneklik
    // ama önerilmez: özet olmadan indirilen paketin doğruluğu garanti edilemez.
    if (beklenenSha256.isEmpty())
        return true;

    QFile f(dosyaYolu);
    if (!f.open(QIODevice::ReadOnly)) {
        if (errorOut)
            *errorOut = QStringLiteral("Paket okunamadı: %1").arg(dosyaYolu);
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    // addData(QIODevice*) dosyayı parça parça okur — büyük bir paket için
    // tamamını belleğe almak gerekmez.
    if (!hash.addData(&f)) {
        if (errorOut)
            *errorOut = QStringLiteral("Paket özeti hesaplanamadı: %1").arg(dosyaYolu);
        return false;
    }

    const QString hesaplanan = QString::fromLatin1(hash.result().toHex());
    if (hesaplanan.compare(beklenenSha256, Qt::CaseInsensitive) != 0) {
        if (errorOut) {
            *errorOut = QStringLiteral("Paket doğrulanamadı. Beklenen %1, bulunan %2.")
                            .arg(beklenenSha256, hesaplanan);
        }
        return false;
    }
    return true;
}
