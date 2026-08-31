#pragma once

#include "core/models.h"

#include <QAbstractTableModel>
#include <QVector>

// Arşiv ekranındaki teklif listesinin modeli.
//
// Salt okunurdur: arşivde hiçbir hücre düzenlenmez. Durum değiştirme ve
// kopyalama ayrı komutlardır (tabloda yanlışlıkla yapılan bir düzenlemenin
// kayıtlı bir belgeyi değiştirmesi istenmez).
class QuoteSummaryModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { ColTeklifNo = 0, ColTarih, ColMusteri, ColDurum, ColToplam, ColCount };

    explicit QuoteSummaryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void setQuotes(const QVector<QuoteSummary> &quotes);

    // Satır numarasına karşılık gelen özet. Geçersiz satırda boş QuoteSummary
    // döner, böylece çağıran tarafın ayrıca sınır kontrolü yapması gerekmez.
    QuoteSummary at(int row) const;

private:
    QVector<QuoteSummary> m_quotes;
};
