#include "teklif/update/update_info.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

namespace {

// "0.2.0" -> [0, 2, 0]. Sayıya çevrilemeyen parça 0 sayılır.
QList<int> versionParts(const QString &version)
{
    QList<int> parts;
    const QStringList parcalar = version.trimmed().split(QLatin1Char('.'));
    for (const QString &p : parcalar) {
        bool ok = false;
        const int deger = p.toInt(&ok);
        parts.append(ok ? deger : 0);
    }
    return parts;
}

} // namespace

int compareVersions(const QString &a, const QString &b)
{
    const QList<int> pa = versionParts(a);
    const QList<int> pb = versionParts(b);

    // Uzun olana göre dönülür; eksik parçalar 0 sayılır ("1.2" == "1.2.0").
    const int n = qMax(pa.size(), pb.size());
    for (int i = 0; i < n; ++i) {
        const int x = i < pa.size() ? pa.at(i) : 0;
        const int y = i < pb.size() ? pb.at(i) : 0;
        if (x != y)
            return x < y ? -1 : 1;
    }
    return 0;
}

std::optional<UpdateInfo> parseUpdateManifest(const QByteArray &json, QString *errorOut)
{
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseErr);
    if (doc.isNull() || !doc.isObject()) {
        if (errorOut)
            *errorOut = QStringLiteral("Güncelleme bilgisi okunamadı: %1").arg(parseErr.errorString());
        return std::nullopt;
    }

    const QJsonObject o = doc.object();

    UpdateInfo info;
    info.version = o.value(QStringLiteral("version")).toString().trimmed();
    info.minVersion = o.value(QStringLiteral("minVersion")).toString().trimmed();
    info.url = QUrl(o.value(QStringLiteral("url")).toString().trimmed());
    info.sha256 = o.value(QStringLiteral("sha256")).toString().trimmed().toLower();
    info.notes = o.value(QStringLiteral("notes")).toString();

    if (info.version.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Güncelleme bilgisinde sürüm alanı yok.");
        return std::nullopt;
    }
    if (!info.url.isValid() || info.url.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Güncelleme bilgisinde geçerli bir indirme adresi yok.");
        return std::nullopt;
    }
    // Yalnızca https kabul edilir: paket imzalanmadığı için taşıma katmanının
    // güvenliği tek koruma. http olsaydı araya girip sahte paket verilebilirdi.
    if (info.url.scheme() != QLatin1String("https")) {
        if (errorOut)
            *errorOut = QStringLiteral("Güncelleme adresi https olmalı: %1").arg(info.url.toString());
        return std::nullopt;
    }

    return info;
}
