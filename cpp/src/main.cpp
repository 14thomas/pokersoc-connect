#include <cstdio>
#include <iomanip>
#include <iostream>
#include <string>

#include "application/CashOutService.h"
#include "application/CashboxService.h"
#include "domain/cashbox/Cashbox.h"
#include "domain/money/CashDenomination.h"
#include "domain/money/ChipDenomination.h"
#include "domain/money/DenominationCount.h"
#include "domain/money/DenominationTable.h"
#include "domain/money/Money.h"
#include "domain/transaction/TransactionLog.h"
#include "persistence/SqliteStore.h"

using namespace pokersoc::domain;
using pokersoc::application::CashboxService;
using pokersoc::application::CashOutService;
using pokersoc::persistence::SqliteStore;

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

DenominationCount buildOpeningFloat(const DenominationTable& table) {
    DenominationCount floatCount;
    const auto add = [&](std::int64_t cents, int qty) {
        if (const auto denomination = table.findCurrency(cents)) {
            floatCount.add(denomination, qty);
        }
    };
    add(5000, 2); add(2000, 5); add(1000, 10); add(500, 10);
    add(200, 20); add(100, 20); add(50, 20);
    add(20, 20); add(10, 20); add(5, 40);
    return floatCount;
}

DenominationCount buildReturnedChips(const DenominationTable& table) {
    DenominationCount chips;
    const auto add = [&](std::int64_t cents, int qty) {
        if (const auto denomination = table.findChip(cents)) {
            chips.add(denomination, qty);
        }
    };
    add(2500, 1); // $25 plaque
    add(500, 2);  // 2 x $5 chip
    add(25, 2);   // 2 x 25c chip
    return chips;
}

void demoPersistence() {
    const DenominationTable table = DenominationTable::standardAustralian();
    const std::string dbPath = "pokersoc.db";
    std::remove(dbPath.c_str()); // start the demo from a clean database

    // ---- Session 1: do real work; every action is written to the DB ----
    {
        SqliteStore store(dbPath);
        Cashbox cashbox = Cashbox::withStandardCurrency(table);
        TransactionLog log;
        CashboxService cashboxService(cashbox, store);
        CashOutService cashOutService(cashbox, log, store);

        cashboxService.addFloat(buildOpeningFloat(table));

        const auto result =
            cashOutService.cashOut("ALICE", buildReturnedChips(table), Money::fromDollars(0.50));

        std::cout << "=== Session 1 (live, writing to " << dbPath << ") ===\n";
        std::cout << "  " << result.message << '\n';
        std::cout << "  cashbox total = " << cashbox.total().toString()
                  << ", cash-outs = " << log.size()
                  << ", movements = " << cashbox.movements().size() << '\n';
    } // store goes out of scope here -> simulate the app closing / crashing

    // ---- Session 2: cold start, rebuild everything from the DB ----
    {
        SqliteStore store(dbPath);
        Cashbox cashbox = Cashbox::withStandardCurrency(table); // starts empty
        TransactionLog log;
        store.load(table, cashbox, log); // replays the stored events

        std::cout << "\n=== Session 2 (reloaded from " << dbPath << ") ===\n";
        std::cout << "  cashbox total = " << cashbox.total().toString()
                  << ", cash-outs = " << log.size()
                  << ", movements = " << cashbox.movements().size() << '\n';
        printCashbox(cashbox);
        for (const auto& transaction : log.entries()) {
            std::cout << "  " << transaction->describe() << '\n';
        }
    }
}

} // namespace

int main() {
    std::cout << "PokerSoc Connect (C++) - domain demo\n\n";
    demoMoney();
    demoDenominations();
    std::cout << '\n';
    demoPersistence();
    return 0;
}
