#include "util/Csv.h"

namespace pokersoc::util {

std::string csvQuote(const std::string& field) {
    std::string result = "\"";
    for (const char c : field) {
        if (c == '"') result += '"';
        result += c;
    }
    result += '"';
    return result;
}

std::string csvEscape(const std::string& field) {
    if (field.find_first_of(",\"\r\n") == std::string::npos) {
        return field;
    }
    return csvQuote(field);
}

std::vector<std::string> csvParseLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool inQuotes = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    current += '"';
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                current += c;
            }
        } else if (c == '"') {
            inQuotes = true;
        } else if (c == ',') {
            fields.push_back(current);
            current.clear();
        } else if (c != '\r') {
            current += c;
        }
    }
    fields.push_back(current);
    return fields;
}

std::string trim(const std::string& text) {
    const char* whitespace = " \t\r\n";
    const auto first = text.find_first_not_of(whitespace);
    if (first == std::string::npos) {
        return "";
    }
    const auto last = text.find_last_not_of(whitespace);
    return text.substr(first, last - first + 1);
}

} // namespace pokersoc::util
