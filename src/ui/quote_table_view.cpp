#include "quote_table_view.h"

QuoteTableView::QuoteTableView(QWidget *parent) : QTableView(parent) {}

QModelIndex QuoteTableView::moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers)
{
    if ((cursorAction != MoveNext && cursorAction != MovePrevious) || !model())
        return QTableView::moveCursor(cursorAction, modifiers);

    const QModelIndex mevcut = currentIndex();
    if (!mevcut.isValid())
        return QTableView::moveCursor(cursorAction, modifiers);

    const int satirSayisi = model()->rowCount();
    const int sutunSayisi = model()->columnCount();
    if (satirSayisi == 0 || sutunSayisi == 0)
        return mevcut;

    const int yon = (cursorAction == MoveNext) ? 1 : -1;
    int satir = mevcut.row();
    int sutun = mevcut.column();

    // En kötü durumda tüm hücreleri bir tur dolaşır; hiçbiri düzenlenebilir
    // değilse mevcut hücrede kalır (sonsuz döngüye girmez).
    for (int adim = 0; adim < satirSayisi * sutunSayisi; ++adim) {
        sutun += yon;
        if (sutun >= sutunSayisi) {
            sutun = 0;
            satir = (satir + 1) % satirSayisi;
        } else if (sutun < 0) {
            sutun = sutunSayisi - 1;
            satir = (satir - 1 + satirSayisi) % satirSayisi;
        }

        const QModelIndex aday = model()->index(satir, sutun);
        if (model()->flags(aday) & Qt::ItemIsEditable)
            return aday;
    }
    return mevcut;
}

void QuoteTableView::closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint)
{
    if (hint == QAbstractItemDelegate::EditNextItem || hint == QAbstractItemDelegate::EditPreviousItem) {
        // Qt'nin varsayılan "sıradaki hücreye geç" mantığı salt okunur
        // sütunları atlamaz; önce sadece kapat, sonra kendi moveCursor()
        // mantığımızla doğru hücreye geç.
        QTableView::closeEditor(editor, QAbstractItemDelegate::NoHint);
        const QModelIndex sonraki = moveCursor(
            hint == QAbstractItemDelegate::EditNextItem ? MoveNext : MovePrevious, Qt::NoModifier);
        setCurrentIndex(sonraki);
        edit(sonraki);
        return;
    }
    QTableView::closeEditor(editor, hint);
}
