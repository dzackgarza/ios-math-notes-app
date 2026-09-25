#include "geometry/hit_shapes.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "ink/geometry/internal/polyline_processing.h"
#include "ink/geometry/mesh.h"
#include "ink/geometry/mesh_format.h"
#include "ink/geometry/tessellator.h"

namespace ink_engine {
namespace {

// google/ink mesh_creation_native.cc CalculateSlope.
float Slope(ink::Point p1, ink::Point p2) {
  if (p2.x == p1.x) return std::numeric_limits<float>::infinity();
  return (p2.y - p1.y) / (p2.x - p1.x);
}

// A zero-area single-triangle mesh; it still hit-tests by intersection.
absl::StatusOr<ink::Mesh> DegenerateTriangle(std::vector<float> xs, std::vector<float> ys) {
  return ink::Mesh::Create(ink::MeshFormat(), {xs, ys}, {0, 1, 2});
}

}  // namespace

ink::Quad EraserQuad(const ink::Segment &segment, float padding) {
  return ink::Quad::FromCenterDimensionsAndRotation(
      segment.Midpoint(), segment.Length() + 2 * padding, 2 * padding,
      segment.Vector().Direction());
}

absl::StatusOr<ink::PartitionedMesh> LassoMesh(absl::Span<const ink::Point> points) {
  if (points.empty()) return ink::PartitionedMesh();

  std::vector<ink::Point> closed = ink::geometry_internal::CreateClosedShape(points);
  absl::StatusOr<ink::Mesh> mesh;
  if (closed.size() < 3) {
    // Fewer than 3 points: repeat points into one point- or segment-like triangle.
    std::vector<float> xs, ys;
    for (size_t i = 0; i < 3; ++i) {
      xs.push_back(closed[i % closed.size()].x);
      ys.push_back(closed[i % closed.size()].y);
    }
    mesh = DegenerateTriangle(xs, ys);
  } else {
    mesh = ink::CreateMeshFromPolyline(closed);
  }

  if (!mesh.ok() && closed.size() >= 2) {
    // The tessellator rejects collinear points; represent them as a segment.
    float min_x = std::min(closed[0].x, closed[1].x);
    float max_x = std::max(closed[0].x, closed[1].x);
    float min_y = std::min(closed[0].y, closed[1].y);
    float max_y = std::max(closed[0].y, closed[1].y);
    float slope = Slope(closed[0], closed[1]);
    bool collinear = true;
    for (size_t i = 2; i < closed.size(); ++i) {
      if (slope != Slope(closed[i - 1], closed[i])) {
        collinear = false;
        break;
      }
      min_x = std::min(min_x, closed[i].x);
      max_x = std::max(max_x, closed[i].x);
      min_y = std::min(min_y, closed[i].y);
      max_y = std::max(max_y, closed[i].y);
    }
    if (collinear) {
      mesh = DegenerateTriangle({min_x, min_x, max_x},
                                slope < 0 ? std::vector<float>{max_y, max_y, min_y}
                                          : std::vector<float>{min_y, min_y, max_y});
    }
  }
  if (!mesh.ok()) return mesh.status();
  return ink::PartitionedMesh::FromMeshes(absl::MakeSpan(&*mesh, 1));
}

}  // namespace ink_engine
