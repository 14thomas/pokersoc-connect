#pragma once

#include <string>

#include "domain/money/DenominationCount.h"
#include "domain/money/Money.h"
#include "domain/transaction/Transaction.h"

namespace pokersoc::domain {

class Cashbox;

/// Staff add cash to the float.
class FloatAdd final : public Transaction {
public:
    explicit FloatAdd(DenominationCount cash);

    TransactionType type() const noexcept override { return TransactionType::FloatAdd; }
    Money amount() const noexcept override { return cash_.total(); }

    void apply(Cashbox& cashbox) const override;
    void reverse(Cashbox& cashbox) const override;
    std::string describe() const override;

    const DenominationCount& cash() const noexcept { return cash_; }

private:
    DenominationCount cash_;
};

/// A cash tip handed to the house; the cash goes into the cashbox.
class CashTip final : public Transaction {
public:
    explicit CashTip(DenominationCount cash, std::string playerId = "");

    TransactionType type() const noexcept override { return TransactionType::CashTip; }
    Money amount() const noexcept override { return cash_.total(); }

    void apply(Cashbox& cashbox) const override;
    void reverse(Cashbox& cashbox) const override;
    std::string describe() const override;

    const DenominationCount& cash() const noexcept { return cash_; }

private:
    DenominationCount cash_;
};

/// Chips found on the floor / left behind, kept by the house as a tip. The
/// cashbox holds currency only, so this records the value but moves no cash.
class LostChips final : public Transaction {
public:
    explicit LostChips(DenominationCount chips);

    TransactionType type() const noexcept override { return TransactionType::LostChips; }
    Money amount() const noexcept override { return chips_.total(); }

    void apply(Cashbox&) const override {}
    void reverse(Cashbox&) const override {}
    std::string describe() const override;

    const DenominationCount& chips() const noexcept { return chips_; }

private:
    DenominationCount chips_;
};

/// Like-for-like swap of currency, e.g. a $50 note given out for five $10s.
/// `given` leaves the cashbox, `received` comes in; their totals are equal.
class Exchange final : public Transaction {
public:
    Exchange(DenominationCount given, DenominationCount received);

    TransactionType type() const noexcept override { return TransactionType::Exchange; }
    Money amount() const noexcept override { return given_.total(); }

    void apply(Cashbox& cashbox) const override;
    void reverse(Cashbox& cashbox) const override;
    std::string describe() const override;

    const DenominationCount& given() const noexcept { return given_; }
    const DenominationCount& received() const noexcept { return received_; }

private:
    DenominationCount given_;
    DenominationCount received_;
};

/// "2 x $50, 1 x 20c" style summary of a tally.
std::string describeCount(const DenominationCount& count);

} // namespace pokersoc::domain
