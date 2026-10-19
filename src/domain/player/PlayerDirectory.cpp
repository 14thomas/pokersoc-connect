#include "domain/player/PlayerDirectory.h"

#include <utility>

namespace pokersoc::domain {

const Player& PlayerDirectory::upsert(Player player) {
    std::string key = player.id();
    const auto result = players_.insert_or_assign(std::move(key), std::move(player));
    return result.first->second;
}

const Player* PlayerDirectory::find(const std::string& id) const {
    const auto it = players_.find(id);
    return it == players_.end() ? nullptr : &it->second;
}

bool PlayerDirectory::contains(const std::string& id) const {
    return players_.find(id) != players_.end();
}

std::vector<const Player*> PlayerDirectory::all() const {
    std::vector<const Player*> result;
    result.reserve(players_.size());
    for (const auto& entry : players_) {
        result.push_back(&entry.second);
    }
    return result;
}

} // namespace pokersoc::domain
