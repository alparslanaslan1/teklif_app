#pragma once

#include "core/models.h"
#include "core/money.h"

#include <QDialog>

class QLineEdit;
class QLabel;

// Katalogdan bir kalem seçildiğinde açılan popup: miktar + birim fiyat +
// satır notu. Fiyat katalogdan ön dolu gelir ama değiştirilebilir — Kaydet
// edildiğinde bu değer teklife KOPYALANIR, kataloğa asla yazılmaz.
//
// Doğrulama: miktar sıfırdan büyük olmalı, fiyat negatif olamaz. Geçersiz
// girdide accept() çağrılmaz, dialog açık kalır ve bir hata mesajı gösterir.
// Esc her zaman Qt'nin varsayılan davranışıyla reject() tetikler — ekstra
// kod gerekmez.
class LineEntryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LineEntryDialog(const Item &kaynak, QWidget *parent = nullptr);

    double miktar() const { return m_miktar; }
    Money birimFiyat() const { return m_birimFiyat; }
    QString satirNotu() const;

private slots:
    void validateAndMaybeAccept();
    void updatePreview();

private:
    Item m_kaynak;
    double m_miktar = 0.0;
    Money m_birimFiyat;

    QLineEdit *m_miktarEdit;
    QLineEdit *m_fiyatEdit;
    QLineEdit *m_notEdit;
    QLabel *m_previewLabel;
    QLabel *m_hataLabel;
};
