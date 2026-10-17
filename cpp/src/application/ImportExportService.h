#pragma once

#include <cstddef>
#include <string>

namespace pokersoc::application {

class FoodService;
class PlayerService;

/// File import/export in the formats the original app used, so files move
/// freely between the old and new versions:
///   - players CSV  (player_id,display_name,email,...,membership_type,created_at)
///   - session players CSV (who attended, for the club's sign-in records)
///   - product catalog JSON ({"version":1,"products":[{name,price,image}]})
/// Every function throws std::runtime_error if the file cannot be read/written.
class ImportExportService {
public:
    struct ImportResult {
        std::size_t added = 0;
        std::size_t updated = 0;
        std::size_t skipped = 0;
    };

    ImportExportService(PlayerService& players, FoodService& food)
        : players_(players), food_(food) {}

    /// Returns the number of players written.
    std::size_t exportPlayers(const std::string& path) const;
    /// `merge` = true overwrites existing players with the file's data;
    /// false only adds players that are not already known.
    ImportResult importPlayers(const std::string& path, bool merge);

    /// Players scanned in this session, by first scan. Returns rows written.
    std::size_t exportSessionPlayers(const std::string& path) const;

    /// Returns the number of products written.
    std::size_t exportCatalog(const std::string& path) const;
    /// `merge` = false clears the catalog first. Returns products imported.
    std::size_t importCatalog(const std::string& path, bool merge);

private:
    PlayerService& players_;
    FoodService& food_;
};

} // namespace pokersoc::application
