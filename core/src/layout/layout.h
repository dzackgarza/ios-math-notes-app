// Page layout in the scroll view: listed pages stacked vertically, each
// centered on the widest one, with a fixed gap. Follows Write
// syncscribble/scribblearea.cpp:196-214 ScribbleArea::getPageOrigin
// (VIEWMODE_VERT, centerPages; styluslabs/Write 401b65d). Content coordinates
// are pt, origin at the top left of the widest page's column.
#pragma once

#include <vector>

#include "document/document.h"

namespace ink_engine {

// Write's pageSpacing of 20 units, in pt (a Write unit is 0.48 pt).
inline constexpr double kPageGap = 9.6;

struct PagePlacement {
  size_t page = 0;  // index in Document::pages
  double x = 0, y = 0, width = 0, height = 0;
  bool operator==(const PagePlacement &) const = default;
};

// Listed pages in order; unlisted pages are not laid out.
std::vector<PagePlacement> LayoutPages(const Document &document);

// The ghost page after the last page, `width` × `height`, centered on the
// widest page (Write syncscribble/scribblearea.cpp:196-214 with
// pagenum == numPages(), and :1891-1900).
PagePlacement GhostPlacement(const std::vector<PagePlacement> &layout, double width, double height);

// The placement a pen-down at content height y draws on: the page whose
// band, including half the gap on each side, holds y; the first or last page
// beyond the ends.
const PagePlacement *PageAt(const std::vector<PagePlacement> &layout, double y);

}  // namespace ink_engine
