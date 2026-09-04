#include "teklif/ui/quote_line_model.h"

#include "teklif/core/calculator.h"
#include "teklif/core/numparse.h"

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
    case ColNot:
        return l.satirNotu;
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
        // Boş bırakılabilir: elle açılan satır boş başlar ve kullanıcı
        // yazarken hücreyi temizleyebilmelidir. Boş satırlar kaydedilirken
        // ayıklanır (bkz. filledLines).
        m_lines[index.row()].aciklama = value.toString().trimmed();
        emit dataChanged(index, index);
        return true;
    }
    case ColBirim: {
        m_lines[index.row()].birim = value.toString().trimmed();
        emit dataChanged(index, index);
        return true;
    }
    case ColNot: {
        m_lines[index.row()].satirNotu = value.toString().trimmed();
        emit dataChanged(index, index);
        return true;
    }
    case ColMiktar: {
        const auto miktar = parseTurkishNumber(value.toString());
        // Negatif miktar anlamsız; SIFIR ise kabul edilir — tutarı olmayan
        // bir başlık/ara açıklama satırı yazmanın tek yolu budur.
        if (!miktar.has_value() || miktar.value() < 0.0)
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
        return false; // Sira ve Tutar salt okunur
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
    case ColNot:
        return QStringLiteral("Not");
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
    case ColBirim:
    case ColMiktar:
    case ColBirimFiyat:
    case ColNot:
        f |= Qt::ItemIsEditable;
        break;
    default:
        break; // ColSira ve ColTutar hesaplanır, elle yazılamaz
    }
    return f;
}

namespace {
// Elle açılan satırın başlangıç miktarı.
constexpr double kVarsayilanMiktar = 1.0;
} // namespace

int QuoteLineModel::addLine(const Item &kaynak, double miktar, Money birimFiyat,
                             const QString &satirNotu)
{
    QuoteLine l;
    l.aciklama = kaynak.ad;    // katalogdan KOPYALANIR
    l.birim = kaynak.birim;    // katalogdan KOPYALANIR
    l.miktar = miktar;
    l.birimFiyat = birimFiyat; // katalog referansı YOK, değer kopyalanır
    l.satirNotu = satirNotu;
    l.tutar = Calculator::lineTotal(miktar, birimFiyat);

    const int row = m_lines.size();
    beginInsertRows(QModelIndex(), row, row);
    m_lines.append(l);
    renumber();
    endInsertRows();
    emit totalsMayHaveChanged();
    return row;
}

int QuoteLineModel::addEmptyLine()
{
    QuoteLine l;
    // Miktar 1 ile başlar: elle girilen satırların neredeyse tamamı tek
    // kalemdir, kullanıcı yalnızca açıklama ve fiyat yazarak bitirebilsin.
    l.miktar = kVarsayilanMiktar;

    const int row = m_lines.size();
    beginInsertRows(QModelIndex(), row, row);
    m_lines.append(l);
    renumber();
    endInsertRows();
    emit totalsMayHaveChanged();
    return row;
}

QVector<QuoteLine> QuoteLineModel::filledLines() const
{
    QVector<QuoteLine> dolu;
    dolu.reserve(m_lines.size());
    for (const QuoteLine &l : m_lines) {
        if (!l.aciklama.trimmed().isEmpty())
            dolu.append(l);
    }
    // Ayıklamadan sonra numaralar boşluksuz olmalı: belgede "1, 2, 4" diye
    // giden bir sıra, silinmiş bir satır varmış izlenimi verirdi.
    for (int i = 0; i < dolu.size(); ++i)
        dolu[i].sira = i + 1;
    return dolu;
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
