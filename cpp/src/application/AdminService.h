#pragma once

#include <string>

namespace pokersoc::persistence {
class AppStore;
}

namespace pokersoc::application {

/// The admin password that guards destructive actions (undo, age overrides).
/// Stored per session; defaults to "1234" like the original app.
class AdminService {
public:
    explicit AdminService(persistence::AppStore& store) : store_(store) {}

    bool checkPassword(const std::string& attempt) const;
    /// Returns false (and changes nothing) for an empty password.
    bool setPassword(const std::string& password);

private:
    std::string password() const;

    persistence::AppStore& store_;
};

} // namespace pokersoc::application
