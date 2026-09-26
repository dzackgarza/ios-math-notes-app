// Test-only readers of the Stylus Labs Write fixtures in tests/fixtures/write
// (their README.md gives the trace grammar, expected.json, and units), and a
// replay of a trace's pen input through the C ABI (#1, "How the engine
// replays a fixture").
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "document/document.h"
#include "ink.h"

namespace ink_test {

// Write units are 1/150 inch: points = 0.48 × units.
inline constexpr double kWritePt = 0.48;

struct WriteEvent {
  enum Kind { kInput, kMode } kind = kInput;
  double x = 0, y = 0, pressure = 0, time = 0;  // ie: page units, ms
  int ev = 0;                                   // ie: 1 press, 0 move, -1 release
  int mode = 0;                                 // mode: scribblemode.h number
};

// The `ie` and `mode` lines of trace.txt. `view 0 0 0` and `screen` only set
// up Write's view; any other command is an error, since replaying it is the
// job of a later unit.
std::vector<WriteEvent> ReadWriteTrace(const std::string &path);

struct WriteElement {
  int id = 0;
  std::vector<std::vector<ink_engine::Point>> pen_points;  // pt, page coordinates
};

struct WriteExpected {
  std::vector<WriteElement> elements;  // page 0, document order
  std::set<int> deleted;
};

// expected.json, pen points converted to points.
WriteExpected ReadWriteExpected(const std::string &path);

// The ink paths of input.html's first page (not its ruling), each as a
// polyline in points: pugixml for the document, SkParsePath for `d`. The
// page group's translate(0.5, 0.5) is Write's pixel alignment; Write reads
// the paths without it.
std::vector<std::vector<ink_engine::Point>> ReadWriteDocument(const std::string &path);

// Strokes along polylines in points, as the engine stores them: marker brush
// 0.48 pt wide (Write's pen width 1), samples 10 ms apart, as Write's traces
// draw them.
ink_engine::Document WithStrokes(ink_engine::Document document,
                                 const std::vector<std::vector<ink_engine::Point>> &lines);

// Replays a trace on page 0 at zoom 1: view units are Write units. Modes 14
// and 16 turn on the stroke and free eraser for the next gesture only (Write
// modes other than 12 last one gesture). Returns the ids of the strokes each
// drawing gesture committed, in order.
std::vector<std::string> ReplayWriteTrace(InkCanvas *canvas, const std::vector<WriteEvent> &trace);

}  // namespace ink_test
