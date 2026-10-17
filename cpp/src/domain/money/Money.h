#pragma once

#include <cstdint>
#include <string>

namespace pokersoc::domain {

class Money {
public:
    constexpr Money() noexcept = default;
    explicit constexpr Money(std::int64_t cents) noexcept : cents_(cents) {}

    static constexpr Money fromCents(std::int64_t cents) noexcept { return Money(cents); }
    static Money fromDollars(double dollars) noexcept;

    constexpr std::int64_t cents() const noexcept { return cents_; }
    constexpr double toDollars() const noexcept { return static_cast<double>(cents_) / 100.0; }

    constexpr Money operator+(Money other) const noexcept { return Money(cents_ + other.cents_); }
    constexpr Money operator-(Money other) const noexcept { return Money(cents_ - other.cents_); }
    constexpr Money operator-() const noexcept { return Money(-cents_); }
    constexpr Money operator*(std::int64_t qty) const noexcept { return Money(cents_ * qty); }

    Money& operator+=(Money other) noexcept { cents_ += other.cents_; return *this; }
    Money& operator-=(Money other) noexcept { cents_ -= other.cents_; return *this; }

    constexpr bool operator==(Money other) const noexcept { return cents_ == other.cents_; }
    constexpr bool operator!=(Money other) const noexcept { return cents_ != other.cents_; }
    constexpr bool operator<(Money other) const noexcept { return cents_ < other.cents_; }
    constexpr bool operator<=(Money other) const noexcept { return cents_ <= other.cents_; }
    constexpr bool operator>(Money other) const noexcept { return cents_ > other.cents_; }
    constexpr bool operator>=(Money other) const noexcept { return cents_ >= other.cents_; }

    constexpr bool isZero() const noexcept { return cents_ == 0; }
    constexpr bool isNegative() const noexcept { return cents_ < 0; }
    constexpr Money abs() const noexcept { return Money(cents_ < 0 ? -cents_ : cents_); }

    /// Human readable, e.g. "$12.50" or "-$3.05".
    std::string toString() const;

private:
    std::int64_t cents_ = 0;
};

constexpr Money operator*(std::int64_t qty, Money money) noexcept { return money * qty; }

} // namespace pokersoc::domain
