#include <iomanip>
#include <iostream>
#include <string>

#include "application/CashOutService.h"
#include "domain/cashbox/Cashbox.h"
#include "domain/money/CashDenomination.h"
#include "domain/money/ChipDenomination.h"
#include "domain/money/DenominationCount.h"
#include "domain/money/DenominationTable.h"
#include "domain/money/Money.h"

using namespace pokersoc::domain;
using pokersoc::application::CashOutService;

namespace {

const char* categoryName(DenominationCategory category) {
    switch (category) {
        case DenominationCategory::Note:   return "Note";
        case DenominationCategory::Coin:   return "Coin";
        case DenominationCategory::Chip:   return "Chip";
        case DenominationCategory::Plaque: return "Plaque";
    }
    return "?";
}

void demoMoney() {
    std::cout << "=== Money ===\n";
    const Money buyIn = Money::fromDollars(15.0);
    const Money tendered = Money::fromDollars(20.0);
    const Money change = tendered - buyIn;

    std::cout << "  buy-in     " << buyIn.toString() << '\n';
    std::cout << "  tendered   " << tendered.toString() << '\n';
    std::cout << "  change     " << change.toString() << '\n';
    std::cout << "  3 x buy-in " << (buyIn * 3).toString() << '\n';
    std::cout << "  rounding   " << Money::fromDollars(2.345).toString()
              << " (from 2.345)\n\n";
}

void printRow(const DenominationTable::Ptr& denomination) {
    std::cout << "  " << std::left << std::setw(8) << denomination->label()
              << std::right << std::setw(10) << denomination->value().toString()
              << "  " << std::left << std::setw(7) << categoryName(denomination->category())
              << std::setw(16) << denomination->assetName();

    if (denomination->isChip()) {
        const auto* chip = static_cast<const ChipDenomination*>(denomination.get());
        std::cout << "stack of " << chip->stackSize();
    }
    std::cout << '\n';
}

void demoDenominations() {
    const DenominationTable table = DenominationTable::standardAustralian();

    std::cout << "=== Currency (" << table.currency().size() << ") ===\n";
    for (const auto& denomination : table.currency()) {
        printRow(denomination);
    }

    std::cout << "\n=== Chips (" << table.chips().size() << ") ===\n";
    for (const auto& denomination : table.chips()) {
        printRow(denomination);
    }

    std::cout << "\n=== Lookups ===\n";
    if (const auto fifty = table.findCurrency(5000)) {
        std::cout << "  findCurrency(5000) -> " << fifty->label() << '\n';
    }
    if (const auto plaque = table.findChip(2500)) {
        std::cout << "  findChip(2500)     -> " << plaque->label()
                  << " (stack of "
                  << static_cast<const ChipDenomination*>(plaque.get())->stackSize()
                  << ")\n";
    }
    std::cout << "  findChip(9999)     -> "
              << (table.findChip(9999) ? "found" : "none") << '\n';
}

void printCashbox(const Cashbox& cashbox) {
    for (const auto& entry : cashbox.stock().entries()) {
        std::cout << "  " << std::left << std::setw(6) << entry.denomination->label()
                  << " x " << std::right << std::setw(3) << entry.qty << "   "
                  << (entry.denomination->value() * entry.qty).toString() << '\n';
    }
    std::cout << "  total = " << cashbox.total().toString() << '\n';
}

void demoCashbox() {
    const DenominationTable table = DenominationTable::standardAustralian();
    Cashbox cashbox = Cashbox::withStandardCurrency(table);

    // Build an opening float and load it.
    DenominationCount openingFloat;
    const auto addFloat = [&](std::int64_t cents, int qty) {
        if (const auto denomination = table.findCurrency(cents)) {
            openingFloat.add(denomination, qty);
        }
    };
    addFloat(5000, 2); addFloat(2000, 5); addFloat(1000, 10); addFloat(500, 10);
    addFloat(200, 20); addFloat(100, 20); addFloat(50, 20);
    addFloat(20, 20); addFloat(10, 20); addFloat(5, 40);
    cashbox.applyFloat(openingFloat);

    std::cout << "=== Cashbox opening float ===\n";
    printCashbox(cashbox);

    // A player returns chips worth $35.50 and leaves a 50c tip.
    DenominationCount chips;
    const auto addChip = [&](std::int64_t cents, int qty) {
        if (const auto denomination = table.findChip(cents)) {
            chips.add(denomination, qty);
        }
    };
    addChip(2500, 1); // $25 plaque
    addChip(500, 2);  // 2 x $5 chip
    addChip(25, 2);   // 2 x 25c chip
    const Money tip = Money::fromDollars(0.50);

    CashOutService service(cashbox);
    const auto result = service.cashOut("ALICE", chips, tip);

    std::cout << "\n=== Cash-out ===\n";
    std::cout << "  chips returned " << chips.total().toString() << '\n';
    std::cout << "  tip (kept)     " << tip.toString() << '\n';
    if (result.success) {
        std::cout << "  cash paid out  " << result.transaction->amount().toString() << ":\n";
        for (const auto& entry : result.change.entries()) {
            std::cout << "      " << entry.qty << " x " << std::left << std::setw(6)
                      << entry.denomination->label() << "("
                      << (entry.denomination->value() * entry.qty).toString() << ")\n";
        }
        std::cout << "  " << result.transaction->describe() << '\n';
    } else {
        std::cout << "  FAILED: " << result.message << '\n';
    }

    std::cout << "\n=== Cashbox after cash-out ===\n";
    printCashbox(cashbox);

    std::cout << "\n=== Change check ===\n";
    const Money odd = Money::fromDollars(12.34);
    const auto change = cashbox.makeChange(odd);
    std::cout << "  makeChange(" << odd.toString() << ") -> "
              << (change ? "ok" : "cannot make exact change") << '\n';
}

} // namespace

int main() {
    std::cout << "PokerSoc Connect (C++) - domain demo\n\n";
    demoMoney();
    demoDenominations();
    std::cout << '\n';
    demoCashbox();
    return 0;
}
