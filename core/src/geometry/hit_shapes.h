// Hit-test shapes for the eraser and the lasso, built on google/ink geometry.
#pragma once

#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "ink/geometry/partitioned_mesh.h"
#include "ink/geometry/point.h"
#include "ink/geometry/quad.h"
#include "ink/geometry/segment.h"

namespace ink_engine {

// The area an eraser sweeps along `segment`: the segment widened by `padding`
// on every side. Follows the Jetpack Ink geometry guide
// (developer.android.com/develop/ui/views/touch-and-input/stylus-input/ink-api-geometry-apis),
// with google/ink Quad::FromCenterDimensionsAndRotation (ink/geometry/quad.h:97-110).
ink::Quad EraserQuad(const ink::Segment &segment, float padding);

// A closed lasso mesh from the lasso's points. Ports google/ink
// ink/strokes/internal/jni/mesh_creation_native.cc:56-110
// (MeshCreationNative_createClosedShapeFromStrokeInputBatch) at 1b220eee.
absl::StatusOr<ink::PartitionedMesh> LassoMesh(absl::Span<const ink::Point> points);

}  // namespace ink_engine
