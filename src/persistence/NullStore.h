#pragma once

#include <map>
#include <optional>
#include <string>

#include "domain/food/Product.h"
#include "domain/player/Attendance.h"
#include "domain/transaction/Transaction.h"
#include "persistence/AppStore.h"
#include "util/Time.h"

namespace pokersoc::persistence {

/// A store that writes nothing to disk. Ids, timestamps and settings are kept
/// in memory so the app behaves normally for the life of the process. Handy for
/// tests or running the app with no database file.
class NullStore final : public AppStore {
public:
    void recordTransaction(domain::Transaction& transaction) override {
        transaction.setId(++nextTransactionId_);
        transaction.setTime(util::nowUtc());
    }
    void voidTransaction(long long) override {}
    void load(const domain::DenominationTable&, domain::Cashbox&, domain::TransactionLog&) override {}

    void recordPlayer(const domain::Player&) override {}
    void loadPlayers(domain::PlayerDirectory&) override {}

    domain::AttendanceRecord recordAttendance(const std::string& playerId) override {
        return domain::AttendanceRecord{playerId, util::nowUtc()};
    }
    void loadAttendance(domain::Attendance&) override {}

    long long saveProduct(const domain::Product& product) override {
        return product.id != 0 ? product.id : ++nextProductId_;
    }
    void deleteProduct(long long) override {}
    void saveFoodSlot(int, long long) override {}
    void loadCatalog(domain::ProductCatalog&) override {}

    void setSetting(const std::string& key, const std::string& value) override {
        settings_[key] = value;
    }
    std::optional<std::string> getSetting(const std::string& key) override {
        const auto it = settings_.find(key);
        if (it == settings_.end()) return std::nullopt;
        return it->second;
    }

    void backupTo(const std::string&) override {}

private:
    long long nextTransactionId_ = 0;
    long long nextProductId_ = 0;
    std::map<std::string, std::string> settings_;
};

} // namespace pokersoc::persistence
