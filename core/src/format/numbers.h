// Numbers in page files (FORMAT.md, Page SVG): std::to_chars fixed format at
// a given precision, "-0" written as "0", trailing zeros removed; fast_float
// reads them back (std::from_chars(double) is unavailable at iOS 18.0).
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace ink_engine {

inline constexpr int kCoordinatePrecision = 2;  // coordinates, sizes, transforms
inline constexpr int kAnglePrecision = 3;       // force and angles
inline constexpr int kTimePrecision = 0;        // T, whole milliseconds

std::string FormatNumber(double value, int precision);

// Parses a whole string as a number; std::nullopt if any character is left.
std::optional<double> ParseNumber(std::string_view text);

// Parses the next number in `text` after skipping spaces and commas, and
// advances `text` past it.
std::optional<double> NextNumber(std::string_view &text);

}  // namespace ink_engine
