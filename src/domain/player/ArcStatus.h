#pragma once

#include <string_view>

namespace pokersoc::domain {

/// Whether a player holds an ARC (student union) membership. This drives
/// tournament pricing later on; `Unknown` until we have recorded it.
enum class ArcStatus { Unknown, Arc, NonArc };

/// Stable text token for persistence / logs (e.g. "ARC").
inline const char* arcStatusName(ArcStatus status) noexcept {
    switch (status) {
        case ArcStatus::Unknown: return "UNKNOWN";
        case ArcStatus::Arc:     return "ARC";
        case ArcStatus::NonArc:  return "NON_ARC";
    }
    return "UNKNOWN";
}

/// Inverse of arcStatusName; unknown tokens fall back to `Unknown`.
inline ArcStatus arcStatusFromName(std::string_view name) noexcept {
    if (name == "ARC")     return ArcStatus::Arc;
    if (name == "NON_ARC") return ArcStatus::NonArc;
    return ArcStatus::Unknown;
}

} // namespace pokersoc::domain
