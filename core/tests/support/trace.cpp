#include "trace.h"

#include <cstdio>
#include <fstream>
#include <stdexcept>

#include "ink/brush/stock_brushes.h"
#include "ink/color/color.h"
#include "ink/geometry/angle.h"
#include "ink/geometry/partitioned_mesh.h"
#include "ink/types/duration.h"
#include "ink/types/physical_distance.h"

namespace ink_test {
namespace {

void Require(bool ok, const std::string &what) {
  if (!ok) throw std::runtime_error(what);
}

void AppendInput(ink::StrokeInputBatch &batch, const ink::StrokeInput &input) {
  auto status = batch.Append(input);
  Require(status.ok(), std::string(status.message()));
}

void WriteBatch(std::FILE *out, size_t frame, char kind, const ink::StrokeInputBatch &batch) {
  for (ink::StrokeInput in : batch) {
    // %.9g round-trips every float exactly.
    std::fprintf(out, "%zu %c %d %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n", frame, kind,
                 static_cast<int>(in.tool_type), in.position.x, in.position.y,
                 in.elapsed_time.ToSeconds(), in.stroke_unit_length.ToCentimeters(),
                 in.pressure, in.tilt.ValueInRadians(), in.orientation.ValueInRadians());
  }
}

ink::Brush MakeBrush(const ink::BrushFamily &family) {
  auto brush = ink::Brush::Create(family, ink::Color::Black(), /*size=*/5, /*epsilon=*/0.1);
  Require(brush.ok(), std::string(brush.status().message()));
  return *brush;
}

}  // namespace

std::vector<NamedBrush> StockTestBrushes() {
  using namespace ink::stock_brushes;
  return {
      {"marker", MakeBrush(Marker(MarkerVersion::kV1))},
      {"pressure_pen", MakeBrush(PressurePen(PressurePenVersion::kV1))},
      {"highlighter",
       MakeBrush(Highlighter(ink::BrushPaint::SelfOverlap::kDiscard, HighlighterVersion::kV1))},
  };
}

std::vector<TraceFrame> ReadTrace(const std::string &path) {
  std::ifstream in(path);
  Require(in.is_open(), "cannot open " + path);
  std::vector<TraceFrame> frames;
  size_t frame;
  char kind;
  int tool;
  float x, y, seconds, unit_cm, pressure, tilt, orientation;
  while (in >> frame >> kind >> tool >> x >> y >> seconds >> unit_cm >> pressure >> tilt >>
         orientation) {
    if (frame >= frames.size()) frames.resize(frame + 1);
    ink::StrokeInput input{
        .tool_type = static_cast<ink::StrokeInput::ToolType>(tool),
        .position = {x, y},
        .elapsed_time = ink::Duration32::Seconds(seconds),
        .stroke_unit_length = ink::PhysicalDistance::Centimeters(unit_cm),
        .pressure = pressure,
        .tilt = ink::Angle::Radians(tilt),
        .orientation = ink::Angle::Radians(orientation),
    };
    AppendInput(kind == 'r' ? frames[frame].real : frames[frame].predicted, input);
  }
  Require(in.eof(), "malformed trace " + path);
  return frames;
}

void WriteTrace(const std::string &path, const std::vector<TraceFrame> &frames) {
  std::FILE *out = std::fopen(path.c_str(), "w");
  Require(out != nullptr, "cannot write " + path);
  for (size_t i = 0; i < frames.size(); ++i) {
    WriteBatch(out, i, 'r', frames[i].real);
    WriteBatch(out, i, 'p', frames[i].predicted);
  }
  std::fclose(out);
}

ink::StrokeInputBatch RealInputs(const std::vector<TraceFrame> &frames) {
  ink::StrokeInputBatch all;
  for (const TraceFrame &frame : frames) {
    auto status = all.Append(frame.real);
    Require(status.ok(), std::string(status.message()));
  }
  return all;
}

// Walks outlines as google/ink documents in partitioned_mesh.h
// (PartitionedMesh::Outlines, "Example usage").
std::vector<OutlineVertex> Outline(const ink::Stroke &stroke) {
  const ink::PartitionedMesh &shape = stroke.GetShape();
  std::vector<OutlineVertex> vertices;
  for (uint32_t group = 0; group < shape.RenderGroupCount(); ++group) {
    for (uint32_t outline = 0; outline < shape.OutlineCount(group); ++outline) {
      for (const auto &index : shape.Outline(group, outline)) {
        vertices.push_back({group, outline,
                            shape.RenderGroupMeshes(group)[index.mesh_index].VertexPosition(
                                index.vertex_index)});
      }
    }
  }
  return vertices;
}

std::vector<OutlineVertex> ReadOutline(const std::string &path) {
  std::ifstream in(path);
  Require(in.is_open(), "cannot open " + path);
  std::vector<OutlineVertex> vertices;
  OutlineVertex v;
  while (in >> v.group >> v.outline >> v.position.x >> v.position.y) vertices.push_back(v);
  Require(in.eof(), "malformed outline " + path);
  return vertices;
}

void WriteOutline(const std::string &path, const std::vector<OutlineVertex> &outline) {
  std::FILE *out = std::fopen(path.c_str(), "w");
  Require(out != nullptr, "cannot write " + path);
  for (const OutlineVertex &v : outline) {
    std::fprintf(out, "%u %u %.9g %.9g\n", v.group, v.outline, v.position.x, v.position.y);
  }
  std::fclose(out);
}

}  // namespace ink_test
