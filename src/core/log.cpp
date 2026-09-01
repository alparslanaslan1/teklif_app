#include "log.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QStandardPaths>
#include <QTextStream>

#include <cstdio>

Q_LOGGING_CATEGORY(logDb, "teklif.db")
Q_LOGGING_CATEGORY(logUpdate, "teklif.update")
Q_LOGGING_CATEGORY(logApp, "teklif.app")

namespace Log {
namespace {

// Günlüğe birden fazla iş parçacığından yazılabilir (ağ yanıtları kendi
// iş parçacığında olabilir); satırların birbirine karışmaması için kilit.
QMutex g_kilit;
QtMessageHandler g_oncekiHandler = nullptr;

QString seviyeAdi(QtMsgType tur)
{
    switch (tur) {
    case QtDebugMsg:    return QStringLiteral("DEBUG");
    case QtInfoMsg:     return QStringLiteral("BILGI");
    case QtWarningMsg:  return QStringLiteral("UYARI");
    case QtCriticalMsg: return QStringLiteral("KRITIK");
    case QtFatalMsg:    return QStringLiteral("OLUMCUL");
    }
    return QStringLiteral("?");
}

// Dosya sınırı aşmışsa .1'e taşır. Tek bir eski kopya yeterli: günlük
// hata ayıklama içindir, arşiv değildir.
void gerekiyorsaDondur(const QString &yol)
{
    if (QFileInfo(yol).size() < kMaksBoyut)
        return;
    const QString eski = yol + QStringLiteral(".1");
    QFile::remove(eski);
    QFile::rename(yol, eski);
}

void handler(QtMsgType tur, const QMessageLogContext &baglam, const QString &mesaj)
{
    // Konsola yazmayı sürdür: geliştirme sırasında hâlâ en pratik yol.
    if (g_oncekiHandler)
        g_oncekiHandler(tur, baglam, mesaj);

    const QString yol = filePath();
    if (yol.isEmpty())
        return;

    QMutexLocker kilit(&g_kilit);
    gerekiyorsaDondur(yol);

    QFile f(yol);
    // Günlüğe yazamamak programı DURDURMAMALI: disk doluysa ya da izin
    // yoksa sessizce vazgeçilir.
    if (!f.open(QIODevice::Append | QIODevice::Text))
        return;

    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << QDateTime::currentDateTime().toString(Qt::ISODate) << ' '
        << seviyeAdi(tur) << ' '
        << (baglam.category ? baglam.category : "default") << ": "
        << mesaj << '\n';
}

} // namespace

QString filePath()
{
    const QString dizin = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dizin.isEmpty())
        return {};
    if (!QDir().mkpath(dizin))
        return {};
    return dizin + QStringLiteral("/teklif.log");
}

void install()
{
    g_oncekiHandler = qInstallMessageHandler(handler);
}

} // namespace Log
