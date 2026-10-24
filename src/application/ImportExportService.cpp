#include "application/ImportExportService.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "application/FoodService.h"
#include "application/PlayerService.h"
#include "domain/food/ProductCatalog.h"
#include "domain/player/Attendance.h"
#include "domain/player/Player.h"
#include "util/Csv.h"
#include "util/Json.h"
#include "util/Time.h"

namespace pokersoc::application {

using domain::ArcStatus;
using domain::MembershipType;
using domain::Player;

namespace {

std::string upper(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

/// The original app stored "None" for blank profile fields.
std::string orNone(const std::string& value) {
    return value.empty() ? "None" : value;
}

std::string fromNone(const std::string& value) {
    return upper(value) == "NONE" ? "" : value;
}

const char* arcMemberText(ArcStatus status) {
    switch (status) {
        case ArcStatus::Arc:     return "Yes";
        case ArcStatus::NonArc:  return "No";
        case ArcStatus::Unknown: return "None";
    }
    return "None";
}

ArcStatus arcStatusFromText(const std::string& text) {
    const std::string value = upper(util::trim(text));
    if (value == "YES" || value == "Y" || value == "TRUE" || value == "1" || value == "ARC") {
        return ArcStatus::Arc;
    }
    if (value == "NO" || value == "N" || value == "FALSE" || value == "0" || value == "NON_ARC" ||
        value == "NON-ARC") {
        return ArcStatus::NonArc;
    }
    return ArcStatus::Unknown;
}

std::ofstream openForWrite(const std::string& path) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Cannot write '" + path + "'.");
    }
    return out;
}

std::string readWholeFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot read '" + path + "'.");
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    std::string text = buffer.str();
    if (text.size() >= 3 && text.compare(0, 3, "\xEF\xBB\xBF") == 0) {
        text.erase(0, 3); // UTF-8 BOM (Excel)
    }
    return text;
}

} // namespace

std::size_t ImportExportService::exportPlayers(const std::string& path) const {
    std::ofstream out = openForWrite(path);
    out << "player_id,display_name,email,student_number,degree,study_year,arc_member,"
           "is_underage,membership_type,created_at\r\n";

    const auto players = players_.all();
    for (const Player* player : players) {
        out << util::csvEscape(player->id()) << ','
            << util::csvEscape(player->name().empty() ? "New Player" : player->name()) << ','
            << util::csvEscape(orNone(player->email())) << ','
            << util::csvEscape(orNone(player->studentNumber())) << ','
            << util::csvEscape(orNone(player->degree())) << ','
            << util::csvEscape(orNone(player->studyYear())) << ','
            << arcMemberText(player->arcStatus()) << ','
            << (player->isUnderage() ? 1 : 0) << ','
            << domain::membershipTypeLabel(player->membership()) << ','
            << util::csvEscape(player->createdAt()) << "\r\n";
    }
    return players.size();
}

ImportExportService::ImportResult ImportExportService::importPlayers(const std::string& path,
                                                                     bool merge) {
    std::istringstream in(readWholeFile(path));
    ImportResult result;

    std::string line;
    bool header = true;
    while (std::getline(in, line)) {
        if (header) {
            header = false;
            continue;
        }
        if (util::trim(line).empty()) {
            continue;
        }

        const std::vector<std::string> parts = util::csvParseLine(line);
        auto field = [&parts](std::size_t index) {
            return index < parts.size() ? util::trim(parts[index]) : std::string();
        };

        const std::string id = field(0);
        if (parts.size() < 2 || id.empty()) {
            ++result.skipped;
            continue;
        }
        const bool exists = players_.contains(id);
        if (exists && !merge) {
            ++result.skipped;
            continue;
        }

        Player player(id, field(1).empty() ? "New Player" : field(1));
        player.setEmail(fromNone(field(2)));
        player.setStudentNumber(fromNone(field(3)));
        player.setDegree(fromNone(field(4)));
        player.setStudyYear(field(5));
        player.setArcStatus(arcStatusFromText(field(6)));
        const std::string underage = upper(field(7));
        player.setUnderage(underage == "1" || underage == "TRUE");
        player.setMembership(domain::membershipTypeFromName(upper(field(8))));
        if (const Player* current = players_.find(id)) {
            player.setCreatedAt(current->createdAt());
        }

        players_.save(std::move(player));
        ++(exists ? result.updated : result.added);
    }
    return result;
}

std::size_t ImportExportService::exportSessionPlayers(const std::string& path) const {
    std::ofstream out = openForWrite(path);
    out << "Timestamp, Full Name, Email Address, ARC member, ZID (N/A if non-UNSW), Player ID\r\n";

    const auto firstScans = players_.attendance().firstScans();
    for (const auto& scan : firstScans) {
        const Player* player = players_.find(scan.playerId);
        const std::string name = player && !player->name().empty() ? player->name() : "New Player";
        const std::string email = player ? orNone(player->email()) : "None";
        const std::string arc = player ? arcMemberText(player->arcStatus()) : "None";
        const std::string zid = player ? orNone(player->studentNumber()) : "None";

        out << util::csvQuote(util::utcToLocal(scan.time, "%Y-%m-%d %H:%M:%S")) << ','
            << util::csvQuote(name) << ',' << util::csvQuote(email) << ','
            << util::csvQuote(arc) << ',' << util::csvQuote(zid) << ','
            << util::csvQuote(scan.playerId) << "\r\n";
    }
    return firstScans.size();
}

std::size_t ImportExportService::exportCatalog(const std::string& path) const {
    std::ofstream out = openForWrite(path);
    const auto products = food_.catalog().all();

    out << "{\n  \"version\": 1,\n  \"products\": [";
    for (std::size_t i = 0; i < products.size(); ++i) {
        const auto* product = products[i];
        char price[32];
        std::snprintf(price, sizeof price, "%.2f", product->price.toDollars());
        out << (i == 0 ? "\n" : ",\n") << "    {\n"
            << "      \"name\": " << util::jsonString(product->name) << ",\n"
            << "      \"price\": " << price << ",\n"
            << "      \"image\": "
            << (product->image.empty() ? "null" : util::jsonString(product->image)) << "\n"
            << "    }";
    }
    out << (products.empty() ? "]\n}\n" : "\n  ]\n}\n");
    return products.size();
}

std::size_t ImportExportService::importCatalog(const std::string& path, bool merge) {
    const util::JsonValue root = util::parseJson(readWholeFile(path));
    const util::JsonValue* products = root.get("products");
    if (!products || !products->isArray()) {
        throw std::runtime_error("'" + path + "' has no \"products\" list.");
    }

    if (!merge) {
        food_.clearCatalog();
    }

    std::size_t imported = 0;
    for (const auto& item : products->items) {
        const util::JsonValue* name = item.get("name");
        if (!name || name->kind != util::JsonValue::Kind::String || name->string.empty()) {
            continue;
        }
        domain::Product product;
        product.name = name->string;
        if (const auto* price = item.get("price"); price && price->kind == util::JsonValue::Kind::Number) {
            product.price = domain::Money::fromDollars(price->number);
        }
        if (const auto* image = item.get("image"); image && image->kind == util::JsonValue::Kind::String) {
            product.image = image->string;
        }
        if (merge) {
            if (const auto* existing = food_.catalog().findByName(product.name)) {
                product.id = existing->id; // merge updates by name instead of duplicating
            }
        }
        food_.saveProduct(std::move(product));
        ++imported;
    }
    return imported;
}

} // namespace pokersoc::application
