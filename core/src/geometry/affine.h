// Affine maps as SVG matrix(a, b, c, d, e, f): x' = a x + c y + e,
// y' = b x + d y + f.
#pragma once

#include <cmath>

#include "document/document.h"

namespace ink_engine {

inline Point Apply(const Transform &m, Point p) {
  return {m.a * p.x + m.c * p.y + m.e, m.b * p.x + m.d * p.y + m.f};
}

// The map that applies `first`, then `then`.
inline Transform Compose(const Transform &then, const Transform &first) {
  return {then.a * first.a + then.c * first.b,
          then.b * first.a + then.d * first.b,
          then.a * first.c + then.c * first.d,
          then.b * first.c + then.d * first.d,
          then.a * first.e + then.c * first.f + then.e,
          then.b * first.e + then.d * first.f + then.f};
}

inline Transform Inverse(const Transform &m) {
  double det = m.a * m.d - m.b * m.c;
  return {m.d / det,
          -m.b / det,
          -m.c / det,
          m.a / det,
          (m.c * m.f - m.d * m.e) / det,
          (m.b * m.e - m.a * m.f) / det};
}

inline Transform Translation(double dx, double dy) { return {1, 0, 0, 1, dx, dy}; }

// Write Selection::scale (syncscribble/selection.cpp:245-251, styluslabs/Write
// 401b65d): Transform2D(sx, 0, 0, sy, (1 - sx) origin.x, (1 - sy) origin.y).
inline Transform ScaleAbout(double sx, double sy, Point origin) {
  return {sx, 0, 0, sy, (1 - sx) * origin.x, (1 - sy) * origin.y};
}

// A rotation by `radians` (y down: clockwise on screen) about `origin`.
inline Transform RotateAbout(double radians, Point origin) {
  double c = std::cos(radians), s = std::sin(radians);
  return Compose(Translation(origin.x, origin.y),
                 Compose(Transform{c, s, -s, c, 0, 0}, Translation(-origin.x, -origin.y)));
}

}  // namespace ink_engine
