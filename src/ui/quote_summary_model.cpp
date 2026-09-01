#include "teklif/ui/quote_summary_model.h"

QuoteSummaryModel::QuoteSummaryModel(QObject *parent) : QAbstractTableModel(parent) {}

int QuoteSummaryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_quotes.size();
}

int QuoteSummaryModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(ColCount);
}

QVariant QuoteSummaryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_quotes.size())
        return {};

    const QuoteSummary &s = m_quotes.at(index.row());

    if (role == Qt::TextAlignmentRole) {
        return index.column() == ColToplam ? QVariant(Qt::AlignRight | Qt::AlignVCenter)
                                            : QVariant(Qt::AlignLeft | Qt::AlignVCenter);
    }

    if (role != Qt::DisplayRole)
        return {};

    switch (index.column()) {
    case ColTeklifNo:
        return s.teklifNo;
    case ColTarih:
        // Ekranda Türk biçimi; sıralama zaten SQL tarafında ISO metinle
        // yapıldığı için gösterim biçimi sıralamayı etkilemez.
        return s.tarih.isValid() ? s.tarih.toString(QStringLiteral("dd.MM.yyyy")) : QString();
    case ColMusteri:
        return s.customerUnvan;
    case ColDurum:
        return s.durum;
    case ColToplam:
        return s.genelToplam.toString();
    default:
        return {};
    }
}

QVariant QuoteSummaryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);

    switch (section) {
    case ColTeklifNo:
        return QStringLiteral("Teklif No");
    case ColTarih:
        return QStringLiteral("Tarih");
    case ColMusteri:
        return QStringLiteral("Müşteri");
    case ColDurum:
        return QStringLiteral("Durum");
    case ColToplam:
        return QStringLiteral("Genel Toplam");
    default:
        return {};
    }
}

void QuoteSummaryModel::setQuotes(const QVector<QuoteSummary> &quotes)
{
    beginResetModel();
    m_quotes = quotes;
    endResetModel();
}

QuoteSummary QuoteSummaryModel::at(int row) const
{
    if (row < 0 || row >= m_quotes.size())
        return {};
    return m_quotes.at(row);
}
