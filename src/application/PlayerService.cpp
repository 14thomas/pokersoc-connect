#include "application/PlayerService.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include "domain/player/Attendance.h"
#include "domain/player/Player.h"
#include "domain/player/PlayerDirectory.h"
#include "persistence/AppStore.h"
#include "util/Time.h"

namespace pokersoc::application {

namespace {

std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

} // namespace

const domain::Player* PlayerService::find(const std::string& id) const {
    return directory_.find(id);
}

bool PlayerService::contains(const std::string& id) const {
    return directory_.contains(id);
}

const domain::Player& PlayerService::save(domain::Player player) {
    if (player.createdAt().empty()) {
        player.setCreatedAt(util::nowUtc());
    }
    store_.recordPlayer(player);                   // persist first (crash-safe)
    return directory_.upsert(std::move(player));    // then update in memory
}

std::vector<const domain::Player*> PlayerService::all() const {
    return directory_.all();
}

std::size_t PlayerService::count() const {
    return directory_.size();
}

std::vector<const domain::Player*> PlayerService::search(const std::string& query) const {
    const std::string needle = lower(query);
    if (needle.empty()) {
        return all();
    }
    std::vector<const domain::Player*> matches;
    for (const auto* player : directory_.all()) {
        for (const std::string* field :
             {&player->id(), &player->name(), &player->email(), &player->studentNumber()}) {
            if (lower(*field).find(needle) != std::string::npos) {
                matches.push_back(player);
                break;
            }
        }
    }
    return matches;
}

bool PlayerService::setUnderage(const std::string& id, bool underage) {
    const domain::Player* existing = directory_.find(id);
    if (!existing) {
        return false;
    }
    domain::Player updated = *existing;
    updated.setUnderage(underage);
    save(std::move(updated));
    return true;
}

bool PlayerService::setMembership(const std::string& id, domain::MembershipType membership) {
    const domain::Player* existing = directory_.find(id);
    if (!existing) {
        return false;
    }
    domain::Player updated = *existing;
    updated.setMembership(membership);
    save(std::move(updated));
    return true;
}

void PlayerService::logAttendance(const std::string& id) {
    attendance_.add(store_.recordAttendance(id));
}

} // namespace pokersoc::application
