#include "settings.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

Settings::Settings(QSqlDatabase db) : m_db(std::move(db)) {}

QString Settings::keyQuoteCounter()       { return QStringLiteral("teklif_no_sayac"); }
QString Settings::keyCompanyName()        { return QStringLiteral("firma_unvan"); }
QString Settings::keyCompanyLicence()     { return QStringLiteral("firma_yetki_belgesi"); }
QString Settings::keyCompanyAddress()     { return QStringLiteral("firma_adres"); }
QString Settings::keyCompanyPhone()       { return QStringLiteral("firma_telefon"); }
QString Settings::keyCompanyEmail()       { return QStringLiteral("firma_email"); }
QString Settings::keyCompanyTaxOffice()   { return QStringLiteral("firma_vergi_dairesi"); }
QString Settings::keyCompanyTaxNo()       { return QStringLiteral("firma_vergi_no"); }
QString Settings::keyCompanyLogo()        { return QStringLiteral("firma_logo"); }
QString Settings::keyDefaultVatRate()     { return QStringLiteral("varsayilan_kdv_orani"); }
QString Settings::keyQuoteNoDigits()      { return QStringLiteral("teklif_no_hane"); }
QString Settings::keyUiScale()            { return QStringLiteral("arayuz_olcegi"); }
QString Settings::keyDocumentFontPt()     { return QStringLiteral("belge_yazi_boyutu"); }
QString Settings::keyPdfFolder()          { return QStringLiteral("pdf_klasoru"); }
QString Settings::keyTermsText()          { return QStringLiteral("sartlar_metni"); }
QString Settings::keyUpdateSkipVersion()  { return QStringLiteral("guncelleme_atlanan_surum"); }
QString Settings::keyUpdateCheckEnabled() { return QStringLiteral("guncelleme_denetimi"); }

std::optional<QString> Settings::value(const QString &key, QString *errorOut) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT value FROM settings WHERE key = :key"));
    q.bindValue(QStringLiteral(":key"), key);

    if (!q.exec()) {
        if (errorOut)
            *errorOut = q.lastError().text();
        return std::nullopt;
    }
    if (!q.next())
        return std::nullopt; // anahtar yok — hata değil
    return q.value(0).toString();
}

QString Settings::valueOr(const QString &key, const QString &varsayilan) const
{
    return value(key).value_or(varsayilan);
}

qint64 Settings::intValueOr(const QString &key, qint64 varsayilan) const
{
    const auto v = value(key);
    if (!v.has_value())
        return varsayilan;
    bool ok = false;
    const qint64 sayi = v->toLongLong(&ok);
    // Elle bozulmuş bir değer ("abc") programı çökertmemeli; varsayılana düşer.
    return ok ? sayi : varsayilan;
}

bool Settings::boolValueOr(const QString &key, bool varsayilan) const
{
    const auto v = value(key);
    if (!v.has_value())
        return varsayilan;
    return *v == QLatin1String("1") || v->compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
}

bool Settings::setValue(const QString &key, const QString &value, QString *errorOut)
{
    QSqlQuery q(m_db);
    // UPSERT: tek ifadede "yoksa ekle, varsa güncelle". excluded.value,
    // eklenmeye çalışılan satırın değerine karşılık gelir.
    q.prepare(QStringLiteral("INSERT INTO settings (key, value) VALUES (:key, :val) "
                              "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    q.bindValue(QStringLiteral(":key"), key);
    q.bindValue(QStringLiteral(":val"), value);

    if (!q.exec()) {
        if (errorOut)
            *errorOut = q.lastError().text();
        return false;
    }
    return true;
}

bool Settings::setInt(const QString &key, qint64 value, QString *errorOut)
{
    return setValue(key, QString::number(value), errorOut);
}

bool Settings::setBool(const QString &key, bool value, QString *errorOut)
{
    return setValue(key, value ? QStringLiteral("1") : QStringLiteral("0"), errorOut);
}

bool Settings::remove(const QString &key, QString *errorOut)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM settings WHERE key = :key"));
    q.bindValue(QStringLiteral(":key"), key);

    if (!q.exec()) {
        if (errorOut)
            *errorOut = q.lastError().text();
        return false;
    }
    return true;
}
