#include "domain/player/Attendance.h"

#include <unordered_set>

namespace pokersoc::domain {

std::vector<AttendanceRecord> Attendance::firstScans() const {
    std::vector<AttendanceRecord> result;
    std::unordered_set<std::string> seen;
    for (const auto& record : records_) {
        if (seen.insert(record.playerId).second) {
            result.push_back(record);
        }
    }
    return result;
}

} // namespace pokersoc::domain
