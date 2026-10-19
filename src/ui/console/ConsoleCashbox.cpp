#include <iostream>

#include "application/CashboxService.h"
#include "application/ReportService.h"
#include "domain/money/DenominationTable.h"
#include "ui/console/ConsoleApp.h"
#include "ui/console/ConsoleIO.h"

namespace pokersoc::ui {

void ConsoleApp::cashboxMenu() {
    while (!inputClosed()) {
        std::cout << "\n--- Cashbox ---\n"
                  << "  1) View cashbox\n"
                  << "  2) Add float\n"
                  << "  3) Add cash tip\n"
                  << "  4) Exchange notes/coins\n"
                  << "  0) Back\n";
        const std::string choice = ask("Choice: ", "0");
        if (choice == "1") showCashbox();
        else if (choice == "2") addFloat();
        else if (choice == "3") addTip();
        else if (choice == "4") exchange();
        else if (choice == "0") return;
    }
}

void ConsoleApp::showCashbox() {
    const auto summary = reports_->summary();
    std::cout << "\n  Cash in box:\n";
    printCount(cashbox_.stock());
    std::cout << "    total = " << summary.cashInBox.toString() << "\n\n"
              << "  Float added      " << summary.floatTotal.toString() << '\n'
              << "  Buy-ins          " << summary.buyInTotal.toString() << "  ("
              << summary.buyIns << " buy-ins, " << summary.rebuys << " rebuys)\n"
              << "  Cashed out       " << summary.cashOutTotal.toString() << "  ("
              << summary.cashOuts << ")\n"
              << "  Tips             " << summary.tipsTotal.toString()
              << "  (cash tips, lost chips, cash-out tips)\n"
              << "  Food sales       " << summary.foodSalesTotal.toString() << '\n'
              << "  Exchanges        " << summary.exchanges << '\n';
}

void ConsoleApp::addFloat() {
    const auto cash = askCount("  Float to add: ", cashbox_.currency());
    if (!cash || cash->empty()) return;
    std::cout << "  Adding " << cash->total().toString() << ": " << inlineCount(*cash) << '\n';
    if (!askYesNo("  Confirm? (y/n) [y]: ", true)) return;
    const auto result = cashboxOps_->addFloat(*cash);
    std::cout << "  " << result.message << '\n';
}

void ConsoleApp::addTip() {
    const auto cash = askCount("  Cash tip: ", cashbox_.currency());
    if (!cash || cash->empty()) return;
    const std::string note = ask("  Note (optional): ");
    const auto result = cashboxOps_->addTip(*cash, "", note);
    std::cout << "  " << result.message << '\n';
}

void ConsoleApp::exchange() {
    std::cout << "  Swap notes/coins of equal value.\n";
    const auto received = askCount("  Cash coming IN to the box: ", cashbox_.currency());
    if (!received || received->empty()) return;
    const auto given = askCount("  Cash going OUT of the box [" + received->total().toString() +
                                    " largest-first]: ",
                                cashbox_.currency());
    if (!given) return;

    domain::DenominationCount out = *given;
    if (out.empty()) {
        // Default: the same value in the fewest pieces the box can supply.
        domain::Cashbox probe = cashbox_;
        probe.applyCount(*received, +1, domain::MovementReason::Exchange);
        const auto made = probe.makeChange(received->total());
        if (!made) {
            std::cout << "  The cashbox cannot supply " << received->total().toString() << ".\n";
            return;
        }
        out = *made;
    }
    std::cout << "  In:  " << inlineCount(*received) << "\n  Out: " << inlineCount(out) << '\n';
    if (!askYesNo("  Confirm? (y/n) [y]: ", true)) return;
    const auto result = cashboxOps_->exchange(out, *received);
    std::cout << "  " << result.message << '\n';
}

void ConsoleApp::lostChips() {
    std::cout << "  Chips found or left behind are kept by the house as tips.\n";
    const auto chips = askCount("  Lost chips: ", table_.chips());
    if (!chips || chips->empty()) return;
    const std::string note = ask("  Note (optional): ");
    const auto result = cashboxOps_->recordLostChips(*chips, note);
    std::cout << "  " << result.message << '\n';
}

} // namespace pokersoc::ui
