#include "application/AdminService.h"

#include "persistence/AppStore.h"

namespace pokersoc::application {

namespace {
constexpr const char* kPasswordKey = "admin_password";
constexpr const char* kDefaultPassword = "1234";
} // namespace

std::string AdminService::password() const {
    const auto stored = store_.getSetting(kPasswordKey);
    return stored && !stored->empty() ? *stored : kDefaultPassword;
}

bool AdminService::checkPassword(const std::string& attempt) const {
    return attempt == password();
}

bool AdminService::setPassword(const std::string& password) {
    if (password.empty()) {
        return false;
    }
    store_.setSetting(kPasswordKey, password);
    return true;
}

} // namespace pokersoc::application
