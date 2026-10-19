#include <iomanip>
#include <iostream>
#include <stdexcept>

#include "application/ImportExportService.h"
#include "application/PlayerService.h"
#include "application/ReportService.h"
#include "domain/player/Player.h"
#include "domain/transaction/Transaction.h"
#include "ui/console/ConsoleApp.h"
#include "ui/console/ConsoleIO.h"
#include "util/Time.h"

namespace pokersoc::ui {

using domain::ArcStatus;
using domain::MembershipType;
using domain::Player;

namespace {

const char* arcText(ArcStatus status) {
    switch (status) {
        case ArcStatus::Arc:     return "ARC";
        case ArcStatus::NonArc:  return "Non-ARC";
        case ArcStatus::Unknown: return "-";
    }
    return "-";
}

} // namespace

void ConsoleApp::playersMenu() {
    while (!inputClosed()) {
        std::cout << "\n--- Players (" << players_->count() << " known, "
                  << attendance_.uniquePlayerCount() << " scanned in this session) ---\n"
                  << "  1) List all\n"
                  << "  2) Search\n"
                  << "  3) View / edit a player\n"
                  << "  4) Set age status (admin)\n"
                  << "  5) Change membership\n"
                  << "  6) Import players CSV\n"
                  << "  7) Export players CSV\n"
                  << "  0) Back\n";
        const std::string choice = ask("Choice: ", "0");

        if (choice == "1") {
            listPlayers(players_->all());
        } else if (choice == "2") {
            listPlayers(players_->search(ask("  Search for: ")));
        } else if (choice == "3") {
            const std::string id = ask("  Player id [" + currentPlayerId_ + "]: ", currentPlayerId_);
            const Player* player = players_->find(id);
            if (!player) {
                std::cout << "  Unknown player.\n";
                continue;
            }
            showPlayer(*player);
            if (askYesNo("  Edit this player? (y/n) [n]: ", false)) editPlayer(id);
        } else if (choice == "4") {
            setAgeStatus();
        } else if (choice == "5") {
            const std::string id = ask("  Player id [" + currentPlayerId_ + "]: ", currentPlayerId_);
            if (players_->find(id)) editMembership(id);
            else std::cout << "  Unknown player.\n";
        } else if (choice == "6") {
            importPlayers();
        } else if (choice == "7") {
            exportPlayers();
        } else if (choice == "0") {
            return;
        }
    }
}

void ConsoleApp::listPlayers(const std::vector<const Player*>& players) {
    if (players.empty()) {
        std::cout << "  (no players)\n";
        return;
    }
    std::cout << "  " << std::left << std::setw(14) << "ID" << std::setw(22) << "Name"
              << std::setw(9) << "ARC" << std::setw(10) << "Member" << "Age\n";
    for (const Player* player : players) {
        std::cout << "  " << std::left << std::setw(14) << player->id() << std::setw(22)
                  << (player->name().empty() ? "New Player" : player->name()) << std::setw(9)
                  << arcText(player->arcStatus()) << std::setw(10)
                  << domain::membershipTypeLabel(player->membership())
                  << (player->isUnderage() ? "UNDER 18" : "18+") << '\n';
    }
    std::cout << std::right << "  " << players.size() << " player(s).\n";
}

void ConsoleApp::showPlayer(const Player& player) {
    std::cout << "\n  " << (player.name().empty() ? "New Player" : player.name()) << "  ("
              << player.id() << ")\n";
    auto line = [](const char* label, const std::string& value) {
        if (!value.empty()) std::cout << "    " << std::left << std::setw(12) << label << value << '\n';
    };
    line("Email", player.email());
    line("Student #", player.studentNumber());
    line("Degree", player.degree());
    line("Year", player.studyYear() > 0 ? std::to_string(player.studyYear()) : "");
    line("ARC", player.arcStatus() == ArcStatus::Unknown ? "" : arcText(player.arcStatus()));
    line("Membership", domain::membershipTypeLabel(player.membership()));
    line("Age", player.isUnderage() ? "UNDER 18" : "18+");
    line("Added", util::utcToLocal(player.createdAt(), "%Y-%m-%d %H:%M"));
    std::cout << std::right;

    const auto totals = reports_->playerTotals(player.id());
    std::cout << "    Bought in " << totals.boughtIn.toString() << " (" << totals.buyIns
              << "), cashed out " << totals.paidOut.toString() << " (" << totals.cashOuts
              << "), net " << totals.net().toString() << '\n';

    const auto history = reports_->activity(player.id());
    for (const auto* transaction : history) {
        std::cout << "    #" << transaction->id() << "  "
                  << util::utcToLocal(transaction->time()) << "  " << transaction->describe()
                  << '\n';
    }
}

void ConsoleApp::editPlayer(const std::string& id) {
    const Player* existing = players_->find(id);
    if (!existing) return;
    Player player = *existing;

    player.setName(ask("  Name [" + player.name() + "]: ", player.name()));
    player.setEmail(ask("  Email [" + player.email() + "]: ", player.email()));
    player.setStudentNumber(
        ask("  Student # [" + player.studentNumber() + "]: ", player.studentNumber()));
    player.setDegree(ask("  Degree [" + player.degree() + "]: ", player.degree()));
    player.setStudyYear(askInt("  Year of study [" + std::to_string(player.studyYear()) + "]: ",
                               player.studyYear()));

    const std::string arc =
        ask(std::string("  ARC member? (y/n/u = unknown) [") + arcText(player.arcStatus()) + "]: ");
    if (arc == "y" || arc == "Y") player.setArcStatus(ArcStatus::Arc);
    else if (arc == "n" || arc == "N") player.setArcStatus(ArcStatus::NonArc);
    else if (arc == "u" || arc == "U") player.setArcStatus(ArcStatus::Unknown);

    std::cout << "  Membership: 0) None 1) Bronze 2) Silver 3) Gold 4) Platinum 5) Diamond\n";
    const int tier = askInt(std::string("  Membership [") +
                                domain::membershipTypeLabel(player.membership()) + "]: ",
                            static_cast<int>(player.membership()));
    if (tier >= 0 && tier <= 5) player.setMembership(static_cast<MembershipType>(tier));

    players_->save(std::move(player));
    std::cout << "  Saved.\n";
}

void ConsoleApp::editMembership(const std::string& id) {
    const Player* player = players_->find(id);
    if (!player) return;
    std::cout << "  Current membership: " << domain::membershipTypeLabel(player->membership())
              << "\n  0) None 1) Bronze 2) Silver 3) Gold 4) Platinum 5) Diamond\n";
    const int tier = askInt("  New membership [keep]: ", -1);
    if (tier < 0 || tier > 5) return;
    players_->setMembership(id, static_cast<MembershipType>(tier));
    std::cout << "  Membership set to " << domain::membershipTypeLabel(static_cast<MembershipType>(tier))
              << ".\n";
}

void ConsoleApp::setAgeStatus() {
    const std::string id = ask("  Player id [" + currentPlayerId_ + "]: ", currentPlayerId_);
    const Player* player = players_->find(id);
    if (!player) {
        std::cout << "  Unknown player.\n";
        return;
    }
    std::cout << "  " << id << " is currently " << (player->isUnderage() ? "UNDER 18" : "18+")
              << ".\n";
    if (!confirmAdmin()) return;
    const bool underage = askInt("  Set to 1) 18+  2) Under 18: ", 0) == 2;
    players_->setUnderage(id, underage);
    std::cout << "  " << id << " is now " << (underage ? "UNDER 18" : "18+") << ".\n";
}

void ConsoleApp::importPlayers() {
    const std::string path = ask("  CSV file to import: ");
    if (path.empty()) return;
    const bool merge =
        askInt("  1) Merge (update existing players)  2) Only add new players [1]: ", 1) != 2;
    try {
        const auto result = files_->importPlayers(path, merge);
        std::cout << "  Added " << result.added << ", updated " << result.updated << ", skipped "
                  << result.skipped << ".\n";
    } catch (const std::exception& error) {
        std::cout << "  Import failed: " << error.what() << '\n';
    }
}

void ConsoleApp::exportPlayers() {
    const std::string fallback = "players_" + util::localNow("%Y-%m-%d_%H%M%S") + ".csv";
    const std::string path = ask("  Export to [" + fallback + "]: ", fallback);
    try {
        const auto count = files_->exportPlayers(path);
        std::cout << "  Exported " << count << " player(s) to " << path << ".\n";
    } catch (const std::exception& error) {
        std::cout << "  Export failed: " << error.what() << '\n';
    }
}

} // namespace pokersoc::ui
