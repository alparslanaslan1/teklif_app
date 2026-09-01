#include "teklif/core/quote_status.h"

namespace QuoteStatus {

QString taslak()      { return QStringLiteral("Taslak"); }
QString gonderildi()  { return QStringLiteral("Gönderildi"); }
QString onaylandi()   { return QStringLiteral("Onaylandı"); }
QString reddedildi()  { return QStringLiteral("Reddedildi"); }

QStringList all()
{
    // Sıra iş akışını yansıtır: taslak -> gönderildi -> onaylandı/reddedildi.
    return {taslak(), gonderildi(), onaylandi(), reddedildi()};
}

bool isValid(const QString &durum)
{
    return all().contains(durum);
}

} // namespace QuoteStatus
