#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace pokersoc::domain {

/// One card scan at the door / desk.
struct AttendanceRecord {
    std::string playerId;
    std::string time; // UTC "YYYY-MM-DD HH:MM:SS"
};

/// Every scan made during the session, in scan order. Players may be scanned
/// many times; reports usually want unique players or each player's first scan.
class Attendance {
public:
    void add(AttendanceRecord record) { records_.push_back(std::move(record)); }
    void clear() noexcept { records_.clear(); }

    const std::vector<AttendanceRecord>& records() const noexcept { return records_; }
    std::size_t scanCount() const noexcept { return records_.size(); }

    /// The first scan of each distinct player, in scan order.
    std::vector<AttendanceRecord> firstScans() const;
    std::size_t uniquePlayerCount() const { return firstScans().size(); }

private:
    std::vector<AttendanceRecord> records_;
};

} // namespace pokersoc::domain
