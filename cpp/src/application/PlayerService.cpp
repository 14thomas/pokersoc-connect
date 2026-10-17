#include "application/PlayerService.h"

#include <utility>

#include "domain/player/Player.h"
#include "domain/player/PlayerDirectory.h"
#include "persistence/AppStore.h"

namespace pokersoc::application {

const domain::Player* PlayerService::find(const std::string& id) const {
    return directory_.find(id);
}

const domain::Player& PlayerService::save(domain::Player player) {
    store_.recordPlayer(player);                   // persist first (crash-safe)
    return directory_.upsert(std::move(player));    // then update in memory
}

std::vector<const domain::Player*> PlayerService::all() const {
    return directory_.all();
}

std::size_t PlayerService::count() const {
    return directory_.size();
}

} // namespace pokersoc::application
