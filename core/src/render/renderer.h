// Draws a notebook's laid-out pages onto a host surface with Skia.
//
// Committed content (paper, ruling, images, elements) is kept in an offscreen
// surface at the screen's pixel size. A document edit redraws only the
// regions it changed there; a view change redraws all of it. Each frame then
// copies that surface to the screen and draws the live stroke over it. This
// is Write's split between the cached page image, redrawn under a dirty
// rectangle, and the in-progress stroke drawn over it
// (syncscribble/scribblearea.cpp:2580-2640 ScribbleArea::drawImage and
// drawScreen, styluslabs/Write 401b65d).
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "document/document.h"
#include "include/core/SkData.h"
#include "include/core/SkImage.h"
#include "include/core/SkPath.h"
#include "include/core/SkRegion.h"
#include "include/core/SkSurface.h"
#include "layout/layout.h"

class GrDirectContext;
class SkCanvas;

namespace ink_engine {

// Behind the scroll view's pages: the web UI's --desk token, a light gray as
// in Noteful (docs/specs/tablet-ui.md, "Visual style").
inline constexpr uint32_t kDeskColor = 0xFFE9EBEF;

// Counts of the work done, for tests and the frame-time check.
struct RenderStats {
  uint64_t frames = 0;           // screen frames drawn
  uint64_t full_redraws = 0;     // content surface redrawn whole
  uint64_t partial_redraws = 0;  // content surface redrawn under dirty regions
  uint64_t elements_drawn = 0;   // committed elements drawn into the content surface
  uint64_t redrawn_pixels = 0;   // area of the content surface redrawn
};

// The stroke being drawn: its page and its outline in page coordinates.
struct LiveInk {
  size_t page = 0;
  std::vector<Polyline> outline;
  Rgb color;
};

// A notebook's image files by path relative to the notebook, e.g.
// "assets/p0017.png".
using Assets = std::map<std::string, sk_sp<SkData>>;

// What one frame shows.
struct View {
  Transform content_to_view;  // SVG matrix order; content = layout coordinates
  float pixel_ratio = 1;      // device pixels per view unit
  int width = 0, height = 0;  // device pixels
  bool operator==(const View &) const = default;
};

class Renderer {
 public:
  // `context` is the GPU context of the screen surfaces; null for raster.
  // Images come from `assets`, each decoded once, when first drawn.
  Renderer(GrDirectContext *context, const Assets &assets)
      : context_(context), assets_(&assets) {}

  // Brings the content surface up to date with `document` in `view`.
  // `live_changed` says the live stroke's geometry changed. Returns whether
  // the screen needs a new frame.
  bool Update(const Document &document, const View &view, bool live_changed);

  // Draws the content surface and then the live stroke onto `screen`.
  void Draw(SkCanvas *screen, const LiveInk *live);

  // Redraws everything on the next Update: the assets changed.
  void Invalidate() { invalidated_ = true; }

  const RenderStats &stats() const { return stats_; }

 private:
  struct CachedElement {
    immer::box<Element> element;  // holds the key's object alive
    SkPath path;                  // local coordinates; strokes and shapes
    SkRect bounds;                // page coordinates, with the element's transform
  };

  const CachedElement &Cached(const immer::box<Element> &box);
  sk_sp<SkImage> Asset(const std::string &page_file, const std::string &href);
  SkMatrix ContentMatrix() const;
  void Redraw(const SkRegion &region);
  void DrawPage(SkCanvas *canvas, const Page &page, const SkRect &cull);
  void DrawElements(SkCanvas *canvas, const Page &page, const Elements &elements,
                    const SkRect &cull);
  void DrawImage(SkCanvas *canvas, const Page &page, const Image &image);
  void ElementBounds(const Page &page, const Elements &elements, SkRect *bounds);
  SkRegion DirtyRegion(const Document &next);

  struct DecodedAsset {
    sk_sp<SkData> bytes;  // the asset it was decoded from
    sk_sp<SkImage> image;  // null when undecodable
  };

  GrDirectContext *context_;
  const Assets *assets_;
  std::map<std::string, DecodedAsset> images_;
  std::map<const Element *, CachedElement> elements_;
  sk_sp<SkSurface> content_;
  std::optional<Document> document_;
  std::vector<PagePlacement> layout_;
  View view_;
  bool screen_stale_ = true;
  bool invalidated_ = false;
  RenderStats stats_;
};

}  // namespace ink_engine
