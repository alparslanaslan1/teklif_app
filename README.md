# Teklif

Qt 6 / C++17 ile yazılmış masaüstü teklif hazırlama programı. Katalogdan kalem
seçip teklif oluşturur, KDV'li toplamları hesaplar, yazdırır/PDF'e verir.

## Gereksinimler

- CMake 3.21+
- Qt 6 (Core, Gui, Widgets, Sql, Network, PrintSupport, Test)
- C++17 destekleyen bir derleyici

## Derleme

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
```

Qt Creator ve VS Code `CMakePresets.json` dosyasını doğrudan okur; ayrıca
yapılandırma gerekmez.

## Proje yapısı

| Klasör | Sorumluluk |
|---|---|
| `src/core` | İş mantığı. Widgets'e bağımlı **değil**, arayüz açmadan test edilebilir. |
| `src/update` | Otomatik güncelleme. Yalnızca Network'e bağlı, arayüz içermez. |
| `src/print` | Baskı/PDF ve firma logosu. Ekran ve yazıcı **aynı** yerleşim kodunu kullanır. |
| `src/ui` | Teklif ekranı ve bileşenleri. Core'a bağımlı, tersi değil. |
| `tests` | Her katman için birim/entegrasyon testleri. |
| `packaging` | Inno Setup kurulum betiği. |
| `resources` | Uygulama simgesi ve Windows kaynak şablonu. |

Veritabanı programın kurulu olduğu klasörde **değil**, kullanıcının veri
dizinindedir (Windows'ta `%APPDATA%\OzYapi\Teklif\teklif.db`). Böylece program
güncellendiğinde veri yerinde kalır.

## Sürüm numarası

Tek doğruluk kaynağı `CMakeLists.txt` içindeki `project(... VERSION x.y.z)`
satırıdır. Buradan `version.h` üretilir (`APP_VERSION`, `APP_GIT_SHA`,
`APP_BUILD_DATE`) ve build klasörüne yazılır — repoya girmez.

CI, git tag'inden gelen sürümü `-DTEKLIF_VERSION=0.2.0` ile geçer.

## Yeni sürüm yayınlama

```bash
# 1. CMakeLists.txt -> project(TeklifApp VERSION 0.2.0 ...)
git commit -am "Sürüm 0.2.0"

# 2. Tag at ve gönder
git tag v0.2.0
git push && git push --tags
```

`.github/workflows/release.yml` gerisini yapar:

1. Windows'ta Release derler, testleri koşar
2. `windeployqt` ile Qt DLL'lerini toplar (kullanıcıda Qt kurulu değil)
3. ZIP'ler ve SHA-256'sını hesaplar
4. `latest.json` üretir
5. İkisini GitHub Release'e yükler

## Paketleme ve kurulum

`resources/teklif.ico` uygulama simgesidir; Windows'ta `app.rc` üzerinden
exe'ye gömülür ve dosya özelliklerindeki sürüm bilgisini de o doldurur.

`packaging/installer.iss` Inno Setup betiğidir. CI bunu `ISCC.exe` ile
derleyip `TeklifKurulum-<sürüm>.exe` üretir.

Kurulumun iki bilinçli özelliği var:

- **`PrivilegesRequired=lowest`** — yönetici yetkisi olmayan kullanıcı da
  kurabilir. Bu kipte program `%LOCALAPPDATA%\Programs` altına kurulur;
  veritabanı zaten `%APPDATA%` altında olduğu için kurulum yeri veriyi
  etkilemez.
- **`AppId` asla değişmez** — Windows kurulu sürümü bununla tanır.
  Değiştirilirse yeni sürüm eskisinin yanına kurulur ve kullanıcıda iki
  program görünür. Aynı `AppId` sayesinde "üzerine kurulum" (1.3 → 1.4)
  dosyaları değiştirir, kullanıcı verisine dokunmaz.

Kaldırma işlemi `%APPDATA%` altındaki veritabanını **silmez**: program
kaldırılıp yeniden kurulsa da teklifler, müşteriler ve ayarlar yerinde kalır.

## İlk çalıştırma

Program ilk açıldığında firma bilgilerini soran ve isteğe bağlı olarak örnek
katalog kalemleri ekleyen bir sihirbaz gösterilir. Atlanabilir; atlanırsa bir
daha sorulmaz ve aynı bilgiler Ayarlar/Katalog ekranlarından girilebilir.

## Otomatik güncelleme nasıl çalışır

Program açılışta sabit bir adresteki manifest dosyasına bakar:

```
https://github.com/<kullanici>/<repo>/releases/latest/download/latest.json
```

```json
{
  "version":    "0.2.0",
  "minVersion": "",
  "url":        "https://.../teklif-0.2.0-win64.zip",
  "sha256":     "a1b2c3...",
  "notes":      "Teklif listesi ekranı eklendi"
}
```

Akış:

1. `Updater::checkForUpdate()` manifesti çeker (5 sn zaman aşımı)
2. `compareVersions()` ile sunucudaki sürüm çalışandan yeni mi bakılır
3. Yeniyse `updateAvailable` sinyali; kullanıcıya sorulur
4. Onay verilirse paket indirilir ve **SHA-256 doğrulanır**
5. Yardımcı program devralır: ana program kapanır, dosyalar değiştirilir,
   program yeniden başlatılır

**Tasarım kuralı:** güncelleme hiçbir zaman programın açılmasını engellemez.
İnternet yoksa, sunucu kapalıysa veya manifest bozuksa `checkFailed` yayınlanır
ve program normal çalışır. Tek istisna `minVersion`'dır: çalışan sürüm ondan
eskiyse güncelleme zorunlu kılınabilir (veri şeması değiştiğinde eski programın
yeni `.db` dosyasını açamaması bu yüzden önemlidir — bkz. `Db::migrateStep`).

Güvenlik: manifest yalnızca `https` adresi kabul eder ve indirilen paketin
özeti doğrulanır. Paket kod imzalı değilse Windows SmartScreen uyarı verebilir.

## Veri şeması göçü

`Db::kSchemaVersion` uygulamanın bildiği en güncel şema sürümüdür. Eski bir
`.db` dosyası açıldığında `Db::openAndMigrate` önce yanına
`teklif.db.bak-YYYYAAGG-SSDDss` kopyasını alır, sonra göç adımlarını sırayla
uygular. Bir adım başarısız olursa geri alınır ve dosya bozulmaz.

Yeni migration eklerken:

1. `kSchemaVersion`'ı bir artır
2. `Db::migrateStep()` içine yeni bir `case` ekle
3. Eski `case`'leri **silme** — kullanıcıda hâlâ o sürümden dosya olabilir
