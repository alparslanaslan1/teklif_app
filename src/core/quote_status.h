#pragma once

#include <QString>
#include <QStringList>

// Teklif durumları. quotes.durum sütununa METİN olarak yazılır.
//
// NEDEN METİN, ENUM DEĞİL: değer veritabanında saklandığı için sayısal bir
// enum, şemayı okuyan herkesin (DB Browser, yedek dosyası, ileride bir rapor)
// eşleşme tablosunu bilmesini gerektirirdi. Metin kendi kendini açıklar.
//
// NEDEN SABİT FONKSİYON, HER YERDE STRING DEĞİL: "Gönderildi" ile
// "Gonderildi" arasındaki fark derleme hatası vermez, sessizce iki ayrı
// duruma yol açar. Sabitler tek yerde.
namespace QuoteStatus {

QString taslak();      // yeni oluşturulan her teklifin başlangıç durumu
QString gonderildi();  // müşteriye iletildi
QString onaylandi();   // müşteri kabul etti
QString reddedildi();  // müşteri kabul etmedi

// Arayüzdeki açılır listeyi doldurmak için, iş akışı sırasıyla.
QStringList all();

// Bilinmeyen bir durum metni veritabanına yazılmasın diye. Eski bir
// sürümden gelen ya da elle düzenlenmiş bir kayıt bilinmeyen bir durum
// içerebilir; okuma tarafı bunu reddetmez, yalnızca yazma tarafı doğrular.
bool isValid(const QString &durum);

} // namespace QuoteStatus
