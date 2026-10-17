#pragma once

#include <string>
#include <vector>

#include "application/Ledger.h"
#include "domain/food/Product.h"
#include "domain/food/SaleLine.h"
#include "domain/money/DenominationCount.h"
#include "domain/money/Money.h"

namespace pokersoc::domain {
class DenominationTable;
class ProductCatalog;
class SessionMode;
} // namespace pokersoc::domain

namespace pokersoc::persistence {
class AppStore;
}

namespace pokersoc::application {

/// Use case: the food & drink counter. Maintains the product catalog (and its
/// quick slots) and sells items for cash or, in cash games, for chips.
class FoodService {
public:
    struct SaleResult {
        bool success = false;
        std::string message;
        const domain::Transaction* transaction = nullptr;
        domain::DenominationCount change;
    };

    FoodService(domain::ProductCatalog& catalog,
                const domain::DenominationTable& table,
                domain::SessionMode& mode,
                Ledger& ledger,
                persistence::AppStore& store)
        : catalog_(catalog), table_(table), mode_(mode), ledger_(ledger), store_(store) {}

    // ---- catalog ----
    const domain::ProductCatalog& catalog() const noexcept { return catalog_; }
    /// Add a new product, or update the one with the same id if `product.id` != 0.
    const domain::Product& saveProduct(domain::Product product);
    bool removeProduct(long long id);
    /// Remove every product (used by catalog "replace" imports).
    void clearCatalog();
    /// Pin a product to quick slot 1..6 (productId 0 clears it).
    void setSlot(int slot, long long productId);

    // ---- sales ----
    bool chipPaymentAllowed() const noexcept;

    SaleResult sellForCash(const std::string& playerId,
                           const std::vector<domain::SaleLine>& lines,
                           const domain::DenominationCount& payment);
    SaleResult sellForChips(const std::string& playerId,
                            const std::vector<domain::SaleLine>& lines,
                            const domain::DenominationCount& chips);

private:
    domain::ProductCatalog& catalog_;
    const domain::DenominationTable& table_;
    domain::SessionMode& mode_;
    Ledger& ledger_;
    persistence::AppStore& store_;
};

} // namespace pokersoc::application
