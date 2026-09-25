#include "layout/layout.h"

#include <algorithm>

namespace ink_engine {

std::vector<PagePlacement> LayoutPages(const Document &document) {
  double content_width = 0;
  for (const auto &page : document.pages) {
    if (!page->unlisted) content_width = std::max(content_width, page->width);
  }
  std::vector<PagePlacement> layout;
  double y = 0;
  for (size_t i = 0; i < document.pages.size(); ++i) {
    const Page &page = *document.pages[i];
    if (page.unlisted) continue;
    layout.push_back({i, (content_width - page.width) / 2, y, page.width, page.height});
    y += page.height + kPageGap;
  }
  return layout;
}

const PagePlacement *PageAt(const std::vector<PagePlacement> &layout, double y) {
  if (layout.empty()) return nullptr;
  for (const PagePlacement &p : layout) {
    if (y < p.y + p.height + kPageGap / 2) return &p;
  }
  return &layout.back();
}

}  // namespace ink_engine

namespace ink_engine {

PagePlacement GhostPlacement(const std::vector<PagePlacement> &layout, double width, double height) {
  double content_width = layout.empty() ? width : 0, y = 0;
  for (const PagePlacement &p : layout) content_width = std::max(content_width, p.width);
  if (!layout.empty()) y = layout.back().y + layout.back().height + kPageGap;
  return {layout.empty() ? 0 : layout.back().page + 1, (content_width - width) / 2, y, width, height};
}

}  // namespace ink_engine
