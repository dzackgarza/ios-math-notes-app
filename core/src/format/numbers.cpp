#include "format/numbers.h"

#include <charconv>
#include <fast_float/fast_float.h>

namespace ink_engine {

std::string FormatNumber(double value, int precision) {
  char buffer[64];
  auto [end, ec] = std::to_chars(buffer, buffer + sizeof buffer, value,
                                 std::chars_format::fixed, precision);
  std::string text(buffer, ec == std::errc() ? end : buffer);
  if (text.find('.') != std::string::npos) {
    text.erase(text.find_last_not_of('0') + 1);
    if (text.back() == '.') text.pop_back();
  }
  if (text == "-0") text = "0";
  return text;
}

std::optional<double> ParseNumber(std::string_view text) {
  double value;
  auto [end, ec] = fast_float::from_chars(text.data(), text.data() + text.size(), value);
  if (ec != std::errc() || end != text.data() + text.size()) return std::nullopt;
  return value;
}

std::optional<double> NextNumber(std::string_view &text) {
  size_t start = text.find_first_not_of(" \t\r\n,");
  if (start == std::string_view::npos) return std::nullopt;
  text.remove_prefix(start);
  double value;
  auto [end, ec] = fast_float::from_chars(text.data(), text.data() + text.size(), value);
  if (ec != std::errc()) return std::nullopt;
  text.remove_prefix(end - text.data());
  return value;
}

}  // namespace ink_engine
