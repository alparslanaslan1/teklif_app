#include "teklif/ui/dlg_line_entry.h"

#include "teklif/core/calculator.h"
#include "teklif/core/numparse.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

LineEntryDialog::LineEntryDialog(const Item &kaynak, QWidget *parent) : QDialog(parent), m_kaynak(kaynak)
{
    setWindowTitle(kaynak.ad);

    m_miktarEdit = new QLineEdit(this);
    m_miktarEdit->setObjectName(QStringLiteral("miktarEdit"));
    m_miktarEdit->setText(QStringLiteral("1"));

    m_fiyatEdit = new QLineEdit(this);
    m_fiyatEdit->setObjectName(QStringLiteral("fiyatEdit"));
    m_fiyatEdit->setText(kaynak.varsayilanFiyat.toString());

    m_notEdit = new QLineEdit(this);
    m_notEdit->setObjectName(QStringLiteral("notEdit"));
    m_notEdit->setPlaceholderText(QStringLiteral("isteğe bağlı"));

    m_previewLabel = new QLabel(this);
    m_previewLabel->setObjectName(QStringLiteral("previewLabel"));

    m_hataLabel = new QLabel(this);
    m_hataLabel->setObjectName(QStringLiteral("hataLabel"));
    m_hataLabel->setStyleSheet(QStringLiteral("color: #A4402F;"));
    m_hataLabel->setVisible(false);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Miktar (%1)").arg(kaynak.birim), m_miktarEdit);
    form->addRow(QStringLiteral("Birim fiyat ₺"), m_fiyatEdit);
    form->addRow(QStringLiteral("Satır notu"), m_notEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setObjectName(QStringLiteral("buttonBox"));
    connect(buttons, &QDialogButtonBox::accepted, this, &LineEntryDialog::validateAndMaybeAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *lay = new QVBoxLayout(this);
    lay->addLayout(form);
    lay->addWidget(m_previewLabel);
    lay->addWidget(m_hataLabel);
    lay->addWidget(buttons);

    connect(m_miktarEdit, &QLineEdit::textChanged, this, &LineEntryDialog::updatePreview);
    connect(m_fiyatEdit, &QLineEdit::textChanged, this, &LineEntryDialog::updatePreview);
    updatePreview();

    m_miktarEdit->setFocus();
    m_miktarEdit->selectAll();
}

void LineEntryDialog::updatePreview()
{
    const auto miktar = parseTurkishNumber(m_miktarEdit->text());
    const auto fiyat = Money::fromString(m_fiyatEdit->text());
    if (miktar.has_value() && fiyat.has_value()) {
        m_previewLabel->setText(
            QStringLiteral("Satır tutarı: %1").arg(Calculator::lineTotal(miktar.value(), fiyat.value()).toString()));
    } else {
        m_previewLabel->setText(QStringLiteral("Satır tutarı: —"));
    }
    m_hataLabel->setVisible(false);
}

void LineEntryDialog::validateAndMaybeAccept()
{
    const auto miktarOpt = parseTurkishNumber(m_miktarEdit->text());
    const auto fiyatOpt = Money::fromString(m_fiyatEdit->text());

    if (!miktarOpt.has_value() || miktarOpt.value() <= 0.0) {
        m_hataLabel->setText(QStringLiteral("Miktar sıfırdan büyük bir sayı olmalı."));
        m_hataLabel->setVisible(true);
        return;
    }
    if (!fiyatOpt.has_value() || fiyatOpt->isNegative()) {
        m_hataLabel->setText(QStringLiteral("Birim fiyat negatif olamaz."));
        m_hataLabel->setVisible(true);
        return;
    }

    m_miktar = miktarOpt.value();
    m_birimFiyat = fiyatOpt.value();
    accept();
}

QString LineEntryDialog::satirNotu() const
{
    return m_notEdit->text().trimmed();
}
