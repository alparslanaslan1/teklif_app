#pragma once

#include "teklif/core/models.h"

#include <QAbstractTableModel>
#include <QVector>

// Teklif ekranındaki kalem tablosunun modeli.
//
// Satırlar İKİ yoldan girilir ve ikisi de aynı satır tipini üretir:
//   1. Katalogdan seçerek — arama kutusundan gelen kalem, açıklaması/birimi/
//      fiyatı kopyalanmış hâlde eklenir (bkz. addLine)
//   2. Elle — boş bir satır açılır ve hücreler doldurulur (bkz. addEmptyLine)
// Katalogda olmayan bir iş kalemi için katalog kaydı açmak zorunda kalmamak
// önemliydi: teklifin çoğu satırı tekrar eden malzemeler olsa da her teklifte
// bir iki tanesi o işe özeldir.
//
// Tutar HİÇBİR ZAMAN elle yazılamaz; her zaman miktar × birim fiyattan
// yeniden hesaplanır. Sıra numarası da öyle — satır silinince kalanlar
// 1'den yeniden dizilir.
//
// Geçersiz bir düzenleme (harf, negatif sayı) setData() içinde reddedilir
// (false döner) ve m_lines DEĞİŞMEZ — Qt'nin standart davranışı gereği
// görünüm hücreyi eski değeriyle yeniden çizer, ayrı bir "geri al" koduna
// gerek yoktur.
class QuoteLineModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { ColSira = 0, ColAciklama, ColBirim, ColMiktar, ColBirimFiyat, ColTutar, ColNot,
                  ColCount };

    explicit QuoteLineModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // Katalogdan seçilen bir kalemi yeni bir satır olarak ekler. aciklama/
    // birim/fiyat kaynak Item'dan KOPYALANIR — item.id'ye referans TUTULMAZ,
    // katalogda fiyat değişse bile kaydedilmiş teklif etkilenmez.
    // Eklenen satırın numarasını döner.
    int addLine(const Item &kaynak, double miktar, Money birimFiyat, const QString &satirNotu);

    // Boş bir satır açar: açıklama boş, miktar 1, fiyat 0. Kullanıcı hücreleri
    // doldurur. Eklenen satırın numarasını döner ki çağıran taraf imleci
    // doğrudan açıklama hücresine götürebilsin.
    int addEmptyLine();

    // Satırı siler; kalan satırların sıra numaraları 1'den yeniden dizilir.
    void removeLine(int row);

    const QVector<QuoteLine> &lines() const { return m_lines; }

    // Açıklaması boş olan satırlar ayıklanmış hâli. Boş satır ekranda
    // serbestçe durabilir (kullanıcı henüz doldurmamıştır) ama kaydedilen
    // ya da yazdırılan belgeye girmemelidir.
    QVector<QuoteLine> filledLines() const;

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
