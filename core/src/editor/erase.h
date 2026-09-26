// Free erase on stored strokes: the parts of a stroke's sample path under the
// eraser, as a union of path-parameter intervals over the whole gesture, and
// the strokes that remain. Follows Xournal++ (b8b3a59)
// src/core/model/eraser/ErasableStroke.cpp:39-258 (ErasableStroke::erase,
// beginErasure, getStrokes: erased sections in a UnionOfIntervals of path
// parameters, complemented over the stroke) and src/core/model/Stroke.cpp:114-136
// (Stroke::cloneSection: a section keeps the inner samples and gets
// interpolated end samples). The eraser is a capsule around each move
// segment instead of Xournal++'s square box (#23).
#pragma once

#include <string>
#include <utility>
#include <vector>

#include <boost/icl/interval_set.hpp>

#include "document/document.h"
#include "ink.h"
#include "ink/strokes/stroke.h"

namespace ink_engine {

// Path parameters of a sample path: s = i + t is the point at t in [0, 1] along
// the segment from sample i to sample i + 1.
using ErasedSections = boost::icl::interval_set<double>;

// Adds to `erased` the parameters of `path` within `radius` of the segment p-q.
void EraseCapsule(const std::vector<Point> &path, Point p, Point q, double radius,
                  ErasedSections &erased);

// The parameter intervals of a path of `count` samples that `erased` leaves,
// in order. Intervals of zero length are dropped.
std::vector<std::pair<double, double>> RemainingSections(size_t count,
                                                         const ErasedSections &erased);

// The samples of the section [from, to]: the samples strictly inside, with a
// sample interpolated at each end (position, time and force linearly, pen
// angles along the shorter arc).
std::vector<Sample> SectionSamples(const std::vector<Sample> &samples, double from, double to);

// The positions of a stroke's samples through its transform, in page
// coordinates.
std::vector<Point> PagePath(const Stroke &stroke);

// The google/ink stroke a stored stroke describes: its brush (family, color,
// size) and its samples as inputs. A stroke value's outline is this stroke's.
ink::Stroke InkStroke(const Stroke &stroke);

// Recomputes a stroke value's outline from its brush and samples.
void RebuildOutline(Stroke &stroke);

// A shape element as pen strokes along its geometry, one per subpath: a
// marker at the shape's stroke width and color, with the shape's transform.
// Every stroke gets `time` as its start time.
std::vector<Stroke> ShapeStrokes(const Shape &shape, const std::string &time);

}  // namespace ink_engine
