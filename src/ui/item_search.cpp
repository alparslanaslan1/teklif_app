#include "teklif/ui/item_search.h"
#include "teklif/core/search.h"

#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

ItemSearch::ItemSearch(QWidget *parent) : QWidget(parent)
{
    m_edit = new QLineEdit(this);
    m_edit->setObjectName(QStringLiteral("itemSearchEdit"));
    m_edit->setPlaceholderText(QStringLiteral("Malzeme veya hizmet ara..."));
    m_edit->setClearButtonEnabled(true);

    m_popup = new QListWidget(this);
    m_popup->setObjectName(QStringLiteral("itemSearchPopup"));
    m_popup->setVisible(false);
    // Liste kendi başına odak ALMAZ: ok tuşları her zaman arama kutusunda
    // yakalanır (eventFilter), fareyle tıklama zaten itemActivated verir.
    m_popup->setFocusPolicy(Qt::NoFocus);
    // QListWidget'ın varsayılan dikey genişleme eğilimini kısıtla: 1-2
    // sonuçta bile sayfanın yarısını kaplayan boş bir kutu istemiyoruz —
    // en fazla ~6 satır kadar yükseklik, kalanı içerik belirler.
    m_popup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    m_popup->setMaximumHeight(180);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_edit);
    lay->addWidget(m_popup);

    setFocusProxy(m_edit);

    connect(m_edit, &QLineEdit::textChanged, this, &ItemSearch::onTextChanged);
    connect(m_popup, &QListWidget::itemActivated, this, [this](QListWidgetItem *) { chooseCurrent(); });
    m_edit->installEventFilter(this);
}

void ItemSearch::setCatalog(const QVector<Item> &katalog)
{
    m_index.setCatalog(katalog);
}

void ItemSearch::focusInput()
{
    m_edit->setFocus();
}

void ItemSearch::onTextChanged(const QString &text)
{
    m_sonuc = m_index.search(text, kMaxSonuc);
    // showResults'a m_sonuc'un KOPYASI degil kendisi gecerse, bos sonucta
    // cagrilan hideResults() onu temizlerken parametreyi de bosaltir.
    // Erken donuldugu icin bugun zararsiz ama kirilgan; kopya gecmek acik.
    const QVector<Item> gosterilecek = m_sonuc;
    showResults(gosterilecek);
}

void ItemSearch::showResults(const QVector<Item> &sonuc)
{
    m_popup->clear();
    if (sonuc.isEmpty()) {
        hideResults();
        return;
    }
    for (const Item &it : sonuc) {
        auto *li = new QListWidgetItem(
            QStringLiteral("%1   (%2)   %3").arg(it.ad, it.birim, it.varsayilanFiyat.toString()));
        m_popup->addItem(li);
    }
    m_popup->setCurrentRow(0);

    // İçeriğe göre yükseklik: 1 sonuçta minik bir kutu, 6+ sonuçta ~6
    // satırlık bir kutu (sonrası kaydırılır) — sabit bir maksimumda
    // donup kalan boş bir dikdörtgen yerine.
    const int gorunecekSatir = qMin(sonuc.size(), 6);
    const int satirYuksekligi = m_popup->sizeHintForRow(0);
    if (satirYuksekligi > 0)
        m_popup->setFixedHeight(gorunecekSatir * satirYuksekligi + 2 * m_popup->frameWidth());

    m_popup->setVisible(true);
    m_sonucAcik = true;
}

void ItemSearch::hideResults()
{
    m_popup->setVisible(false);
    m_popup->clear();
    m_sonuc.clear();
    m_sonucAcik = false;
}

void ItemSearch::moveSelection(int delta)
{
    if (m_sonuc.isEmpty())
        return;
    const int row = qBound(0, m_popup->currentRow() + delta, m_sonuc.size() - 1);
    m_popup->setCurrentRow(row);
}

void ItemSearch::chooseCurrent()
{
    const int row = m_popup->currentRow();
    if (row < 0 || row >= m_sonuc.size())
        return;

    const Item secilen = m_sonuc.at(row);
    hideResults();
    m_edit->clear();
    emit itemChosen(secilen);
}

bool ItemSearch::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_edit && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (m_sonucAcik) {
            switch (ke->key()) {
            case Qt::Key_Down:
                moveSelection(1);
                return true;
            case Qt::Key_Up:
                moveSelection(-1);
                return true;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                chooseCurrent();
                return true;
            case Qt::Key_Escape:
                hideResults();
                m_edit->clear();
                return true;
            default:
                break;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}
