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

    const bool ileri = (cursorAction == MoveNext);
    int satir = mevcut.row();
    int sutun = mevcut.column();

    // Sütun sınırını her aştığımızda satır bir ilerler/geriler; satır da
    // tablonun dışına çıkınca döngü biter. Bu yüzden sonsuz döngü olamaz,
    // hiçbir hücre düzenlenebilir olmasa bile.
    while (true) {
        sutun += ileri ? 1 : -1;
        if (sutun >= sutunSayisi) {
            sutun = 0;
            ++satir;
        } else if (sutun < 0) {
            sutun = sutunSayisi - 1;
            --satir;
        }

        // TABLONUN DIŞINA ÇIKTIK — SARMA YOK.
        //
        // Eskiden satır numarası % ile döngüsel dolaşılıyordu: son hücrede
        // Tab'a basınca imleç ilk satıra sarıyordu ve odak tabloyu ASLA terk
        // edemiyordu. Kullanıcı klavyeyle Kaydet düğmesine ulaşamıyordu —
        // "fareye dokunmadan teklif gir" hedefiyle doğrudan çelişiyordu.
        //
        // Artık odak bir sonraki/önceki widget'a devredilir; Qt'nin her
        // yerdeki Tab davranışıyla aynı.
        //
        // BURADAN focusNextPrevChild() ÇAĞIRMAYIN: Qt'nin kendi
        // QAbstractItemView::focusNextPrevChild'ı moveCursor'ı çağırır,
        // yani sonsuz özyineleme olur (bir testte yığın taşmasıyla
        // yakalandı). Bunun yerine imleci DEĞİŞTİRMEDEN döneriz; Qt
        // "yeni indeks == mevcut indeks" görür ve odağı kendisi
        // QAbstractScrollArea'ya devredip bir sonraki widget'a taşır.
        if (satir < 0 || satir >= satirSayisi)
            return mevcut;

        const QModelIndex aday = model()->index(satir, sutun);
        if (model()->flags(aday) & Qt::ItemIsEditable)
            return aday;
    }
}

void QuoteTableView::closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint)
{
    if (hint == QAbstractItemDelegate::EditNextItem || hint == QAbstractItemDelegate::EditPreviousItem) {
        // Qt'nin varsayılan "sıradaki hücreye geç" mantığı salt okunur
        // sütunları atlamaz; önce sadece kapat, sonra kendi moveCursor()
        // mantığımızla doğru hücreye geç.
        const QModelIndex mevcut = currentIndex();
        QTableView::closeEditor(editor, QAbstractItemDelegate::NoHint);

        const QModelIndex sonraki = moveCursor(
            hint == QAbstractItemDelegate::EditNextItem ? MoveNext : MovePrevious, Qt::NoModifier);

        // sonraki == mevcut ise tablonun sınırına gelinmiş ve odak zaten
        // dışarı devredilmiştir; aynı hücrenin düzenleyicisini yeniden
        // açmak kullanıcıyı hücreye geri hapsederdi.
        if (sonraki.isValid() && sonraki != mevcut) {
            setCurrentIndex(sonraki);
            edit(sonraki);
        } else {
            // Sınıra gelindi. Düzenleme yolunda odağı Qt kendiliğinden
            // taşımaz (Tab'ı delegate yuttu), bu yüzden elle devredilir.
            // moveCursor'ın aksine BURADAN çağırmak güvenli: closeEditor'ı
            // focusNextPrevChild çağırmaz, özyineleme oluşmaz.
            focusNextPrevChild(hint == QAbstractItemDelegate::EditNextItem);
        }
        return;
    }
    QTableView::closeEditor(editor, hint);
}
