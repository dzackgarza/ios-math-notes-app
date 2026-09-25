#include "strokes/outline.h"

#include "include/core/SkPathBuilder.h"

namespace ink_engine {

std::vector<Polyline> StrokeOutline(const ink::PartitionedMesh &shape) {
  std::vector<Polyline> outline;
  for (uint32_t group = 0; group < shape.RenderGroupCount(); ++group) {
    for (uint32_t i = 0; i < shape.OutlineCount(group); ++i) {
      Polyline &line = outline.emplace_back();
      for (const auto &index : shape.Outline(group, i)) {
        ink::Point p = shape.RenderGroupMeshes(group)[index.mesh_index].VertexPosition(
            index.vertex_index);
        line.push_back({p.x, p.y});
      }
    }
  }
  return outline;
}

std::vector<Polyline> LiveOutline(const ink::InProgressStroke &stroke) {
  std::vector<Polyline> outline;
  for (uint32_t coat = 0; coat < stroke.BrushCoatCount(); ++coat) {
    const ink::MutableMesh &mesh = stroke.GetMesh(coat);
    for (const auto &indices : stroke.GetCoatOutlines(coat)) {
      Polyline &line = outline.emplace_back();
      for (uint32_t index : indices) {
        ink::Point p = mesh.VertexPosition(index);
        line.push_back({p.x, p.y});
      }
    }
  }
  return outline;
}

SkPath OutlinePath(const std::vector<Polyline> &outline) {
  SkPathBuilder builder(SkPathFillType::kWinding);
  for (const Polyline &line : outline) {
    if (line.empty()) continue;
    builder.moveTo(float(line[0].x), float(line[0].y));
    for (size_t i = 1; i < line.size(); ++i) builder.lineTo(float(line[i].x), float(line[i].y));
    builder.close();
  }
  return builder.detach();
}

}  // namespace ink_engine
