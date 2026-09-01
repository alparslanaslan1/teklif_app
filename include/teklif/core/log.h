#pragma once

#include <QLoggingCategory>
#include <QString>

// Uygulama günlüğü.
//
// NEDEN VAR: kullanıcı bir sorun bildirdiğinde elimizde hiçbir iz yoktu.
// Arayüzde gösterilen hata mesajları kapatılınca kayboluyor, sessizce
// geçilen durumlar (güncelleme denetiminin başarısız olması gibi) ise hiç
// görünmüyordu. Günlük dosyası, uzaktaki bir makinede ne olduğunu sormanın
// tek yolu.
//
// Kategoriler: filtrelenebilmeleri için ayrı. Qt'nin QT_LOGGING_RULES ortam
// değişkeniyle ör. "teklif.db.debug=true" denilebilir.
Q_DECLARE_LOGGING_CATEGORY(logDb)      // veritabanı, migration, yedekleme
Q_DECLARE_LOGGING_CATEGORY(logUpdate)  // güncelleme denetimi ve indirme
Q_DECLARE_LOGGING_CATEGORY(logApp)     // genel

namespace Log {

// Günlük dosyasının yolu (kullanıcının veri dizininde, veritabanının yanında).
QString filePath();

// qDebug/qWarning/qCritical çıktısını hem konsola hem günlük dosyasına
// yönlendirir. main() içinde, QApplication kurulduktan hemen sonra bir kez
// çağrılır.
//
// Dosya kMaksBoyut'u aşarsa bir öncekinin üzerine döndürülür (.1 uzantılı):
// sınırsız büyüyen bir günlük, diski dolduran sessiz bir hataya dönüşür.
void install();

// Döndürmeden önce izin verilen en büyük dosya boyutu.
constexpr qint64 kMaksBoyut = 2 * 1024 * 1024; // 2 MB

} // namespace Log
