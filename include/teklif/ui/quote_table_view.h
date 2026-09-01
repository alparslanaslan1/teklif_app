#pragma once

#include <QTableView>

// Sekmeyle (Tab) gezinirken salt okunur sütunları (#, Birim, Tutar) atlar;
// sadece Açıklama, Miktar, B. Fiyat arasında dolaşılır. Qt'nin varsayılan
// davranışı her hücreyi sırayla ziyaret eder — istenen davranış bu değil.
//
// İki senaryoyu da kapsar: hücre seçiliyken (düzenleme açık değilken) Tab
// basılması VE bir hücre düzenlenirken Tab ile "sıradakine geç" (Qt'nin
// delegate katmanı bunu closeEditor(EditNextItem) olarak bildirir).
class QuoteTableView : public QTableView
{
    Q_OBJECT

public:
    explicit QuoteTableView(QWidget *parent = nullptr);

protected:
    QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override;
    void closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint) override;
};
