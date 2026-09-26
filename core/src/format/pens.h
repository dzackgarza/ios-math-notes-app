// The pen presets file Notes/.pens.json (docs/FORMAT.md, Other files): one
// preset per toolbar button, in toolbar order.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "document/document.h"

namespace ink_engine {

struct PenPreset {
  std::string id;
  std::string name;
  std::string brush;  // google/ink stock brush family, as mn:brush
  int brush_version = 1;
  Rgb color;
  double opacity = 1;  // the strokes' fill-opacity
  double size = 0;     // pt
  bool operator==(const PenPreset &) const = default;
};

// The presets written on first use (issue #25).
std::vector<PenPreset> DefaultPens();

// Throws nlohmann::json::exception on bad JSON or a missing key.
std::vector<PenPreset> ReadPens(std::string_view bytes);

// Keys in FORMAT.md order, two-space indent, one trailing newline; size with
// 2 decimals and opacity with 3, as in page files.
std::string WritePens(const std::vector<PenPreset> &pens);

}  // namespace ink_engine
