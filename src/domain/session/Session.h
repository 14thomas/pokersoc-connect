#pragma once

#include <memory>
#include <utility>

#include "domain/session/SessionMode.h"

namespace pokersoc::domain {

/// A running session. It owns the active SessionMode (cash game or tournament)
/// — the one thing that differs between session types. The cashbox and player
/// directory deliberately live outside the Session, because they are shared and
/// behave identically regardless of mode.
class Session {
public:
    explicit Session(std::unique_ptr<SessionMode> mode) : mode_(std::move(mode)) {}

    SessionMode& mode() noexcept { return *mode_; }
    const SessionMode& mode() const noexcept { return *mode_; }

    SessionModeType modeType() const noexcept { return mode_->type(); }
    std::string modeName() const { return mode_->name(); }

private:
    std::unique_ptr<SessionMode> mode_;
};

} // namespace pokersoc::domain
