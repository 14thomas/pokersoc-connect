#pragma once

#include <string>
#include <vector>

namespace pokersoc::util {

/// Quote a field only if it contains a comma, quote or newline.
std::string csvEscape(const std::string& field);

/// Always wrap in quotes (doubling any embedded quotes).
std::string csvQuote(const std::string& field);

/// Split one CSV line into fields, honouring quoted fields and "" escapes.
std::vector<std::string> csvParseLine(const std::string& line);

/// Trim ASCII whitespace from both ends.
std::string trim(const std::string& text);

} // namespace pokersoc::util
