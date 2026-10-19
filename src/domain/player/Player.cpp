#include "domain/player/Player.h"

namespace pokersoc::domain {

std::string Player::describe() const {
    std::string out = id_.empty() ? "(no id)" : id_;
    if (!name_.empty()) {
        out += "  " + name_;
    }
    out += "  [" + std::string(arcStatusName(arcStatus_)) + ", " +
           membershipTypeName(membership_) + "]";
    if (isUnderage_) {
        out += "  (underage)";
    }
    return out;
}

} // namespace pokersoc::domain
