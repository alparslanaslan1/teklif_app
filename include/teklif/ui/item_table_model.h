#pragma once

#include "teklif/core/models.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QVector>

// Katalog ekranındaki kalem tablosunun modeli.
//
// Salt okunurdur: düzenleme sağdaki formdan yapılır. Teklif tablosundan
// farklı olarak burada hücre düzenleme YOK — katalogda bir fiyatı yanlışlıkla
// değiştirmek sonraki tüm tekliflere sızardı, bilinçli bir "Kaydet" adımı
// istenir.
class ItemTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { ColKod = 0, ColAd, ColBirim, ColFiyat, ColKategori, ColDurum, ColCount };

    explicit ItemTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    // kategoriAdlari: categoryId -> ad. Kalem başına ayrı sorgu atmamak için
    // dışarıdan hazır verilir (bkz. RepoItems::listCategories).
    void setItems(const QVector<Item> &items, const QHash<qint64, QString> &kategoriAdlari);

    Item at(int row) const;

private:
    QVector<Item> m_items;
    QHash<qint64, QString> m_kategoriAdlari;
};
