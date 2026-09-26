// The stroke outline walk. It gives the polylines that both the page file's
// `d` (format/page_svg) and the renderer's SkPath are built from: one closed
// subpath per outline, nonzero fill. Follows google/ink
// ink/rendering/skia/native/internal/path_drawable.cc:42-91 (1b220eee).
#pragma once

#include <vector>

#include "document/document.h"
#include "include/core/SkPath.h"
#include "ink/geometry/partitioned_mesh.h"
#include "ink/strokes/in_progress_stroke.h"

namespace ink_engine {

// Outlines of a finished stroke's shape, every render group in order.
std::vector<Polyline> StrokeOutline(const ink::PartitionedMesh &shape);

// Outlines of a stroke being drawn, every brush coat in order.
std::vector<Polyline> LiveOutline(const ink::InProgressStroke &stroke);

SkPath OutlinePath(const std::vector<Polyline> &outline);

// Open polylines: ruling lines and arrow shapes.
SkPath OpenPath(const std::vector<Polyline> &polylines);

// A shape element's geometry in its local coordinates, as SVG draws it.
SkPath ShapePath(const Shape &shape);

}  // namespace ink_engine
