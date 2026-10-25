#pragma once

#include <cstdint>
#include <map>
#include <string>

#include <QColor>
#include <QFont>
#include <QString>

#include "domain/money/Money.h"

class QApplication;
class QLabel;
class QWidget;

namespace pokersoc::domain {
class DenominationCount;
class DenominationTable;
} // namespace pokersoc::domain

/// Look-and-feel shared by every Qt screen. The GUI mirrors the original WPF
/// app, so sizes are WPF device-independent pixels and colours are the WPF
/// named brushes the XAML used.
namespace pokersoc::ui {

/// A tally being built on screen: face value in cents -> quantity.
using Counts = std::map<std::int64_t, int>;

namespace color {
inline constexpr const char* Gray = "#808080";
inline constexpr const char* DarkGray = "#A9A9A9";
inline constexpr const char* LightGray = "#D3D3D3";
inline constexpr const char* LightBlue = "#ADD8E6";
inline constexpr const char* LightGreen = "#90EE90";
inline constexpr const char* LightCoral = "#F08080";
inline constexpr const char* LightYellow = "#FFFFE0";
inline constexpr const char* Orange = "#FFA500";
inline constexpr const char* OrangeRed = "#FF4500";
inline constexpr const char* DarkOrange = "#FF8C00";
inline constexpr const char* Yellow = "#FFFF00";
inline constexpr const char* Gold = "#FFD700";
inline constexpr const char* Plum = "#DDA0DD";
inline constexpr const char* Crimson = "#DC143C";
inline constexpr const char* Green = "#008000";
inline constexpr const char* DarkGreen = "#006400";
inline constexpr const char* DarkBlue = "#00008B";
inline constexpr const char* DarkRed = "#8B0000";
inline constexpr const char* Red = "#FF0000";
inline constexpr const char* Blue = "#0000FF";
inline constexpr const char* Purple = "#800080";
inline constexpr const char* White = "#FFFFFF";
inline constexpr const char* ButtonFace = "#DDDDDD"; // WPF default button background
inline constexpr const char* SilverCoin = "#C0C0C0";
inline constexpr const char* GoldCoin = "#B8860B";
} // namespace color

QString toQ(const std::string& text);
std::string toStd(const QString& text);

/// en-AU currency, e.g. "$1,234.50" / "-$3.05".
QString money(domain::Money amount);
QString money(std::int64_t cents);
/// "5c" for sub-dollar, "$5" otherwise.
QString denomLabel(std::int64_t cents);

/// Resource path of a file compiled into the exe from the Assets folder (`:/Assets/...`).
QString assetPath(const QString& relative);

QFont uiFont(int pixelSize, bool bold = false);

/// A QLabel with a WPF-style font, colour and alignment.
QLabel* makeLabel(const QString& text, int pixelSize = 12, bool bold = false,
                  const QString& colorName = QString(), Qt::Alignment align = Qt::AlignLeft);

/// Style sheet for a QPushButton that keeps the WPF default chrome behaviour
/// (light-blue hover, grey when disabled) over a custom background.
/// `borderWidth` is CSS, e.g. "2px" or "0px 1px 1px 1px" (top right bottom left).
QString buttonStyle(const QString& background, const QString& border = color::Gray,
                    const QString& borderWidth = QStringLiteral("2px"),
                    const QString& extra = QString());

/// Style sheet for a bordered panel (QFrame with an object name).
QString panelStyle(const QString& objectName, const QString& background,
                   const QString& border, int borderPx = 2, int radius = 0);

void applyApplicationStyle(QApplication& app);

/// Convert a screen tally into a domain count using the table's currency or chips.
domain::DenominationCount toCurrencyCount(const Counts& counts,
                                          const domain::DenominationTable& table);
domain::DenominationCount toChipCount(const Counts& counts,
                                      const domain::DenominationTable& table);
Counts toCounts(const domain::DenominationCount& count);
std::int64_t totalCents(const Counts& counts);

} // namespace pokersoc::ui
