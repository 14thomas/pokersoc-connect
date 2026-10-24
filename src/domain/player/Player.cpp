#include "domain/player/Player.h"

#include <cctype>

namespace pokersoc::domain {

void Player::setStudyYear(std::string studyYear) {
    std::string folded = studyYear;
    for (char& c : folded) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    // Empty, the word None, and the old integer default 0 all mean unspecified.
    if (folded.empty() || folded == "none" || folded == "0") {
        studyYear_ = "None";
        return;
    }
    studyYear_ = std::move(studyYear);
}

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
