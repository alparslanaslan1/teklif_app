#pragma once

#include "core/models.h"

#include <QAbstractTableModel>
#include <QVector>

// Teklif ekranındaki kalem tablosunun modeli.
//
// Açıklama, Miktar ve B. Fiyat hücrede DOĞRUDAN düzenlenebilir (plan
// kararı: popup sadece katalogdan yeni kalem eklerken açılır, sonraki
// düzeltmeler hücreden yapılır). #, Birim ve Tutar salt okunur — Tutar
// her zaman miktar × fiyattan yeniden hesaplanır, asla elle yazılamaz.
//
// Geçersiz bir düzenleme (harf, boş, negatif, sıfır miktar) setData()
// içinde reddedilir (false döner) ve m_lines DEĞİŞMEZ — Qt'nin standart
// davranışı gereği görünüm hücreyi eski (modeldeki) değeriyle yeniden
// çizer, ayrıca bir "geri al" koduna gerek yoktur.
class QuoteLineModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { ColSira = 0, ColAciklama, ColBirim, ColMiktar, ColBirimFiyat, ColTutar, ColCount };

    explicit QuoteLineModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // Katalogdan seçilen bir kalemi yeni bir satır olarak ekler. aciklama/
    // birim/fiyat kaynak Item'dan (veya popup'ta değiştirilmiş fiyattan)
    // KOPYALANIR — item.id'ye referans TUTULMAZ.
    void addLine(const Item &kaynak, double miktar, Money birimFiyat, const QString &satirNotu);

    // Satırı siler; kalan satırların sıra numaraları 1'den yeniden dizilir.
    void removeLine(int row);

    const QVector<QuoteLine> &lines() const { return m_lines; }
    // DB'den okunan satırları yükler (teklif açma). sira alanları güvenlik
    // için yeniden dizilir (DB'de zaten 1..n olmalı, ama garanti edilmez).
    void setLines(const QVector<QuoteLine> &lines);
    void clear();

signals:
    // Herhangi bir satır tutarı ekleme/silme/düzenleme ile değiştiğinde;
    // dinleyen taraf (PageQuote) toplamları yeniden hesaplar.
    void totalsMayHaveChanged();

private:
    QVector<QuoteLine> m_lines;
    void renumber();
    void recomputeTutar(int row);
};
