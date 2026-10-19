#pragma once

#include <string>
#include <vector>

#include "domain/player/MembershipType.h"

namespace pokersoc::domain {
class Attendance;
class Player;
class PlayerDirectory;
} // namespace pokersoc::domain

namespace pokersoc::persistence {
class AppStore;
}

namespace pokersoc::application {

/// Use case: manage the player directory and door scans. GUI-agnostic — the
/// console (and later the GUI) both call this. Writes through to the store
/// first (crash-safe), then updates the in-memory state, matching the other
/// services.
class PlayerService {
public:
    PlayerService(domain::PlayerDirectory& directory,
                  domain::Attendance& attendance,
                  persistence::AppStore& store)
        : directory_(directory), attendance_(attendance), store_(store) {}

    /// Look up a player by scanned id (nullptr if unknown).
    const domain::Player* find(const std::string& id) const;
    bool contains(const std::string& id) const;

    /// Create or update a player. Persists first, then stores it in the
    /// directory, returning a reference to the stored player.
    const domain::Player& save(domain::Player player);

    /// Every known player, ordered by id.
    std::vector<const domain::Player*> all() const;
    std::size_t count() const;

    /// Case-insensitive substring match on id, name, email and student number.
    std::vector<const domain::Player*> search(const std::string& query) const;

    // ---- quick edits (no-ops returning false if the id is unknown) ----
    bool setUnderage(const std::string& id, bool underage);
    bool setMembership(const std::string& id, domain::MembershipType membership);

    // ---- attendance ----
    /// Record that `id` was scanned in.
    void logAttendance(const std::string& id);
    const domain::Attendance& attendance() const noexcept { return attendance_; }

private:
    domain::PlayerDirectory& directory_;
    domain::Attendance& attendance_;
    persistence::AppStore& store_;
};

} // namespace pokersoc::application
