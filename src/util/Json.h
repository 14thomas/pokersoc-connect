#pragma once

#include <string>
#include <vector>

namespace pokersoc::util {

/// Just enough JSON for the catalog import/export: a parsed value tree.
struct JsonValue {
    enum class Kind { Null, Bool, Number, String, Array, Object };

    Kind kind = Kind::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<JsonValue> items;     // Array elements, or Object values
    std::vector<std::string> keys;    // Object keys, parallel to `items`

    /// Object member by key, or nullptr if absent / not an object.
    const JsonValue* get(const std::string& key) const;

    bool isNull() const noexcept { return kind == Kind::Null; }
    bool isArray() const noexcept { return kind == Kind::Array; }
    bool isObject() const noexcept { return kind == Kind::Object; }
};

/// Parse a JSON document. Throws std::runtime_error on malformed input.
JsonValue parseJson(const std::string& text);

/// Escape and quote a string as a JSON string literal.
std::string jsonString(const std::string& text);

} // namespace pokersoc::util
