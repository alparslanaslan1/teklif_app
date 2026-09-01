#include "teklif/print/company_logo.h"

#include <QBuffer>
#include <QByteArray>

namespace CompanyLogo {

QImage load(const Settings &settings)
{
    const QString base64 = settings.valueOr(Settings::keyCompanyLogo());
    if (base64.isEmpty())
        return {};

    const QByteArray ham = QByteArray::fromBase64(base64.toLatin1());
    if (ham.isEmpty())
        return {};

    QImage img;
    // Yükleme başarısızsa (veri bozulmuş, elle düzenlenmiş) null döner ve
    // belge logosuz basılır; program çökmez.
    if (!img.loadFromData(ham, "PNG"))
        return {};
    return img;
}

bool save(Settings &settings, const QImage &logo, QString *errorOut)
{
    if (logo.isNull()) {
        if (errorOut)
            *errorOut = QStringLiteral("Geçersiz görsel.");
        return false;
    }

    // Büyük tarafı kMaxBoyut'a indir. Zaten küçükse BÜYÜTME — küçük bir
    // logoyu büyütmek onu bulanıklaştırırdı.
    QImage olcekli = logo;
    if (olcekli.width() > kMaxBoyut || olcekli.height() > kMaxBoyut) {
        olcekli = olcekli.scaled(kMaxBoyut, kMaxBoyut, Qt::KeepAspectRatio,
                                  Qt::SmoothTransformation);
    }

    QByteArray ham;
    QBuffer tampon(&ham);
    tampon.open(QIODevice::WriteOnly);
    if (!olcekli.save(&tampon, "PNG")) {
        if (errorOut)
            *errorOut = QStringLiteral("Görsel PNG olarak kodlanamadı.");
        return false;
    }

    return settings.setValue(Settings::keyCompanyLogo(),
                              QString::fromLatin1(ham.toBase64()), errorOut);
}

bool clear(Settings &settings, QString *errorOut)
{
    return settings.remove(Settings::keyCompanyLogo(), errorOut);
}

} // namespace CompanyLogo
