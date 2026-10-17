#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "domain/player/Player.h"

namespace pokersoc::domain {

/// In-memory index of players keyed by id (the scanned card barcode).
///
/// Pure domain state — lookups, inserts and updates only. Persistence is driven
/// a layer up by PlayerService, exactly as the Cashbox is driven by its
/// service. Entries are kept ordered by id so listings are deterministic.
class PlayerDirectory {
public:
    /// Insert a new player or replace the existing one with the same id.
    /// Returns a reference to the stored player.
    const Player& upsert(Player player);

    /// The player with this id, or nullptr if none is known.
    const Player* find(const std::string& id) const;

    bool contains(const std::string& id) const;
    std::size_t size() const noexcept { return players_.size(); }
    bool empty() const noexcept { return players_.empty(); }

    /// All known players, ordered by id.
    std::vector<const Player*> all() const;

private:
    std::map<std::string, Player> players_;
};

} // namespace pokersoc::domain
