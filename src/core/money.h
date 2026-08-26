#pragma once

#include <QString>
#include <QtGlobal>
#include <optional>

// Para tutarlarını KURUŞ cinsinden qint64 olarak tutar.
//
// Neden double değil: teklif kalemlerini kuruş bazlı tam sayı yerine double TL
// olarak toplarsan, birden fazla satırın toplamında 1 kuruşluk sapmalar birikir
// (örn. üç satır 33,33 TL'lik kalem, double ile toplanınca 99,98 veya 100,00
// çıkabilir; doğrusu 99,99'dur). Kuruş bazlı tam sayı aritmetiği bu sınıfın
// dışına asla double olarak sızmaz.
class Money
{
public:
    constexpr Money() noexcept = default;
    explicit constexpr Money(qint64 kurus) noexcept : m_kurus(kurus) {}

    // "1.234,56", "1234,56", "1234", "1234,5" gibi Türkçe biçimli metni ayrıştırır.
    // Nokta binlik ayraç, virgül ondalık ayraçtır. Binlik ayraç kullanılıyorsa
    // gruplama doğru olmalıdır (örn. "12.34.56" geçersizdir). Ayrıştırılamayan
    // girdide std::nullopt döner — istisna fırlatmaz, çökmez.
    static std::optional<Money> fromString(const QString &metin);

    // Ondalık bir TL tutarını (örn. miktar × birim fiyat çarpımından gelen ara
    // sonucu) en yakın kuruşa yuvarlayarak Money'ye çevirir.
    static Money fromDouble(double tl) noexcept;

    qint64 kurus() const noexcept { return m_kurus; }

    // "1.234,56" biçiminde, binlik ayraçlı, iki ondalık haneli gösterim.
    QString toString() const;

    constexpr Money operator+(const Money &o) const noexcept { return Money(m_kurus + o.m_kurus); }
    constexpr Money operator-(const Money &o) const noexcept { return Money(m_kurus - o.m_kurus); }
    constexpr Money operator*(qint64 kat) const noexcept { return Money(m_kurus * kat); }
    Money operator*(double kat) const noexcept;
    // Kalan kuruş en yakın tam sayıya yuvarlanır (bankacı yuvarlaması değil,
    // klasik yuvarlama — taksit bölme gibi az sayıda kullanım için yeterli).
    Money operator/(qint64 payda) const noexcept;

    Money &operator+=(const Money &o) noexcept { m_kurus += o.m_kurus; return *this; }
    Money &operator-=(const Money &o) noexcept { m_kurus -= o.m_kurus; return *this; }

    constexpr bool operator==(const Money &o) const noexcept { return m_kurus == o.m_kurus; }
    constexpr bool operator!=(const Money &o) const noexcept { return m_kurus != o.m_kurus; }
    constexpr bool operator<(const Money &o) const noexcept { return m_kurus < o.m_kurus; }
    constexpr bool operator<=(const Money &o) const noexcept { return m_kurus <= o.m_kurus; }
    constexpr bool operator>(const Money &o) const noexcept { return m_kurus > o.m_kurus; }
    constexpr bool operator>=(const Money &o) const noexcept { return m_kurus >= o.m_kurus; }

    constexpr bool isZero() const noexcept { return m_kurus == 0; }
    constexpr bool isNegative() const noexcept { return m_kurus < 0; }

private:
    qint64 m_kurus = 0;
};
