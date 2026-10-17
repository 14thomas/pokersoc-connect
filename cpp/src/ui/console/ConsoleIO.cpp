#include "ui/console/ConsoleIO.h"

#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "util/Csv.h"

namespace pokersoc::ui {

using domain::DenominationCount;
using domain::Money;

namespace {

bool g_inputClosed = false;

bool readLine(const std::string& prompt, std::string& out) {
    if (g_inputClosed) {
        return false;
    }
    std::cout << prompt;
    std::cout.flush();
    if (!std::getline(std::cin, out)) {
        g_inputClosed = true;
        std::cout << '\n';
        return false;
    }
    out = util::trim(out);
    return true;
}

bool isCancel(const std::string& text) {
    return text == "x" || text == "X";
}

/// "$50", "50", "0.5" -> 5000 / 5000 / 50 cents; "5c", "50c" -> 5 / 50 cents.
std::optional<std::int64_t> parseDenominationCents(std::string token) {
    if (token.empty()) return std::nullopt;
    if (token.front() == '$') token.erase(token.begin());
    try {
        std::size_t used = 0;
        if (!token.empty() && (token.back() == 'c' || token.back() == 'C')) {
            token.pop_back();
            const long long cents = std::stoll(token, &used);
            if (used != token.size()) return std::nullopt;
            return cents;
        }
        const double dollars = std::stod(token, &used);
        if (used != token.size()) return std::nullopt;
        return static_cast<std::int64_t>(std::llround(dollars * 100.0));
    } catch (...) {
        return std::nullopt;
    }
}

std::shared_ptr<const domain::Denomination> findDenomination(const DenomList& denominations,
                                                             std::int64_t cents) {
    for (const auto& denomination : denominations) {
        if (denomination->cents() == cents) return denomination;
    }
    return nullptr;
}

std::string listLabels(const DenomList& denominations) {
    std::string text;
    for (const auto& denomination : denominations) {
        if (!text.empty()) text += ' ';
        text += denomination->label();
    }
    return text;
}

} // namespace

bool inputClosed() noexcept {
    return g_inputClosed;
}

std::string ask(const std::string& prompt, const std::string& fallback) {
    std::string line;
    if (!readLine(prompt, line) || line.empty()) {
        return fallback;
    }
    return line;
}

std::optional<Money> askMoney(const std::string& prompt, std::optional<Money> fallback) {
    while (true) {
        std::string line;
        if (!readLine(prompt, line)) {
            return std::nullopt;
        }
        if (isCancel(line)) {
            return std::nullopt;
        }
        if (line.empty()) {
            if (fallback) return fallback;
            std::cout << "  Please enter an amount (or x to cancel).\n";
            continue;
        }
        if (line.front() == '$') line.erase(line.begin());
        try {
            std::size_t used = 0;
            const double dollars = std::stod(line, &used);
            if (used == line.size() && dollars >= 0) {
                return Money::fromDollars(dollars);
            }
        } catch (...) {
        }
        std::cout << "  Please enter a dollar amount (e.g. 25 or 25.50).\n";
    }
}

int askInt(const std::string& prompt, int fallback) {
    std::string line;
    if (!readLine(prompt, line) || line.empty()) {
        return fallback;
    }
    try {
        std::size_t used = 0;
        const int value = std::stoi(line, &used);
        return used == line.size() ? value : fallback;
    } catch (...) {
        return fallback;
    }
}

bool askYesNo(const std::string& prompt, bool fallback) {
    std::string line;
    if (!readLine(prompt, line) || line.empty()) {
        return fallback;
    }
    const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(line.front())));
    if (c == 'y') return true;
    if (c == 'n') return false;
    return fallback;
}

DenominationCount breakdown(Money amount, const DenomList& denominations) {
    DenominationCount count;
    std::int64_t remaining = amount.cents();
    for (const auto& denomination : denominations) {
        const std::int64_t face = denomination->cents();
        if (face <= 0 || remaining < face) continue;
        const auto qty = static_cast<int>(remaining / face);
        count.add(denomination, qty);
        remaining -= face * qty;
    }
    return count;
}

std::optional<DenominationCount> askCount(const std::string& prompt,
                                          const DenomList& denominations,
                                          bool allowEmpty) {
    while (true) {
        std::string line;
        if (!readLine(prompt, line)) {
            return std::nullopt;
        }
        if (isCancel(line)) {
            return std::nullopt;
        }
        if (line.empty() || line == "0") {
            if (allowEmpty) return DenominationCount{};
            std::cout << "  Please enter an amount (or x to cancel).\n";
            continue;
        }

        std::vector<std::string> tokens;
        {
            std::istringstream stream(line);
            std::string token;
            while (stream >> token) tokens.push_back(token);
        }

        bool ok = true;
        DenominationCount count;

        const bool singleAmount =
            tokens.size() == 1 && tokens[0].find_first_of("xX*") == std::string::npos;
        if (singleAmount) {
            const auto cents = parseDenominationCents(tokens[0]);
            if (!cents || *cents <= 0) {
                ok = false;
            } else {
                count = breakdown(Money::fromCents(*cents), denominations);
                if (count.total().cents() != *cents) {
                    std::cout << "  " << Money::fromCents(*cents).toString()
                              << " cannot be made from " << listLabels(denominations) << ".\n";
                    continue;
                }
            }
        } else {
            for (const auto& token : tokens) {
                const auto split = token.find_first_of("xX*");
                const std::string face = token.substr(0, split);
                int qty = 1;
                if (split != std::string::npos) {
                    try {
                        std::size_t used = 0;
                        const std::string qtyText = token.substr(split + 1);
                        qty = std::stoi(qtyText, &used);
                        if (used != qtyText.size() || qty <= 0) ok = false;
                    } catch (...) {
                        ok = false;
                    }
                }
                const auto cents = parseDenominationCents(face);
                const auto denomination = cents ? findDenomination(denominations, *cents) : nullptr;
                if (!ok || !denomination) {
                    std::cout << "  '" << token << "' is not one of: " << listLabels(denominations)
                              << '\n';
                    ok = false;
                    break;
                }
                count.add(denomination, qty);
            }
        }

        if (!ok) {
            std::cout << "  Enter an amount (e.g. 45.50), pieces (50 20 5) or "
                         "denomination x qty (50x2 5cx4); x cancels.\n";
            continue;
        }
        return count;
    }
}

void printCount(const DenominationCount& count, const std::string& indent) {
    if (count.empty()) {
        std::cout << indent << "(none)\n";
        return;
    }
    for (const auto& entry : count.entries()) {
        std::cout << indent << std::left << std::setw(6) << entry.denomination->label()
                  << " x " << std::right << std::setw(3) << entry.qty << "   "
                  << (entry.denomination->value() * entry.qty).toString() << '\n';
    }
}

std::string inlineCount(const DenominationCount& count) {
    std::string text;
    for (const auto& entry : count.entries()) {
        if (!text.empty()) text += ' ';
        text += std::to_string(entry.qty) + "x" + entry.denomination->label();
    }
    return text.empty() ? "(none)" : text;
}

void pause() {
    std::string ignored;
    readLine("  (press Enter)", ignored);
}

} // namespace pokersoc::ui
