#pragma once

#include <string>
#include <vector>

namespace pokersoc::domain {
class Player;
class PlayerDirectory;
} // namespace pokersoc::domain

namespace pokersoc::persistence {
class AppStore;
}

namespace pokersoc::application {

/// Use case: manage the player directory. GUI-agnostic — the console (and later
/// the GUI) both call this. Writes through to the store first (crash-safe),
/// then updates the in-memory directory, matching CashboxService /
/// CashOutService.
class PlayerService {
public:
    PlayerService(domain::PlayerDirectory& directory, persistence::AppStore& store)
        : directory_(directory), store_(store) {}

    /// Look up a player by scanned id (nullptr if unknown).
    const domain::Player* find(const std::string& id) const;

    /// Create or update a player. Persists first, then stores it in the
    /// directory, returning a reference to the stored player.
    const domain::Player& save(domain::Player player);

    /// Every known player, ordered by id.
    std::vector<const domain::Player*> all() const;

    std::size_t count() const;

private:
    domain::PlayerDirectory& directory_;
    persistence::AppStore& store_;
};

} // namespace pokersoc::application
