#include "teklif/ui/theme.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QStringList>

namespace Theme {
namespace {

// --------------------------------------------------------------------------
// Renk paleti. Tek yerde durur; bir rengi değiştirmek programın her yerini
// birden değiştirir.
//
// Seçim gerekçesi: teklif ekranı gün boyu açık kalıyor, bu yüzden zemin saf
// beyaz değil hafif gri — beyaz kartlar üzerinde durunca kutular kendiliğinden
// ayrışıyor ve her kutuya kalın çerçeve çizmek gerekmiyor. Vurgu rengi tek:
// ekranda aynı anda yalnızca bir "asıl eylem" var (Kaydet), gerisi sakin.
// --------------------------------------------------------------------------
constexpr auto kZemin        = "#eef1f5"; // pencere zemini
constexpr auto kKart         = "#ffffff"; // kutu/tablo zemini
constexpr auto kKenarlik     = "#d7dee7";
constexpr auto kKenarlikKoyu = "#c2ccd8"; // fare üzerindeyken
constexpr auto kMetin        = "#1b2733";
constexpr auto kMetinSolgun  = "#6b7787";
constexpr auto kVurgu        = "#1e6bb8"; // asıl eylem, seçim, odak
constexpr auto kVurguKoyu    = "#175a9c";
constexpr auto kVurguSolgun  = "#dcebf9"; // seçili satır zemini
constexpr auto kKenarCubugu  = "#1d2b3a"; // sol menü
constexpr auto kKenarMetin   = "#aebbcb";
constexpr auto kSatirAlt     = "#f7f9fc"; // tabloda değişen satır
constexpr auto kTehlike      = "#b23c3c"; // silme gibi geri alınamaz eylemler

// Programın açılıştaki temel yazı boyutu. Ölçekleme her zaman BUNUN üzerine
// uygulanır; bir önceki ölçeğin üzerine uygulansaydı %110 iki kez
// seçildiğinde yazı %121'e çıkardı (bileşik büyüme).
double basePointSize()
{
    static const double taban = [] {
        const double p = QApplication::font().pointSizeF();
        // Bazı platformlarda font piksel cinsinden tanımlıdır ve
        // pointSizeF() -1 döner; o durumda makul bir varsayılana düşülür.
        return p > 0 ? p : 9.0;
    }();
    return taban;
}

int g_mevcut = kDefaultScale;

// Sistemde kurulu ilk uygun yazı tipini seçer. Windows'ta Segoe UI beklenir;
// yoksa sırayla diğerlerine düşülür ve hiçbiri yoksa Qt'nin varsayılanı
// kullanılır — yazı tipi yüzünden program açılmamazlık etmemeli.
QString arayuzYaziTipi()
{
    const QStringList adaylar = {QStringLiteral("Segoe UI"), QStringLiteral("Inter"),
                                 QStringLiteral("Noto Sans"), QStringLiteral("DejaVu Sans")};
    const QStringList kurulu = QFontDatabase::families();
    for (const QString &ad : adaylar) {
        if (kurulu.contains(ad))
            return ad;
    }
    return QApplication::font().family();
}

QString stylesheet()
{
    return QStringLiteral(R"(
/* --- genel ------------------------------------------------------------- */
QWidget {
    background: %ZEMIN%;
    color: %METIN%;
}
QMainWindow, QDialog { background: %ZEMIN%; }
QLabel { background: transparent; }
QToolTip {
    background: %KENARCUBUGU%;
    color: #ffffff;
    border: none;
    padding: 5px 8px;
}

/* --- sol menü ---------------------------------------------------------- */
/* Koyu bir şerit: gezinme ile içerik arasındaki sınır tek bakışta belli
   olsun, kullanıcı hangi ekranda olduğunu aramasın. */
QListWidget#navList {
    background: %KENARCUBUGU%;
    color: %KENARMETIN%;
    border: none;
    outline: none;
    padding: 10px 8px;
}
QListWidget#navList::item {
    padding: 11px 14px;
    border-radius: 7px;
    margin-bottom: 3px;
}
QListWidget#navList::item:hover { background: rgba(255, 255, 255, 0.07); }
QListWidget#navList::item:selected {
    background: %VURGU%;
    color: #ffffff;
    font-weight: 600;
}

/* --- kutular (kart görünümü) ------------------------------------------- */
QGroupBox {
    background: %KART%;
    border: 1px solid %KENARLIK%;
    border-radius: 10px;
    margin-top: 12px;
    padding: 16px 14px 12px 14px;
    font-weight: 600;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 12px;
    padding: 0 6px;
    color: %METINSOLGUN%;
    background: %ZEMIN%;
}

/* --- girdi alanları ---------------------------------------------------- */
QLineEdit, QDateEdit, QSpinBox, QComboBox, QPlainTextEdit, QTextEdit {
    background: %KART%;
    border: 1px solid %KENARLIK%;
    border-radius: 6px;
    padding: 6px 9px;
    selection-background-color: %VURGU%;
    selection-color: #ffffff;
}
QLineEdit:hover, QDateEdit:hover, QSpinBox:hover, QComboBox:hover {
    border-color: %KENARLIKKOYU%;
}
/* Odaklı alan belirgin olmalı: klavyeyle dolaşan kullanıcı nerede olduğunu
   görmeli. Kalınlık değil renk değişiyor — kalınlık değişse alan zıplardı. */
QLineEdit:focus, QDateEdit:focus, QSpinBox:focus, QComboBox:focus,
QPlainTextEdit:focus, QTextEdit:focus {
    border-color: %VURGU%;
}
QLineEdit:disabled, QDateEdit:disabled, QSpinBox:disabled, QComboBox:disabled {
    background: #f2f4f7;
    color: %METINSOLGUN%;
}
QComboBox::drop-down { border: none; width: 22px; }
/* Tarih kutusu: ic bosluk + acilir ok, dar birakilirsa yili kirpiyordu
   ("04.09.202"). Alt sinir verilerek tam tarih her zaman sigar. */
QDateEdit { min-width: 108px; }
QComboBox QAbstractItemView {
    background: %KART%;
    border: 1px solid %KENARLIK%;
    selection-background-color: %VURGUSOLGUN%;
    selection-color: %METIN%;
    outline: none;
}
QSpinBox::up-button, QSpinBox::down-button { width: 16px; border: none; }
QDateEdit::drop-down { border: none; width: 22px; }

/* --- düğmeler ---------------------------------------------------------- */
QPushButton {
    background: %KART%;
    border: 1px solid %KENARLIK%;
    border-radius: 6px;
    padding: 7px 16px;
    font-weight: 500;
}
QPushButton:hover { background: #f4f7fa; border-color: %KENARLIKKOYU%; }
QPushButton:pressed { background: #e9eef4; }
QPushButton:disabled { color: #a6b0bd; background: #f4f6f9; }

/* Asıl eylem (Kaydet) vurgulu; ekranda yalnızca bir tane olur. */
QPushButton[birincil="true"] {
    background: %VURGU%;
    border-color: %VURGU%;
    color: #ffffff;
    font-weight: 600;
}
QPushButton[birincil="true"]:hover { background: %VURGUKOYU%; border-color: %VURGUKOYU%; }
QPushButton[birincil="true"]:disabled { background: #9dbcdc; border-color: #9dbcdc; }

/* Geri alınamaz eylem (Sil): rengiyle uyarır ama vurgulu düğme kadar
   dikkat çekmez — asıl eylem her zaman Kaydet'tir. */
QPushButton[tehlike="true"] { color: %TEHLIKE%; }
QPushButton[tehlike="true"]:hover { background: #fbf0f0; border-color: %TEHLIKE%; }

/* --- tablolar ---------------------------------------------------------- */
QTableView {
    background: %KART%;
    alternate-background-color: %SATIRALT%;
    border: 1px solid %KENARLIK%;
    border-radius: 8px;
    gridline-color: #edf1f6;
    selection-background-color: %VURGUSOLGUN%;
    selection-color: %METIN%;
    outline: none;
}
QTableView::item { padding: 5px 7px; }
QTableView::item:focus { border: none; }
QHeaderView::section {
    background: #f4f6f9;
    color: %METINSOLGUN%;
    border: none;
    border-bottom: 1px solid %KENARLIK%;
    border-right: 1px solid #e8edf3;
    padding: 8px 7px;
    font-weight: 600;
}
QHeaderView::section:last { border-right: none; }
QTableCornerButton::section { background: #f4f6f9; border: none; }

/* --- kaydırma çubukları ------------------------------------------------ */
/* İnce ve sessiz: liste uzun olduğunda yerini belli etsin ama tablonun
   içinden yer çalmasın. */
QScrollBar:vertical { background: transparent; width: 11px; margin: 0; }
QScrollBar::handle:vertical {
    background: #c5cedb;
    border-radius: 5px;
    min-height: 28px;
}
QScrollBar::handle:vertical:hover { background: #aab6c6; }
QScrollBar:horizontal { background: transparent; height: 11px; margin: 0; }
QScrollBar::handle:horizontal {
    background: #c5cedb;
    border-radius: 5px;
    min-width: 28px;
}
QScrollBar::handle:horizontal:hover { background: #aab6c6; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

/* --- onay kutusu ------------------------------------------------------- */
QCheckBox { spacing: 7px; background: transparent; }
QCheckBox::indicator {
    width: 15px;
    height: 15px;
    border: 1px solid %KENARLIKKOYU%;
    border-radius: 4px;
    background: %KART%;
}
QCheckBox::indicator:checked { background: %VURGU%; border-color: %VURGU%; }

/* --- menü çubuğu ------------------------------------------------------- */
QMenuBar { background: %ZEMIN%; border-bottom: 1px solid %KENARLIK%; }
QMenuBar::item { padding: 6px 11px; background: transparent; }
QMenuBar::item:selected { background: #e3e9f0; border-radius: 5px; }
QMenu { background: %KART%; border: 1px solid %KENARLIK%; padding: 5px; }
QMenu::item { padding: 6px 22px 6px 14px; border-radius: 5px; }
QMenu::item:selected { background: %VURGUSOLGUN%; }

/* --- teklif ekranındaki toplam ----------------------------------------- */
/* Belgenin en çok bakılan sayısı; ekranda da öyle görünmeli. */
QLabel#toplamBaslik {
    color: %METINSOLGUN%;
    font-weight: 600;
    padding-right: 10px;
}
QLabel#genelLabel {
    color: %VURGUKOYU%;
    font-size: 19px;
    font-weight: 700;
    padding-right: 4px;
}
QLabel#teklifNoLabel { color: %METINSOLGUN%; font-weight: 600; }
)")
        .replace(QStringLiteral("%ZEMIN%"), QLatin1String(kZemin))
        .replace(QStringLiteral("%KART%"), QLatin1String(kKart))
        .replace(QStringLiteral("%KENARLIKKOYU%"), QLatin1String(kKenarlikKoyu))
        .replace(QStringLiteral("%KENARLIK%"), QLatin1String(kKenarlik))
        .replace(QStringLiteral("%METINSOLGUN%"), QLatin1String(kMetinSolgun))
        .replace(QStringLiteral("%METIN%"), QLatin1String(kMetin))
        .replace(QStringLiteral("%VURGUSOLGUN%"), QLatin1String(kVurguSolgun))
        .replace(QStringLiteral("%VURGUKOYU%"), QLatin1String(kVurguKoyu))
        .replace(QStringLiteral("%VURGU%"), QLatin1String(kVurgu))
        .replace(QStringLiteral("%KENARCUBUGU%"), QLatin1String(kKenarCubugu))
        .replace(QStringLiteral("%KENARMETIN%"), QLatin1String(kKenarMetin))
        .replace(QStringLiteral("%SATIRALT%"), QLatin1String(kSatirAlt))
        .replace(QStringLiteral("%TEHLIKE%"), QLatin1String(kTehlike));
}

} // namespace

void applyStyle()
{
    QFont f = QApplication::font();
    f.setFamily(arayuzYaziTipi());
    QApplication::setFont(f);

    qApp->setStyleSheet(stylesheet());
}

void applyUiScale(int yuzde)
{
    const int olcek = qBound(kMinScale, yuzde, kMaxScale);

    QFont f = QApplication::font();
    f.setPointSizeF(basePointSize() * olcek / 100.0);
    QApplication::setFont(f);

    g_mevcut = olcek;
}

int currentScale()
{
    return g_mevcut;
}

void applyFromSettings(const Settings &settings)
{
    // Sıra önemli: applyStyle yazı tipi AİLESİNİ değiştirir, applyUiScale
    // ise BOYUTU. Ters sırada çağrılsalardı aile değişimi boyutu sıfırlardı.
    applyStyle();
    applyUiScale(static_cast<int>(settings.intValueOr(Settings::keyUiScale(), kDefaultScale)));
}

bool setAndStore(Settings &settings, int yuzde, QString *errorOut)
{
    const int olcek = qBound(kMinScale, yuzde, kMaxScale);
    applyUiScale(olcek);
    return settings.setInt(Settings::keyUiScale(), olcek, errorOut);
}

} // namespace Theme
