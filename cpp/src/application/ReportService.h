#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "domain/money/Money.h"

namespace pokersoc::domain {
class Attendance;
class Cashbox;
class Transaction;
class TransactionLog;
} // namespace pokersoc::domain

namespace pokersoc::application {

/// Read-only views over the session: totals for the cashbox screen, tournament
/// entry counts, and per-player activity.
class ReportService {
public:
    struct Summary {
        domain::Money cashInBox;
        domain::Money floatTotal;
        domain::Money tipsTotal;        // cash tips + lost chips + cash-out tips
        domain::Money foodSalesTotal;   // counter sales + food bought at cash-out
        domain::Money buyInTotal;
        int buyIns = 0;
        int rebuys = 0;
        domain::Money cashOutTotal;     // cash paid to players
        int cashOuts = 0;
        int exchanges = 0;
        std::size_t uniquePlayers = 0;  // distinct players scanned in
        std::size_t scans = 0;
    };

    struct TierCount {
        std::string tier;
        int buyIns = 0;
        int rebuys = 0;
    };

    struct TournamentStats {
        int buyIns = 0;
        int rebuys = 0;
        domain::Money collected;
        std::vector<TierCount> tiers; // only tiers with entries, in display order
    };

    struct PlayerTotals {
        int buyIns = 0;
        domain::Money boughtIn;
        int cashOuts = 0;
        domain::Money paidOut;
        domain::Money net() const noexcept { return paidOut - boughtIn; }
    };

    ReportService(const domain::Cashbox& cashbox,
                  const domain::TransactionLog& log,
                  const domain::Attendance& attendance)
        : cashbox_(cashbox), log_(log), attendance_(attendance) {}

    Summary summary() const;
    TournamentStats tournamentStats() const;

    /// Transactions newest-first, optionally only those for one player.
    std::vector<const domain::Transaction*> activity(const std::string& playerId = "") const;
    PlayerTotals playerTotals(const std::string& playerId) const;

private:
    const domain::Cashbox& cashbox_;
    const domain::TransactionLog& log_;
    const domain::Attendance& attendance_;
};

} // namespace pokersoc::application
