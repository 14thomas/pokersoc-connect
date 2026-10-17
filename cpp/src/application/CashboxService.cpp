#include "application/CashboxService.h"

#include "domain/cashbox/Cashbox.h"
#include "domain/money/DenominationCount.h"
#include "persistence/AppStore.h"

namespace pokersoc::application {

void CashboxService::addFloat(const domain::DenominationCount& amount) {
    if (amount.empty()) {
        return;
    }
    store_.recordFloat(amount); // persist first (crash-safe)
    cashbox_.applyFloat(amount);
}

} // namespace pokersoc::application
