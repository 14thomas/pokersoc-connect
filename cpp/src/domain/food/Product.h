#pragma once

#include <string>

#include "domain/money/Money.h"

namespace pokersoc::domain {

/// A food/drink item on sale at the session. `image` is a file name kept for the
/// (future) GUI; the console ignores it. The id is assigned by the store.
struct Product {
    long long id = 0;
    std::string name;
    Money price;
    std::string image;
};

} // namespace pokersoc::domain
