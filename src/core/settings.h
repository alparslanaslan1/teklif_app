#pragma once

#include <QSqlDatabase>
#include <QString>
#include <optional>

// settings tablosuna (key/value) erişimin TEK yeri.
//
// NEDEN VAR: Bu tablo daha önce doğrudan RepoQuotes içinden ham SQL ile
// okunup yazılıyordu — teklif deposunun ayarlar tablosunun şemasını bilmesi
// gerekiyordu. Anahtar adı bir yazım hatasıyla değişse hiçbir derleme hatası
// çıkmaz, sayaç sessizce sıfırdan başlardı. Artık anahtarlar aşağıda sabit
// olarak duruyor ve şemayı yalnızca bu sınıf biliyor.
//
// Part 7'deki Ayarlar ekranı da (firma bilgileri, KDV oranı, yazı boyutu,
// PDF klasörü) aynı sınıfı kullanır; yeni bir ayar eklemek buraya bir sabit
// eklemekten ibarettir.
class Settings
{
public:
    explicit Settings(QSqlDatabase db);

    // --- Bilinen anahtarlar -------------------------------------------------
    // Metin yerine bu sabitleri kullan: yazım hatası derleme hatasına dönüşür.
    static QString keyQuoteCounter();   // teklif no sayacı (sürekli artar)
    static QString keyCompanyName();    // belge antetindeki firma unvanı
    static QString keyCompanyAddress();
    static QString keyCompanyPhone();
    static QString keyCompanyEmail();
    static QString keyCompanyTaxOffice();
    static QString keyCompanyTaxNo();
    static QString keyDefaultVatRate();  // yeni tekliflerin varsayılan KDV oranı
    static QString keyUiScale();         // arayüz ölçeği (%)
    static QString keyDocumentFontPt();  // belge yazı boyutu (pt)
    static QString keyPdfFolder();
    static QString keyTermsText();       // varsayılan şartlar metni
    static QString keyUpdateSkipVersion();   // "bu sürümü atla" seçilen sürüm
    static QString keyUpdateCheckEnabled();  // açılışta güncelleme denetimi

    // Anahtar yoksa std::nullopt döner — "kayıt yok" ile "değeri boş metin"
    // birbirinden ayırt edilebilsin diye.
    std::optional<QString> value(const QString &key, QString *errorOut = nullptr) const;

    // Anahtar yoksa ya da okunamazsa varsayilan döner. Çağıran tarafın her
    // seferinde optional açması gerekmesin diye.
    QString valueOr(const QString &key, const QString &varsayilan = QString()) const;
    qint64 intValueOr(const QString &key, qint64 varsayilan) const;
    bool boolValueOr(const QString &key, bool varsayilan) const;

    // Anahtar yoksa ekler, varsa günceller (UPSERT).
    bool setValue(const QString &key, const QString &value, QString *errorOut = nullptr);
    bool setInt(const QString &key, qint64 value, QString *errorOut = nullptr);
    bool setBool(const QString &key, bool value, QString *errorOut = nullptr);

    bool remove(const QString &key, QString *errorOut = nullptr);

private:
    QSqlDatabase m_db;
};
