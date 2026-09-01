#include "teklif/ui/item_table_model.h"

#include <QColor>

ItemTableModel::ItemTableModel(QObject *parent) : QAbstractTableModel(parent) {}

int ItemTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

int ItemTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(ColCount);
}

QVariant ItemTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};

    const Item &it = m_items.at(index.row());

    if (role == Qt::TextAlignmentRole) {
        return index.column() == ColFiyat ? QVariant(Qt::AlignRight | Qt::AlignVCenter)
                                           : QVariant(Qt::AlignLeft | Qt::AlignVCenter);
    }

    // Pasif kalemler soluk gösterilir: listede neden "eksik" görünmediklerini
    // kullanıcı bir bakışta anlasın.
    if (role == Qt::ForegroundRole && !it.aktif)
        return QVariant(QColor(140, 140, 140));

    if (role != Qt::DisplayRole)
        return {};

    switch (index.column()) {
    case ColKod:
        return it.kod;
    case ColAd:
        return it.ad;
    case ColBirim:
        return it.birim;
    case ColFiyat:
        return it.varsayilanFiyat.toString();
    case ColKategori:
        // categoryId 0 = kategorisiz; silinmiş bir kategoriye işaret eden id
        // de burada boş görünür (QHash::value bulamazsa boş döner).
        return it.categoryId != 0 ? m_kategoriAdlari.value(it.categoryId) : QString();
    case ColDurum:
        return it.aktif ? QStringLiteral("Aktif") : QStringLiteral("Pasif");
    default:
        return {};
    }
}

QVariant ItemTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);

    switch (section) {
    case ColKod:
        return QStringLiteral("Kod");
    case ColAd:
        return QStringLiteral("Ad");
    case ColBirim:
        return QStringLiteral("Birim");
    case ColFiyat:
        return QStringLiteral("Varsayılan Fiyat");
    case ColKategori:
        return QStringLiteral("Kategori");
    case ColDurum:
        return QStringLiteral("Durum");
    default:
        return {};
    }
}

void ItemTableModel::setItems(const QVector<Item> &items, const QHash<qint64, QString> &kategoriAdlari)
{
    beginResetModel();
    m_items = items;
    m_kategoriAdlari = kategoriAdlari;
    endResetModel();
}

Item ItemTableModel::at(int row) const
{
    if (row < 0 || row >= m_items.size())
        return {};
    return m_items.at(row);
}
