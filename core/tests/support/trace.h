// Stroke input traces and stroke outlines as text files, shared by the engine
// tests and the Linux host tool that writes their fixtures and goldens.
//
// Trace file: one input per line,
//   frame kind tool x y seconds stroke_unit_cm pressure tilt_rad orientation_rad
// where kind is `r` (real) or `p` (predicted), and the frames are the batches
// passed to one InProgressStroke::EnqueueInputs call each.
//
// Outline file: one vertex per line, `group outline x y`.
#pragma once

#include <string>
#include <vector>

#include "ink/brush/brush.h"
#include "ink/geometry/point.h"
#include "ink/strokes/input/stroke_input_batch.h"
#include "ink/strokes/stroke.h"

namespace ink_test {

struct TraceFrame {
  ink::StrokeInputBatch real;
  ink::StrokeInputBatch predicted;
};

struct OutlineVertex {
  unsigned group;
  unsigned outline;
  ink::Point position;
};

struct NamedBrush {
  std::string name;
  ink::Brush brush;
};

// The traces and stock brushes that the outline goldens cover.
inline const std::vector<std::string> kTraceNames = {"straight_line", "spring_shape", "lissajous"};
std::vector<NamedBrush> StockTestBrushes();

std::vector<TraceFrame> ReadTrace(const std::string &path);
void WriteTrace(const std::string &path, const std::vector<TraceFrame> &frames);

// All real inputs of a trace, in order.
ink::StrokeInputBatch RealInputs(const std::vector<TraceFrame> &frames);

std::vector<OutlineVertex> Outline(const ink::Stroke &stroke);
std::vector<OutlineVertex> ReadOutline(const std::string &path);
void WriteOutline(const std::string &path, const std::vector<OutlineVertex> &outline);

}  // namespace ink_test
