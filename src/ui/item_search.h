#pragma once

#include "core/models.h"

#include <QVector>
#include <QWidget>

class QLineEdit;
class QListWidget;

// Katalogda arama kutusu: yazdıkça filtreler (core/search.h -> itemAra),
// ok tuşlarıyla gezinme, Enter ile seçim, Esc ile kapatma+temizleme.
// Seçim yapılınca itemChosen sinyali yayınlanır; kutu kendiliğinden
// temizlenir ve odak üzerinde kalır (satır ekleme sonrası odağın arama
// kutusuna dönmesi gerektiği için page_quote ekstra bir şey yapmasına
// gerek kalmaz — zaten burada oluyor).
//
// NOT: Sonuç listesi bu widget'ın içinde katlanır açılan bir alt widget
// olarak gösterilir (ayrı bir kayan/floating popup penceresi değil).
// Klavye tabanlı kullanım için işlevsel olarak birebir aynıdır; floating
// overlay görünümü ileride bir cila (polish) adımı olarak eklenebilir.
class ItemSearch : public QWidget
{
    Q_OBJECT

public:
    explicit ItemSearch(QWidget *parent = nullptr);

    void setCatalog(const QVector<Item> &katalog);
    void focusInput();

signals:
    void itemChosen(const Item &item);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onTextChanged(const QString &text);
    void chooseCurrent();

private:
    QLineEdit *m_edit;
    QListWidget *m_popup;
    QVector<Item> m_katalog;
    QVector<Item> m_sonuc;
    // m_popup->isVisible() GÜVENİLMEZ: bir üst pencere hiç show()
    // edilmemişse (örn. testlerde) Qt bunu her zaman false döndürür,
    // setVisible(true) çağrılmış olsa bile — çünkü gerçek ekran
    // görünürlüğü tüm ata zincirinin de görünür olmasını gerektirir.
    // Ok tuşları/Enter/Esc bu yüzden ayrı bir mantıksal bayrağa bakar.
    bool m_sonucAcik = false;

    void showResults(const QVector<Item> &sonuc);
    void hideResults();
    void moveSelection(int delta);
};
