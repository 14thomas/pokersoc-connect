#include "util/Json.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace pokersoc::util {

namespace {

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}

    JsonValue parseDocument() {
        JsonValue value = parseValue();
        skipWhitespace();
        if (pos_ != text_.size()) {
            fail("trailing characters");
        }
        return value;
    }

private:
    [[noreturn]] void fail(const char* what) const {
        throw std::runtime_error("JSON: " + std::string(what) + " at offset " +
                                 std::to_string(pos_));
    }

    void skipWhitespace() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    char peek() {
        skipWhitespace();
        return pos_ < text_.size() ? text_[pos_] : '\0';
    }

    void expect(char c) {
        if (peek() != c) {
            fail("unexpected character");
        }
        ++pos_;
    }

    bool consumeLiteral(const char* literal) {
        const std::string word(literal);
        if (text_.compare(pos_, word.size(), word) == 0) {
            pos_ += word.size();
            return true;
        }
        return false;
    }

    JsonValue parseValue() {
        JsonValue value;
        const char c = peek();
        if (c == '{') {
            parseObject(value);
        } else if (c == '[') {
            parseArray(value);
        } else if (c == '"') {
            value.kind = JsonValue::Kind::String;
            value.string = parseString();
        } else if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            value.kind = JsonValue::Kind::Number;
            value.number = parseNumber();
        } else if (consumeLiteral("true")) {
            value.kind = JsonValue::Kind::Bool;
            value.boolean = true;
        } else if (consumeLiteral("false")) {
            value.kind = JsonValue::Kind::Bool;
        } else if (consumeLiteral("null")) {
            value.kind = JsonValue::Kind::Null;
        } else {
            fail("unexpected token");
        }
        return value;
    }

    void parseObject(JsonValue& value) {
        value.kind = JsonValue::Kind::Object;
        expect('{');
        if (peek() == '}') {
            ++pos_;
            return;
        }
        while (true) {
            if (peek() != '"') {
                fail("expected key");
            }
            value.keys.push_back(parseString());
            expect(':');
            value.items.push_back(parseValue());
            const char next = peek();
            ++pos_;
            if (next == '}') return;
            if (next != ',') fail("expected , or }");
        }
    }

    void parseArray(JsonValue& value) {
        value.kind = JsonValue::Kind::Array;
        expect('[');
        if (peek() == ']') {
            ++pos_;
            return;
        }
        while (true) {
            value.items.push_back(parseValue());
            const char next = peek();
            ++pos_;
            if (next == ']') return;
            if (next != ',') fail("expected , or ]");
        }
    }

    static void appendUtf8(std::string& out, unsigned codepoint) {
        if (codepoint < 0x80) {
            out += static_cast<char>(codepoint);
        } else if (codepoint < 0x800) {
            out += static_cast<char>(0xC0 | (codepoint >> 6));
            out += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else if (codepoint < 0x10000) {
            out += static_cast<char>(0xE0 | (codepoint >> 12));
            out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (codepoint >> 18));
            out += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
    }

    unsigned parseHex4() {
        if (pos_ + 4 > text_.size()) fail("bad \\u escape");
        const std::string hex = text_.substr(pos_, 4);
        pos_ += 4;
        char* end = nullptr;
        const unsigned long value = std::strtoul(hex.c_str(), &end, 16);
        if (end != hex.c_str() + 4) fail("bad \\u escape");
        return static_cast<unsigned>(value);
    }

    std::string parseString() {
        expect('"');
        std::string out;
        while (pos_ < text_.size()) {
            const char c = text_[pos_++];
            if (c == '"') {
                return out;
            }
            if (c != '\\') {
                out += c;
                continue;
            }
            if (pos_ >= text_.size()) break;
            const char escape = text_[pos_++];
            switch (escape) {
                case '"':  out += '"'; break;
                case '\\': out += '\\'; break;
                case '/':  out += '/'; break;
                case 'b':  out += '\b'; break;
                case 'f':  out += '\f'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case 'u': {
                    unsigned codepoint = parseHex4();
                    if (codepoint >= 0xD800 && codepoint <= 0xDBFF &&
                        text_.compare(pos_, 2, "\\u") == 0) {
                        pos_ += 2;
                        const unsigned low = parseHex4();
                        codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                    }
                    appendUtf8(out, codepoint);
                    break;
                }
                default: fail("bad escape");
            }
        }
        fail("unterminated string");
    }

    double parseNumber() {
        const char* start = text_.c_str() + pos_;
        char* end = nullptr;
        const double value = std::strtod(start, &end);
        if (end == start) fail("bad number");
        pos_ += static_cast<std::size_t>(end - start);
        return value;
    }

    const std::string& text_;
    std::size_t pos_ = 0;
};

} // namespace

const JsonValue* JsonValue::get(const std::string& key) const {
    if (kind != Kind::Object) {
        return nullptr;
    }
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (keys[i] == key) {
            return &items[i];
        }
    }
    return nullptr;
}

JsonValue parseJson(const std::string& text) {
    return Parser(text).parseDocument();
}

std::string jsonString(const std::string& text) {
    std::string out = "\"";
    for (const char c : text) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buffer[8];
                    std::snprintf(buffer, sizeof buffer, "\\u%04x", c);
                    out += buffer;
                } else {
                    out += c;
                }
        }
    }
    out += '"';
    return out;
}

} // namespace pokersoc::util
