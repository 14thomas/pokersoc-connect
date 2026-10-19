#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "domain/food/SaleLine.h"
#include "domain/money/DenominationCount.h"
#include "domain/money/Money.h"
#include "domain/transaction/Transaction.h"

namespace pokersoc::domain {

class Cashbox;

enum class PaymentMethod { Cash, Chips };

inline const char* paymentMethodName(PaymentMethod method) noexcept {
    return method == PaymentMethod::Chips ? "CHIP" : "CASH";
}

inline PaymentMethod paymentMethodFromName(std::string_view name) noexcept {
    return name == "CHIP" ? PaymentMethod::Chips : PaymentMethod::Cash;
}

/// A food purchase from the catalog. Cash payments go into the cashbox (with
/// change given back from it); chip payments move no cash, since chips are not
/// held in the cashbox. The player is optional (anonymous sales are allowed).
class FoodSale final : public Transaction {
public:
    FoodSale(std::string playerId,
             std::vector<SaleLine> lines,
             PaymentMethod method,
             DenominationCount payment,
             DenominationCount change);

    TransactionType type() const noexcept override { return TransactionType::FoodSale; }
    Money amount() const noexcept override { return saleLinesTotal(lines_); }

    void apply(Cashbox& cashbox) const override;
    void reverse(Cashbox& cashbox) const override;
    std::string describe() const override;

    const std::vector<SaleLine>& lines() const noexcept { return lines_; }
    PaymentMethod method() const noexcept { return method_; }
    const DenominationCount& payment() const noexcept { return payment_; }
    const DenominationCount& change() const noexcept { return change_; }

private:
    std::vector<SaleLine> lines_;
    PaymentMethod method_;
    DenominationCount payment_;
    DenominationCount change_;
};

} // namespace pokersoc::domain
