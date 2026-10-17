#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "domain/money/Denomination.h"
#include "domain/money/DenominationCount.h"
#include "domain/money/Money.h"

namespace pokersoc::ui {

using DenomList = std::vector<std::shared_ptr<const domain::Denomination>>;

/// Console input helpers. Every reader is end-of-input safe: once stdin closes
/// (e.g. piped input runs out) they return their fallback and `inputClosed()`
/// turns true so menu loops can unwind cleanly.
bool inputClosed() noexcept;

/// Print `prompt`, read one trimmed line. Returns `fallback` for an empty line
/// or end of input.
std::string ask(const std::string& prompt, const std::string& fallback = "");

/// A dollar amount ("25", "$25.50"). Empty line -> `fallback`; with no fallback
/// it re-prompts. Returns nullopt only at end of input, or when the user types
/// "x" to cancel.
std::optional<domain::Money> askMoney(const std::string& prompt,
                                      std::optional<domain::Money> fallback = std::nullopt);

int askInt(const std::string& prompt, int fallback);
bool askYesNo(const std::string& prompt, bool fallback);

/// A tally of physical notes/coins/chips. Accepted forms:
///   "120.50"          one amount, broken down largest-first
///   "50 20 20 5"      one piece of each listed denomination
///   "50x2 20x1 5cx4"  denomination x quantity ("5c" or "0.05" for 5 cents)
/// Empty line -> empty tally (or re-prompt if `allowEmpty` is false).
/// Returns nullopt at end of input or when the user types "x" to cancel.
std::optional<domain::DenominationCount> askCount(const std::string& prompt,
                                                  const DenomList& denominations,
                                                  bool allowEmpty = true);

/// Greedy largest-first breakdown of `amount` over `denominations` (unlimited
/// supply). Leaves any remainder that cannot be represented.
domain::DenominationCount breakdown(domain::Money amount, const DenomList& denominations);

/// "    $50 x  2   $100.00" style lines.
void printCount(const domain::DenominationCount& count, const std::string& indent = "    ");
/// "2x$50 1x20c" on one line.
std::string inlineCount(const domain::DenominationCount& count);

void pause();

} // namespace pokersoc::ui
