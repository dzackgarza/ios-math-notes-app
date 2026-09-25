// Liang–Barsky segment clipping and the linestring-to-pieces loop, ported
// from Boost.Geometry boost/geometry/algorithms/detail/overlay/clip_linestring.hpp
// (liang_barsky::check_edge, clip_segment; clip_linestring_with_box), with
// the parameters kept to interpolate the samples' other channels.
#include "strokes/clip.h"

#include <cmath>
#include <numbers>

namespace ink_engine {
namespace {

// Narrows [t1, t2] by one boundary: p·t <= q.
bool CheckEdge(double p, double q, double &t1, double &t2) {
  if (p < 0) {
    double r = q / p;
    if (r > t2) return false;
    if (r > t1) t1 = r;
  } else if (p > 0) {
    double r = q / p;
    if (r < t1) return false;
    if (r < t2) t2 = r;
  } else if (q < 0) {
    return false;
  }
  return true;
}

// The parameter interval of segment a-b inside `rect`, if any.
bool ClipSegment(const InkPenSample &a, const InkPenSample &b, const Rect &rect, double &t1,
                 double &t2) {
  t1 = 0;
  t2 = 1;
  double dx = b.x - a.x, dy = b.y - a.y;
  return CheckEdge(-dx, a.x - rect.left, t1, t2) && CheckEdge(dx, rect.right - a.x, t1, t2) &&
         CheckEdge(-dy, a.y - rect.top, t1, t2) && CheckEdge(dy, rect.bottom - a.y, t1, t2);
}

float Lerp(float a, float b, double t) { return float(a + (b - a) * t); }

float LerpAngle(float a, float b, double t) {
  double d = std::remainder(double(b) - a, 2 * std::numbers::pi);
  return float(a + d * t);
}

}  // namespace

InkPenSample Interpolate(const InkPenSample &a, const InkPenSample &b, double t) {
  InkPenSample s = a;
  s.x = a.x + (b.x - a.x) * t;
  s.y = a.y + (b.y - a.y) * t;
  s.time = a.time + (b.time - a.time) * t;
  s.pressure = Lerp(a.pressure, b.pressure, t);
  s.altitude = Lerp(a.altitude, b.altitude, t);
  s.azimuth = LerpAngle(a.azimuth, b.azimuth, t);
  s.roll = LerpAngle(a.roll, b.roll, t);
  s.hover_height = Lerp(a.hover_height, b.hover_height, t);
  s.id = kInterpolatedSampleId;
  return s;
}

std::vector<std::vector<InkPenSample>> PiecesInside(const std::vector<InkPenSample> &samples,
                                                    const Rect &rect) {
  std::vector<std::vector<InkPenSample>> pieces;
  if (samples.size() == 1) {
    const InkPenSample &s = samples[0];
    if (s.x >= rect.left && s.x <= rect.right && s.y >= rect.top && s.y <= rect.bottom) {
      pieces.push_back({s});
    }
    return pieces;
  }
  std::vector<InkPenSample> piece;
  for (size_t i = 0; i + 1 < samples.size(); ++i) {
    const InkPenSample &a = samples[i], &b = samples[i + 1];
    double t1 = 0, t2 = 1;
    if (!ClipSegment(a, b, rect, t1, t2)) {
      if (!piece.empty()) pieces.push_back(std::move(piece));
      piece.clear();
      continue;
    }
    if (piece.empty()) piece.push_back(t1 > 0 ? Interpolate(a, b, t1) : a);
    if (t2 < 1) {
      piece.push_back(Interpolate(a, b, t2));
      pieces.push_back(std::move(piece));
      piece.clear();
    } else {
      piece.push_back(b);
    }
  }
  if (!piece.empty()) pieces.push_back(std::move(piece));
  return pieces;
}

}  // namespace ink_engine
