#include "quote_line_model.h"

#include "core/calculator.h"
#include "core/numparse.h"

namespace {

// Miktarı Türkçe biçimde ("0,333", "24") gösterir: en fazla 3 ondalık
// basamak, gereksiz kuyruk sıfırları atılır. Money::toString()'ten farklı
// olarak sabit 2 hane DEĞİL — miktar kuruş gibi kesin bir alt birime sahip
// değil (0,333 m² gibi kesirli birimler olabilir).
QString formatMiktar(double m)
{
    QString s = QString::number(m, 'f', 3);
    while (s.endsWith(QLatin1Char('0')))
        s.chop(1);
    if (s.endsWith(QLatin1Char('.')))
        s.chop(1);
    s.replace(QLatin1Char('.'), QLatin1Char(','));
    return s;
}

} // namespace

QuoteLineModel::QuoteLineModel(QObject *parent) : QAbstractTableModel(parent) {}

int QuoteLineModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_lines.size();
}

int QuoteLineModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(ColCount);
}

QVariant QuoteLineModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_lines.size())
        return {};

    const QuoteLine &l = m_lines.at(index.row());

    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case ColSira:
        case ColMiktar:
        case ColBirimFiyat:
        case ColTutar:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    if (role != Qt::DisplayRole && role != Qt::EditRole)
        return {};

    switch (index.column()) {
    case ColSira:
        return l.sira;
    case ColAciklama:
        return l.aciklama;
    case ColBirim:
        return l.birim;
    case ColMiktar:
        return formatMiktar(l.miktar);
    case ColBirimFiyat:
        return l.birimFiyat.toString();
    case ColTutar:
        return l.tutar.toString();
    default:
        return {};
    }
}

bool QuoteLineModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole || !index.isValid() || index.row() < 0 || index.row() >= m_lines.size())
        return false;

    switch (index.column()) {
    case ColAciklama: {
        const QString metin = value.toString().trimmed();
        if (metin.isEmpty())
            return false; // boş açıklamayla satır anlamsız, reddedilir
        m_lines[index.row()].aciklama = metin;
        emit dataChanged(index, index);
        return true;
    }
    case ColMiktar: {
        const auto miktar = parseTurkishNumber(value.toString());
        if (!miktar.has_value() || miktar.value() <= 0.0)
            return false;
        m_lines[index.row()].miktar = miktar.value();
        recomputeTutar(index.row());
        emit dataChanged(this->index(index.row(), ColMiktar), this->index(index.row(), ColTutar));
        emit totalsMayHaveChanged();
        return true;
    }
    case ColBirimFiyat: {
        const auto fiyat = Money::fromString(value.toString());
        if (!fiyat.has_value() || fiyat->isNegative())
            return false;
        m_lines[index.row()].birimFiyat = fiyat.value();
        recomputeTutar(index.row());
        emit dataChanged(this->index(index.row(), ColBirimFiyat), this->index(index.row(), ColTutar));
        emit totalsMayHaveChanged();
        return true;
    }
    default:
        return false; // Sira, Birim, Tutar salt okunur
    }
}

QVariant QuoteLineModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);

    switch (section) {
    case ColSira:
        return QStringLiteral("#");
    case ColAciklama:
        return QStringLiteral("Açıklama");
    case ColBirim:
        return QStringLiteral("Birim");
    case ColMiktar:
        return QStringLiteral("Miktar");
    case ColBirimFiyat:
        return QStringLiteral("B. Fiyat");
    case ColTutar:
        return QStringLiteral("Tutar");
    default:
        return {};
    }
}

Qt::ItemFlags QuoteLineModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    switch (index.column()) {
    case ColAciklama:
    case ColMiktar:
    case ColBirimFiyat:
        f |= Qt::ItemIsEditable;
        break;
    default:
        break; // ColSira, ColBirim, ColTutar salt okunur
    }
    return f;
}

void QuoteLineModel::addLine(const Item &kaynak, double miktar, Money birimFiyat, const QString &satirNotu)
{
    QuoteLine l;
    l.aciklama = kaynak.ad;    // katalogdan KOPYALANIR
    l.birim = kaynak.birim;    // katalogdan KOPYALANIR
    l.miktar = miktar;
    l.birimFiyat = birimFiyat; // popup'ta değişmiş olabilir; katalog referansı YOK
    l.satirNotu = satirNotu;
    l.tutar = Calculator::lineTotal(miktar, birimFiyat);

    const int row = m_lines.size();
    beginInsertRows(QModelIndex(), row, row);
    m_lines.append(l);
    renumber();
    endInsertRows();
    emit totalsMayHaveChanged();
}

void QuoteLineModel::removeLine(int row)
{
    if (row < 0 || row >= m_lines.size())
        return;

    beginRemoveRows(QModelIndex(), row, row);
    m_lines.removeAt(row);
    endRemoveRows();

    renumber();
    if (!m_lines.isEmpty())
        emit dataChanged(index(0, ColSira), index(m_lines.size() - 1, ColSira));

    emit totalsMayHaveChanged();
}

void QuoteLineModel::setLines(const QVector<QuoteLine> &lines)
{
    beginResetModel();
    m_lines = lines;
    renumber();
    endResetModel();
    emit totalsMayHaveChanged();
}

void QuoteLineModel::clear()
{
    setLines({});
}

void QuoteLineModel::renumber()
{
    for (int i = 0; i < m_lines.size(); ++i)
        m_lines[i].sira = i + 1;
}

void QuoteLineModel::recomputeTutar(int row)
{
    QuoteLine &l = m_lines[row];
    l.tutar = Calculator::lineTotal(l.miktar, l.birimFiyat);
}
