// Page files: one SVG per page (docs/FORMAT.md, Page SVG).
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "document/document.h"

namespace ink_engine {

// Parses a page file. A file that does not parse becomes an error page that
// carries the pugixml message. Layers not in `layer_ids` are kept, after them.
Page ReadPage(std::string_view bytes, const std::string &file,
              const std::vector<std::string> &layer_ids);

// Serializes a page. Never called for error pages.
std::string WritePage(const Page &page);

// The `d` attribute: per polyline an absolute M, then relative l; Z closes.
std::string WritePathData(const std::vector<Polyline> &polylines, bool closed);
std::vector<Polyline> ReadPathData(std::string_view d);

// Colors as `#RRGGBB`, written in upper case; other text reads as black.
Rgb ReadColor(std::string_view text);
std::string WriteColor(Rgb c);

}  // namespace ink_engine
