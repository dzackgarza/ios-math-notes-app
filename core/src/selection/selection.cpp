#include "selection/selection.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <boost/geometry/algorithms/simplify.hpp>
#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

#include "editor/erase.h"
#include "format/page_svg.h"
#include "geometry/affine.h"
#include "include/core/SkMatrix.h"
#include "ink/geometry/affine_transform.h"
#include "ink/geometry/mesh.h"
#include "ink/geometry/mesh_format.h"
#include "strokes/outline.h"

namespace ink_engine {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

Rect Empty() { return {kInf, kInf, -kInf, -kInf}; }

void Add(Rect &r, Point p) {
  r.left = std::min(r.left, p.x), r.right = std::max(r.right, p.x);
  r.top = std::min(r.top, p.y), r.bottom = std::max(r.bottom, p.y);
}

ink::AffineTransform ToInk(const Transform &m) {
  return ink::AffineTransform(float(m.a), float(m.c), float(m.e), float(m.b), float(m.d), float(m.f));
}

// The element's hit-test meshes in its local coordinates.
std::vector<ink::PartitionedMesh> HitMeshes(const Element &element) {
  std::vector<ink::PartitionedMesh> meshes;
  if (const auto *stroke = std::get_if<Stroke>(&element.value)) {
    meshes.push_back(InkStroke(*stroke).GetShape());
  } else if (const auto *shape = std::get_if<Shape>(&element.value)) {
    for (const Stroke &s : ShapeStrokes(*shape, "")) meshes.push_back(InkStroke(s).GetShape());
  } else if (const auto *image = std::get_if<Image>(&element.value)) {
    float l = float(image->x), t = float(image->y);
    float r = float(image->x + image->width), b = float(image->y + image->height);
    absl::StatusOr<ink::Mesh> mesh =
        ink::Mesh::Create(ink::MeshFormat(), {{l, r, r, l}, {t, t, b, b}}, {0, 1, 2, 0, 2, 3});
    if (mesh.ok()) {
      absl::StatusOr<ink::PartitionedMesh> partitioned =
          ink::PartitionedMesh::FromMeshes(absl::MakeSpan(&*mesh, 1));
      if (partitioned.ok()) meshes.push_back(*std::move(partitioned));
    }
  }
  return meshes;
}

const Transform &LocalTransform(const Element &element) {
  static const Transform kIdentity;
  if (const auto *s = std::get_if<Stroke>(&element.value)) return s->transform;
  if (const auto *s = std::get_if<Shape>(&element.value)) return s->transform;
  if (const auto *i = std::get_if<Image>(&element.value)) return i->transform;
  return kIdentity;
}

const Elements *Children(const Element &element) {
  if (const auto *b = std::get_if<Bookmark>(&element.value)) return &b->children;
  if (const auto *l = std::get_if<Link>(&element.value)) return &l->children;
  return nullptr;
}

template <class Visit>
Element MapChildren(const Element &element, Visit &&visit) {
  Element copy = element;
  auto remap = [&](Elements &children) {
    Elements mapped;
    for (const auto &child : children) mapped = std::move(mapped).push_back(immer::box<Element>(visit(*child)));
    children = std::move(mapped);
  };
  if (auto *b = std::get_if<Bookmark>(&copy.value)) remap(b->children);
  if (auto *l = std::get_if<Link>(&copy.value)) remap(l->children);
  return copy;
}

}  // namespace

// Write LassoSelector::addPoint (selection.cpp:1105-1154): each new point
// closes the path again; the chunk from simplify_start_ to the end is
// simplified, and when that removed points the next chunk starts at the
// second-to-last point. Ramer-Douglas-Peucker is Boost.Geometry's simplify
// (Write ulib/geom.h:191-210 simplifyRDP).
void LassoPath::Add(Point p, double simplify) {
  namespace bg = boost::geometry;
  using BgPoint = bg::model::d2::point_xy<double>;
  points_.push_back(p);
  if (points_.size() < 3) return;
  bg::model::linestring<BgPoint> chunk, simplified;
  for (size_t i = simplify_start_; i < points_.size(); ++i) chunk.push_back({points_[i].x, points_[i].y});
  bg::simplify(chunk, simplified, simplify);
  if (simplified.size() <= 2) return;
  points_.resize(simplify_start_);
  for (const BgPoint &q : simplified) points_.push_back({q.x(), q.y()});
  simplify_start_ = points_.size() - 2;
}

Rect Union(const Rect &a, const Rect &b) {
  return {std::min(a.left, b.left), std::min(a.top, b.top), std::max(a.right, b.right),
          std::max(a.bottom, b.bottom)};
}

bool IsEmpty(const Rect &r) { return r.left > r.right || r.top > r.bottom; }

Rect ElementBounds(const Element &element) {
  Rect bounds = Empty();
  if (const Elements *children = Children(element)) {
    for (const auto &child : *children) bounds = Union(bounds, ElementBounds(*child));
    return bounds;
  }
  const Transform &m = LocalTransform(element);
  if (const auto *stroke = std::get_if<Stroke>(&element.value)) {
    for (const Polyline &line : stroke->outline) {
      for (Point p : line) Add(bounds, Apply(m, p));
    }
  } else if (const auto *shape = std::get_if<Shape>(&element.value)) {
    SkMatrix to_page = SkMatrix::MakeAll(float(m.a), float(m.c), float(m.e), float(m.b),
                                         float(m.d), float(m.f), 0, 0, 1);
    SkRect box = ShapePath(*shape).makeTransform(to_page).computeTightBounds();
    double half = shape->stroke_width / 2 * std::sqrt(std::abs(m.a * m.d - m.b * m.c));
    bounds = {box.left() - half, box.top() - half, box.right() + half, box.bottom() + half};
  } else if (const auto *image = std::get_if<Image>(&element.value)) {
    for (Point p : {Point{image->x, image->y}, Point{image->x + image->width, image->y},
                    Point{image->x, image->y + image->height},
                    Point{image->x + image->width, image->y + image->height}}) {
      Add(bounds, Apply(m, p));
    }
  }
  return bounds;
}

bool InsideRect(const Element &element, const Rect &rect) {
  Rect b = ElementBounds(element);
  return !IsEmpty(b) && b.left >= rect.left && b.right <= rect.right && b.top >= rect.top &&
         b.bottom <= rect.bottom;
}

bool LassoSelects(const ink::PartitionedMesh &lasso, const Element &element) {
  if (const Elements *children = Children(element)) {
    return !children->empty() && std::all_of(children->begin(), children->end(), [&](const auto &c) {
      return LassoSelects(lasso, *c);
    });
  }
  std::vector<ink::PartitionedMesh> meshes = HitMeshes(element);
  if (meshes.empty()) return false;
  ink::AffineTransform lasso_to_local = ToInk(Inverse(LocalTransform(element)));
  return std::all_of(meshes.begin(), meshes.end(), [&](const ink::PartitionedMesh &mesh) {
    return mesh.CoverageIsGreaterThan(lasso, kLassoCoverage, lasso_to_local);
  });
}

// Write RectSelector::shrink (selection.cpp:727-741).
Rect SelectionRect(const Rect &bounds, double scale) {
  double padx = (4 * kHandlePad / scale - (bounds.right - bounds.left)) / 2;
  double pady = (4 * kHandlePad / scale - (bounds.bottom - bounds.top)) / 2;
  padx = std::max(kSelectionMinPad, padx + kSelectionMinPad);
  pady = std::max(kSelectionMinPad, pady + kSelectionMinPad);
  return {bounds.left - padx, bounds.top - pady, bounds.right + padx, bounds.bottom + pady};
}

Point RotateHandle(const Rect &rect, double scale) {
  return {(rect.left + rect.right) / 2, rect.top - 6 * kHandleSize / scale};
}

// Write ScribbleArea::selectionHit (scribblearea.cpp:1345-1377) with
// RectSelector::scaleHandleHit and rotHandleHit (selection.cpp:748-778).
HandleHit HitSelection(const Rect &rect, Point p, double scale, bool touch) {
  double h = touch ? 2 * kHandleSize : kHandleSize;
  double a = (h + 3) / scale;
  auto near = [&](Point c) { return p.x >= c.x - a && p.x <= c.x + a && p.y >= c.y - a && p.y <= c.y + a; };
  const Point corners[4][2] = {
      {{rect.left, rect.top}, {rect.right, rect.bottom}},
      {{rect.right, rect.top}, {rect.left, rect.bottom}},
      {{rect.left, rect.bottom}, {rect.right, rect.top}},
      {{rect.right, rect.bottom}, {rect.left, rect.top}},
  };
  for (const auto &[corner, opposite] : corners) {
    if (!near(corner)) continue;
    // The bottom-right corner scales with a fixed aspect ratio; the others freely.
    return {HandleKind::kScale, opposite, opposite.x < p.x && opposite.y < p.y};
  }
  if (near(RotateHandle(rect, scale))) {
    return {HandleKind::kRotate, {(rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2}};
  }
  if (p.x >= rect.left && p.x <= rect.right && p.y >= rect.top && p.y <= rect.bottom) {
    return {HandleKind::kMove};
  }
  return {};
}

Element Transformed(const Element &element, const Transform &m) {
  if (Children(element)) return MapChildren(element, [&](const Element &c) { return Transformed(c, m); });
  Element copy = element;
  std::visit(
      [&](auto &e) {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, Stroke> || std::is_same_v<T, Shape> || std::is_same_v<T, Image>) {
          e.transform = Compose(m, e.transform);
        }
      },
      copy.value);
  return copy;
}

namespace {

// The id field of an element that has one; a link has none.
template <class E>
auto IdOf(E &element) -> decltype(&std::get<Stroke>(element.value).id) {
  using Id = decltype(&std::get<Stroke>(element.value).id);
  return std::visit(
      [](auto &e) -> Id {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, Link>) {
          return nullptr;
        } else {
          return &e.id;
        }
      },
      element.value);
}

std::string NewId(const Element &element, IdGenerator &ids) {
  return std::holds_alternative<Bookmark>(element.value) ? ids.BookmarkId() : ids.StrokeId();
}

}  // namespace

Element WithNewIds(const Element &element, IdGenerator &ids) {
  Element copy = MapChildren(element, [&](const Element &c) { return WithNewIds(c, ids); });
  if (std::string *id = IdOf(copy)) *id = NewId(copy, ids);
  return copy;
}

Element WithFreeIds(const Element &element, const std::vector<std::string> &taken, IdGenerator &ids) {
  Element copy = MapChildren(element, [&](const Element &c) { return WithFreeIds(c, taken, ids); });
  std::string *id = IdOf(copy);
  if (id && std::find(taken.begin(), taken.end(), *id) != taken.end()) *id = NewId(copy, ids);
  return copy;
}

void CollectIds(const Elements &elements, std::vector<std::string> &ids) {
  for (const auto &box : elements) {
    if (const std::string *id = IdOf(*box)) ids.push_back(*id);
    if (const Elements *children = Children(*box)) CollectIds(*children, ids);
  }
}

std::string ClipboardSvg(const Elements &elements, const std::string &layer_id) {
  Rect bounds = Empty();
  for (const auto &box : elements) bounds = Union(bounds, ElementBounds(*box));
  Page page{.id = "clipboard",
            .width = std::max(1.0, std::ceil(bounds.right)),
            .height = std::max(1.0, std::ceil(bounds.bottom)),
            .layers = {LayerContent{layer_id, elements}}};
  return WritePage(page);
}

std::optional<Elements> ReadClipboard(std::string_view svg) {
  Page page = ReadPage(svg, "clipboard.svg", {});
  if (page.error) return std::nullopt;
  Elements elements;
  for (const LayerContent &layer : page.layers) elements = elements + layer.elements;
  return elements;
}

}  // namespace ink_engine
