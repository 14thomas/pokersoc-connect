#pragma once

#include <string_view>

namespace pokersoc::domain {

/// Club membership tiers (ported from the original app). `None` is the default
/// for a walk-in guest who is not a paid member.
enum class MembershipType { None, Bronze, Silver, Gold, Platinum, Diamond };

/// Stable text token for persistence / logs (e.g. "GOLD").
inline const char* membershipTypeName(MembershipType type) noexcept {
    switch (type) {
        case MembershipType::None:     return "NONE";
        case MembershipType::Bronze:   return "BRONZE";
        case MembershipType::Silver:   return "SILVER";
        case MembershipType::Gold:     return "GOLD";
        case MembershipType::Platinum: return "PLATINUM";
        case MembershipType::Diamond:  return "DIAMOND";
    }
    return "NONE";
}

/// Inverse of membershipTypeName; unknown tokens fall back to `None`.
inline MembershipType membershipTypeFromName(std::string_view name) noexcept {
    if (name == "BRONZE")   return MembershipType::Bronze;
    if (name == "SILVER")   return MembershipType::Silver;
    if (name == "GOLD")     return MembershipType::Gold;
    if (name == "PLATINUM") return MembershipType::Platinum;
    if (name == "DIAMOND")  return MembershipType::Diamond;
    return MembershipType::None;
}

} // namespace pokersoc::domain
