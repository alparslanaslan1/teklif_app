#pragma once

#include "teklif/print/document_layout.h"

#include <QString>

class QPrinter;

// Belgeyi gerçek bir çıktıya verir: yazıcı ya da PDF.
//
// Yerleşimi KENDİSİ yapmaz — DocumentLayout'u çağırır. Böylece ekran
// önizlemesi, yazıcı ve PDF üçü de birebir aynı çizimi üretir.
class PrintService
{
public:
    // Belgeyi verilen QPrinter'a basar. Çağıran taraf yazıcıyı seçmiş
    // (QPrintDialog) ya da PDF'e ayarlamış olmalıdır.
    // Yazıcı hazır değilse false döner ve errorOut doldurulur — çökmez.
    static bool paint(const DocumentContext &ctx, QPrinter *printer, QString *errorOut = nullptr);

    // Belgeyi doğrudan PDF dosyasına yazar. Yazıcı sürücüsü GEREKMEZ;
    // Qt'nin kendi PDF motoru kullanılır, bu yüzden yazıcısı olmayan bir
    // makinede de çalışır.
    static bool exportPdf(const DocumentContext &ctx, const QString &filePath,
                           QString *errorOut = nullptr);

    // "2026-0043_AhmetYilmaz.pdf" biçiminde dosya adı önerir.
    // Dosya sisteminde sorun çıkaran karakterler (/ \ : * ? " < > |) ve
    // Türkçe harfler ASCII karşılıklarına indirilir — Türkçe olmayan bir
    // sistemde ya da e-postayla gönderildiğinde ad bozulmasın diye.
    static QString suggestedFileName(const Quote &quote, const Customer &customer);
};
