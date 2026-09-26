// Cutting a stroke's input samples to a rectangle: the pieces of the sample
// path inside it, each ending at the edge with an interpolated sample. Pen-up
// keeps the parts of a stroke on its page; the eraser (#23) reuses the pieces.
#pragma once

#include <cstdint>
#include <vector>

#include "ink.h"

namespace ink_engine {

struct Rect {
  double left = 0, top = 0, right = 0, bottom = 0;
};

// Host id of a sample made by interpolation: no host update refers to it.
inline constexpr uint32_t kInterpolatedSampleId = UINT32_MAX;

// The parameter interval [t1, t2] of the segment a-b inside `rect` (edges
// included); false when the segment misses it.
bool ClipParameters(double ax, double ay, double bx, double by, const Rect &rect, double &t1,
                    double &t2);

// The sample at parameter t in [0, 1] from a to b: position, time, pressure
// and pen angles interpolated linearly (angles along the shorter arc); the
// other fields are a's.
InkPenSample Interpolate(const InkPenSample &a, const InkPenSample &b, double t);

// The maximal runs of the polyline through `samples` inside `rect` (edges
// included), in order. A run that enters or leaves the rectangle between two
// samples starts or ends with a sample interpolated at the edge. An empty
// result means the whole path is outside.
std::vector<std::vector<InkPenSample>> PiecesInside(const std::vector<InkPenSample> &samples,
                                                    const Rect &rect);

}  // namespace ink_engine
