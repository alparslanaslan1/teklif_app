; Inno Setup kurulum betiği — Windows installer üretir.
;
; NEDEN INNO SETUP: "üzerine kurulum" davranışı (1.3 -> 1.4) kutudan çıktığı
; gibi doğru çalışıyor; aynı AppId'ye sahip eski sürüm bulunur, dosyalar
; değiştirilir, kullanıcı verisine dokunulmaz. NSIS'te aynı şeyi elde etmek
; elle betik yazmayı gerektirir. Part 8'deki uzaktan güncelleme de bu
; installer'ı sessiz kipte (/SILENT) çalıştırır.
;
; Derleme: iscc /DAppVersion=0.2.0 /DSourceDir=..\dist\TeklifApp installer.iss
; CI bunu release.yml içinde yapar.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist\TeklifApp"
#endif

#define AppName      "Teklif"
#define AppExeName   "teklif_app.exe"
#define AppPublisher "Karasu Vizyon Doğalgaz"

[Setup]
; AppId ASLA DEĞİŞMEMELİ: Windows kurulu sürümü bununla tanır. Değişirse
; yeni sürüm eskisinin YANINA kurulur ve kullanıcıda iki program görünür.
AppId={{8F3C21A4-9D5E-4B72-A1C6-7E0B94D2F318}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
VersionInfoVersion={#AppVersion}

DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
; Kurulum sihirbazında ne yükleneceğini seçtirmiyoruz; tek bir program var.
DisableDirPage=no

; PrivilegesRequired=lowest: yönetici yetkisi OLMAYAN kullanıcı da kurabilsin.
; Bu kipte {autopf} = %LOCALAPPDATA%\Programs, yani Program Files'a yazma
; denemesi hiç yapılmaz. Veritabanı zaten %APPDATA%'da olduğu için programın
; nereye kurulduğu veriyi etkilemez.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

OutputDir=..\dist
OutputBaseFilename=TeklifKurulum-{#AppVersion}
SetupIconFile=..\resources\teklif.ico
UninstallDisplayIcon={app}\{#AppExeName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

; Windows 10 ve üzeri.
MinVersion=10.0

[Languages]
Name: "turkish"; MessagesFile: "compiler:Languages\Turkish.isl"

[Tasks]
Name: "desktopicon"; Description: "Masaüstü kısayolu oluştur"; GroupDescription: "Ek kısayollar:"

[Files]
; windeployqt'nin hazırladığı klasörün TAMAMI: exe, Qt DLL'leri ve
; eklentiler (sqlite sürücüsü, platform eklentisi, yazdırma). Bunlar olmadan
; program Qt kurulu olmayan bir makinede açılmaz.
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{group}\{#AppName} Kaldır"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
; Sessiz kurulumda (uzaktan güncelleme) programı kendiliğinden başlatma:
; güncelleyen program zaten kendini yeniden açacak.
Filename: "{app}\{#AppExeName}"; Description: "{#AppName} programını başlat"; \
    Flags: nowait postinstall skipifsilent

[UninstallDelete]
; KULLANICI VERİSİ SİLİNMEZ. Veritabanı %APPDATA%\KarasuVizyon\Teklif\teklif.db
; altındadır ve buraya HİÇ dokunulmaz — program kaldırılıp yeniden kurulsa
; da teklifler, müşteriler ve ayarlar yerinde kalır. Yalnızca kurulum
; klasöründe kalan artıklar temizlenir.
Type: filesandordirs; Name: "{app}\platforms"
Type: filesandordirs; Name: "{app}\sqldrivers"
Type: filesandordirs; Name: "{app}\imageformats"
Type: filesandordirs; Name: "{app}\styles"
