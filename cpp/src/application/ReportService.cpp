#include "application/ReportService.h"

#include <algorithm>
#include <cctype>

#include "domain/cashbox/Cashbox.h"
#include "domain/player/Attendance.h"
#include "domain/transaction/CashOut.h"
#include "domain/transaction/Transaction.h"
#include "domain/transaction/TransactionLog.h"

namespace pokersoc::application {

using domain::TransactionType;

namespace {

bool startsWithIgnoreCase(const std::string& text, const std::string& prefix) {
    if (text.size() < prefix.size()) {
        return false;
    }
    return std::equal(prefix.begin(), prefix.end(), text.begin(),
                      [](unsigned char a, unsigned char b) {
                          return std::tolower(a) == std::tolower(b);
                      });
}

} // namespace

ReportService::Summary ReportService::summary() const {
    Summary summary;
    summary.cashInBox = cashbox_.total();
    summary.uniquePlayers = attendance_.uniquePlayerCount();
    summary.scans = attendance_.scanCount();

    for (const auto& transaction : log_.entries()) {
        switch (transaction->type()) {
            case TransactionType::FloatAdd:
                summary.floatTotal += transaction->amount();
                break;
            case TransactionType::CashTip:
            case TransactionType::LostChips:
                summary.tipsTotal += transaction->amount();
                break;
            case TransactionType::FoodSale:
                summary.foodSalesTotal += transaction->amount();
                break;
            case TransactionType::BuyIn:
                ++summary.buyIns;
                summary.buyInTotal += transaction->amount();
                break;
            case TransactionType::Rebuy:
                ++summary.rebuys;
                summary.buyInTotal += transaction->amount();
                break;
            case TransactionType::CashOut: {
                const auto& cashOut = static_cast<const domain::CashOut&>(*transaction);
                ++summary.cashOuts;
                summary.cashOutTotal += cashOut.amount();
                summary.tipsTotal += cashOut.tip();
                summary.foodSalesTotal += cashOut.foodTotal();
                break;
            }
            case TransactionType::Exchange:
                ++summary.exchanges;
                break;
        }
    }
    return summary;
}

ReportService::TournamentStats ReportService::tournamentStats() const {
    static const char* const kTierOrder[] = {"Bronze",     "Silver", "Gold",          "Platinum",
                                             "Diamond",    "ARC Member", "Non-ARC Member"};

    TournamentStats stats;
    std::vector<TierCount> counts;
    for (const char* tier : kTierOrder) {
        counts.push_back(TierCount{tier, 0, 0});
    }
    // Longest name first, so "Non-ARC Member" is not mistaken for another tier.
    std::vector<TierCount*> byLength;
    for (auto& count : counts) byLength.push_back(&count);
    std::stable_sort(byLength.begin(), byLength.end(), [](const TierCount* a, const TierCount* b) {
        return a->tier.size() > b->tier.size();
    });

    for (const auto& transaction : log_.entries()) {
        const TransactionType type = transaction->type();
        if (type != TransactionType::BuyIn && type != TransactionType::Rebuy) {
            continue;
        }
        const bool isRebuy = type == TransactionType::Rebuy;
        (isRebuy ? stats.rebuys : stats.buyIns)++;
        stats.collected += transaction->amount();

        for (TierCount* count : byLength) {
            if (startsWithIgnoreCase(transaction->notes(), count->tier)) {
                (isRebuy ? count->rebuys : count->buyIns)++;
                break;
            }
        }
    }

    for (const auto& count : counts) {
        if (count.buyIns > 0 || count.rebuys > 0) {
            stats.tiers.push_back(count);
        }
    }
    return stats;
}

std::vector<const domain::Transaction*> ReportService::activity(const std::string& playerId) const {
    std::vector<const domain::Transaction*> result;
    const auto& entries = log_.entries();
    for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
        if (playerId.empty() || (*it)->playerId() == playerId) {
            result.push_back(it->get());
        }
    }
    return result;
}

ReportService::PlayerTotals ReportService::playerTotals(const std::string& playerId) const {
    PlayerTotals totals;
    for (const auto& transaction : log_.entries()) {
        if (transaction->playerId() != playerId) {
            continue;
        }
        const TransactionType type = transaction->type();
        if (type == TransactionType::BuyIn || type == TransactionType::Rebuy) {
            ++totals.buyIns;
            totals.boughtIn += transaction->amount();
        } else if (type == TransactionType::CashOut) {
            ++totals.cashOuts;
            totals.paidOut += transaction->amount();
        }
    }
    return totals;
}

} // namespace pokersoc::application
