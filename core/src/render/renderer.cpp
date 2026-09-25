#include "render/renderer.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <unordered_set>

#include "include/codec/SkCodec.h"
#include "include/codec/SkPngDecoder.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "strokes/outline.h"

namespace ink_engine {
namespace {

SkMatrix ToSkMatrix(const Transform &t) {
  return SkMatrix::MakeAll(float(t.a), float(t.c), float(t.e), float(t.b), float(t.d), float(t.f),
                           0, 0, 1);
}

SkColor ToSkColor(Rgb rgb, double opacity = 1) {
  return SkColorSetARGB(uint8_t(std::lround(opacity * 255)), rgb.r, rgb.g, rgb.b);
}

// Open polylines: ruling lines and arrow shapes.
SkPath OpenPath(const std::vector<Polyline> &polylines) {
  SkPathBuilder builder;
  for (const Polyline &line : polylines) {
    if (line.empty()) continue;
    builder.moveTo(float(line[0].x), float(line[0].y));
    for (size_t i = 1; i < line.size(); ++i) builder.lineTo(float(line[i].x), float(line[i].y));
  }
  return builder.detach();
}

SkPath ShapePath(const Shape &shape) {
  auto point = [&](size_t i) {
    return i < shape.points.size() ? SkPoint::Make(float(shape.points[i].x), float(shape.points[i].y))
                                   : SkPoint::Make(0, 0);
  };
  SkPathBuilder builder;
  switch (shape.kind) {
    case ShapeKind::kLine:
      builder.moveTo(point(0)).lineTo(point(1));
      break;
    case ShapeKind::kPolygon:
      for (size_t i = 0; i < shape.points.size(); ++i) {
        i == 0 ? builder.moveTo(point(i)) : builder.lineTo(point(i));
      }
      builder.close();
      break;
    case ShapeKind::kRect:
      builder.addRect(SkRect::MakeXYWH(point(0).x(), point(0).y(), point(1).x(), point(1).y()));
      break;
    case ShapeKind::kEllipse: {
      SkPoint c = point(0), r = point(1);
      builder.addOval(SkRect::MakeLTRB(c.x() - r.x(), c.y() - r.y(), c.x() + r.x(), c.y() + r.y()));
      break;
    }
    case ShapeKind::kPath:
      return OpenPath(shape.path);
  }
  return builder.detach();
}

// SVG stroke defaults: butt caps, miter joins, miter limit 4.
SkPaint StrokePaint(Rgb color, double width) {
  SkPaint paint(SkColor4f::FromColor(ToSkColor(color)));
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(float(width));
  paint.setStrokeMiter(4);
  return paint;
}

// Draws a shape as Chromium draws the SVG element: rect and ellipse with
// their own canvas calls, whose thin-stroke rasterization differs from a
// path's; the rest as paths (third_party/blink/renderer/core/paint/
// svg_shape_painter.cc, SVGShapePainter::FillShape and StrokeShape).
void DrawShape(SkCanvas *canvas, const Shape &shape, const SkPath &path) {
  SkPaint paint = StrokePaint(shape.stroke, shape.stroke_width);
  SkRect rect;
  switch (shape.kind) {
    case ShapeKind::kRect:
      if (path.isRect(&rect)) return canvas->drawRect(rect, paint);
      break;
    case ShapeKind::kEllipse:
      if (path.isOval(&rect)) return canvas->drawOval(rect, paint);
      break;
    default:
      break;
  }
  canvas->drawPath(path, paint);
}

SkPaint FillPaint(Rgb color, double opacity = 1) {
  SkPaint paint(SkColor4f::FromColor(ToSkColor(color, opacity)));
  paint.setAntiAlias(true);
  return paint;
}

// A path relative to the page file, as a path relative to the notebook.
std::string NotebookPath(const std::string &page_file, const std::string &href) {
  namespace fs = std::filesystem;
  return (fs::path(page_file).parent_path() / href).lexically_normal().generic_string();
}

void CollectElements(const Elements &elements, std::unordered_set<const Element *> *out) {
  for (const auto &box : elements) {
    out->insert(&*box);
    if (auto *b = std::get_if<Bookmark>(&box->value)) CollectElements(b->children, out);
    if (auto *l = std::get_if<Link>(&box->value)) CollectElements(l->children, out);
  }
}

}  // namespace

void Renderer::SetAsset(const std::string &path, sk_sp<SkData> bytes) {
  asset_bytes_[path] = std::move(bytes);
  images_.erase(path);
}

sk_sp<SkImage> Renderer::Asset(const std::string &page_file, const std::string &href) {
  std::string path = NotebookPath(page_file, href);
  auto cached = images_.find(path);
  if (cached != images_.end()) return cached->second;
  sk_sp<SkImage> image;
  auto bytes = asset_bytes_.find(path);
  if (bytes != asset_bytes_.end()) {
    if (auto codec = SkPngDecoder::Decode(bytes->second, nullptr, nullptr)) {
      image = std::get<0>(codec->getImage());
    }
  }
  // A missing or undecodable file draws nothing, as in an SVG viewer.
  images_[path] = image;
  return image;
}

const Renderer::CachedElement &Renderer::Cached(const immer::box<Element> &box) {
  auto found = elements_.find(&*box);
  if (found != elements_.end()) return found->second;
  CachedElement entry{box, SkPath(), SkRect::MakeEmpty()};
  std::visit(
      [&](const auto &e) {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, Stroke>) {
          entry.path = OutlinePath(e.outline);
          entry.bounds = ToSkMatrix(e.transform).mapRect(entry.path.getBounds());
        } else if constexpr (std::is_same_v<T, Shape>) {
          entry.path = ShapePath(e);
          // A miter reaches at most miter limit × half the width past a vertex.
          float outset = float(e.stroke_width) * 2;
          entry.bounds =
              ToSkMatrix(e.transform).mapRect(entry.path.getBounds().makeOutset(outset, outset));
        } else if constexpr (std::is_same_v<T, Image>) {
          entry.bounds = ToSkMatrix(e.transform)
                             .mapRect(SkRect::MakeXYWH(float(e.x), float(e.y), float(e.width),
                                                       float(e.height)));
        } else {
          for (const auto &child : e.children) entry.bounds.join(Cached(child).bounds);
        }
      },
      box->value);
  return elements_.emplace(&*box, std::move(entry)).first->second;
}

SkMatrix Renderer::ContentMatrix() const {
  return SkMatrix::Scale(view_.pixel_ratio, view_.pixel_ratio) *
         ToSkMatrix(view_.content_to_view);
}

bool Renderer::Update(const Document &document, const View &view, bool live_changed) {
  std::vector<PagePlacement> layout = LayoutPages(document);
  bool document_changed = !document_ || !(document_->pages == document.pages) ||
                          !(document_->notebook == document.notebook);
  bool full = !content_ || !(view == view_) || layout != layout_ ||
              !(document_->notebook == document.notebook);
  SkRegion dirty;
  if (!full && document_changed) dirty = DirtyRegion(document);

  if (full && document_changed && document_) {
    std::unordered_set<const Element *> present;
    for (const auto &page : document.pages) {
      for (const LayerContent &layer : page->layers) CollectElements(layer.elements, &present);
    }
    std::erase_if(elements_, [&](const auto &entry) { return !present.contains(entry.first); });
  }

  if (!content_ || view.width != view_.width || view.height != view_.height) {
    SkImageInfo info = SkImageInfo::MakeN32Premul(view.width, view.height, SkColorSpace::MakeSRGB());
    content_ = context_ ? SkSurfaces::RenderTarget(context_, skgpu::Budgeted::kYes, info)
                        : SkSurfaces::Raster(info);
  }
  document_ = document;
  layout_ = std::move(layout);
  view_ = view;

  if (full) {
    Redraw(SkRegion(SkIRect::MakeWH(view.width, view.height)));
    ++stats_.full_redraws;
  } else if (!dirty.isEmpty()) {
    Redraw(dirty);
    ++stats_.partial_redraws;
  }
  if (live_changed) screen_stale_ = true;
  return screen_stale_;
}

// Device-space areas of the content surface that `next` changes: the bounds
// of every element added or removed, and whole pages whose paper changed.
SkRegion Renderer::DirtyRegion(const Document &next) {
  SkRegion region;
  SkMatrix content = ContentMatrix();
  for (const PagePlacement &placement : layout_) {
    const auto &before = document_->pages[placement.page];
    const auto &after = next.pages[placement.page];
    if (&*before == &*after) continue;
    SkMatrix to_device = content * SkMatrix::Translate(float(placement.x), float(placement.y));
    auto add = [&](const SkRect &page_rect) {
      // Antialiasing reaches one pixel past the geometry.
      region.op(to_device.mapRect(page_rect).roundOut().makeOutset(2, 2), SkRegion::kUnion_Op);
    };
    if (!(before->background == after->background) ||
        before->layers.size() != after->layers.size()) {
      add(SkRect::MakeWH(float(after->width), float(after->height)));
      continue;
    }
    for (size_t l = 0; l < after->layers.size(); ++l) {
      std::unordered_set<const Element *> old_set, new_set;
      for (const auto &box : before->layers[l].elements) old_set.insert(&*box);
      for (const auto &box : after->layers[l].elements) new_set.insert(&*box);
      for (const auto &box : after->layers[l].elements) {
        if (!old_set.contains(&*box)) add(Cached(box).bounds);
      }
      for (const auto &box : before->layers[l].elements) {
        if (new_set.contains(&*box)) continue;
        add(Cached(box).bounds);
        elements_.erase(&*box);
      }
      // Reordering alone changes what is on top.
      if (old_set == new_set && !(before->layers[l].elements == after->layers[l].elements)) {
        for (const auto &box : after->layers[l].elements) add(Cached(box).bounds);
      }
    }
  }
  region.op(SkIRect::MakeWH(view_.width, view_.height), SkRegion::kIntersect_Op);
  return region;
}

void Renderer::Redraw(const SkRegion &region) {
  SkCanvas *canvas = content_->getCanvas();
  canvas->save();
  canvas->clipRegion(region);
  canvas->clear(kDeskColor);
  SkMatrix content = ContentMatrix();
  SkMatrix inverse;
  if (content.invert(&inverse)) {
    SkRect clip_content = inverse.mapRect(SkRect::Make(region.getBounds()));
    for (const PagePlacement &placement : layout_) {
      SkRect page_rect = SkRect::MakeXYWH(float(placement.x), float(placement.y),
                                          float(placement.width), float(placement.height));
      if (!SkRect::Intersects(page_rect, clip_content)) continue;
      canvas->setMatrix(content * SkMatrix::Translate(page_rect.x(), page_rect.y()));
      DrawPage(canvas, *document_->pages[placement.page],
               clip_content.makeOffset(-page_rect.x(), -page_rect.y()));
    }
  }
  canvas->restore();
  for (SkRegion::Iterator it(region); !it.done(); it.next()) {
    stats_.redrawn_pixels += uint64_t(it.rect().width()) * uint64_t(it.rect().height());
  }
  screen_stale_ = true;
}

void Renderer::DrawPage(SkCanvas *canvas, const Page &page, const SkRect &cull) {
  const Background &bg = page.background;
  // g#background in file order: the paper, the imported page image, the ruling.
  canvas->drawRect(SkRect::MakeWH(float(page.width), float(page.height)), FillPaint(bg.fill));
  if (bg.image) DrawImage(canvas, page, *bg.image);
  for (const RulingPath &line : bg.lines) {
    canvas->drawPath(OpenPath(line.d), StrokePaint(line.stroke, line.stroke_width));
  }
  const std::vector<Layer> &layers = document_->notebook.layers;
  for (const LayerContent &content : page.layers) {
    auto layer = std::find_if(layers.begin(), layers.end(),
                              [&](const Layer &l) { return l.id == content.layer_id; });
    if (layer != layers.end() && layer->hidden) continue;
    DrawElements(canvas, page, content.elements, cull);
  }
}

void Renderer::DrawElements(SkCanvas *canvas, const Page &page, const Elements &elements,
                            const SkRect &cull) {
  for (const auto &box : elements) {
    const CachedElement &cached = Cached(box);
    if (!SkRect::Intersects(cached.bounds, cull)) continue;
    std::visit(
        [&](const auto &e) {
          using T = std::decay_t<decltype(e)>;
          if constexpr (std::is_same_v<T, Stroke>) {
            SkPaint paint = FillPaint(e.fill, e.fill_opacity.value_or(1));
            canvas->save();
            canvas->concat(ToSkMatrix(e.transform));
            canvas->drawPath(cached.path, paint);
            canvas->restore();
            ++stats_.elements_drawn;
          } else if constexpr (std::is_same_v<T, Shape>) {
            canvas->save();
            canvas->concat(ToSkMatrix(e.transform));
            DrawShape(canvas, e, cached.path);
            canvas->restore();
            ++stats_.elements_drawn;
          } else if constexpr (std::is_same_v<T, Image>) {
            DrawImage(canvas, page, e);
            ++stats_.elements_drawn;
          } else {
            DrawElements(canvas, page, e.children, cull);
          }
        },
        box->value);
  }
}

void Renderer::DrawImage(SkCanvas *canvas, const Page &page, const Image &image) {
  sk_sp<SkImage> decoded = Asset(page.file, image.href);
  if (!decoded) return;
  canvas->save();
  canvas->concat(ToSkMatrix(image.transform));
  // SVG preserveAspectRatio defaults to xMidYMid meet.
  SkRect box = SkRect::MakeXYWH(float(image.x), float(image.y), float(image.width),
                                float(image.height));
  float scale = std::min(box.width() / decoded->width(), box.height() / decoded->height());
  SkRect dst = SkRect::MakeXYWH(0, 0, decoded->width() * scale, decoded->height() * scale);
  dst.offset(box.centerX() - dst.centerX(), box.centerY() - dst.centerY());
  canvas->drawImageRect(decoded, dst, SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kLinear));
  canvas->restore();
}

void Renderer::Draw(SkCanvas *screen, const LiveInk *live) {
  screen->save();
  screen->resetMatrix();
  content_->draw(screen, 0, 0);
  if (live) {
    const PagePlacement *placement = nullptr;
    for (const PagePlacement &p : layout_) {
      if (p.page == live->page) placement = &p;
    }
    if (placement) {
      screen->setMatrix(ContentMatrix() *
                        SkMatrix::Translate(float(placement->x), float(placement->y)));
      screen->drawPath(OutlinePath(live->outline), FillPaint(live->color));
    }
  }
  screen->restore();
  screen_stale_ = false;
  ++stats_.frames;
}

}  // namespace ink_engine
